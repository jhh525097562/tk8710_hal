#define _POSIX_C_SOURCE 200809L
#include "ns_scan_manager.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int publications, results, starts;
static char last[4096];

static int publish(void *context, const char *data, int qos)
{
    (void)context;
    assert(qos == 1);
    snprintf(last, sizeof(last), "%s", data);
    publications++;
    struct json_object *root = json_tokener_parse(data), *r;
    assert(root);
    if (json_object_object_get_ex(root, "scan_result", &r)) results++;
    json_object_put(root);
    return 0;
}

static void handle(ns_scan_manager *m, const char *data)
{
    struct json_object *root = json_tokener_parse(data);
    assert(root && ns_scan_manager_handle(m, root) == 1);
    json_object_put(root);
}

static void *serve(void *arg)
{
    int fd = accept(*(int *)arg, NULL, NULL);
    assert(fd >= 0);
    char command[256] = {0};
    size_t used = 0;
    while (!strchr(command, '\n') && used < sizeof(command) - 1) {
        ssize_t n = recv(fd, command + used, sizeof(command) - 1 - used, 0);
        assert(n > 0);
        used += n;
    }
    assert(strstr(command, "request_id=test1"));
    starts++;
    assert(send(fd, "OK\n", 3, 0) == 3);
    close(fd);
    return NULL;
}

static void fixture(const char *dir, const char *name, const char *data)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *fp = fopen(path, "w");
    assert(fp);
    fputs(data, fp);
    fclose(fp);
}

int main(void)
{
    char dir[] = "/tmp/mqtt_scan_manager_XXXXXX";
    assert(mkdtemp(dir));
    char socket_path[108];
    snprintf(socket_path, sizeof(socket_path), "%s/socket", dir);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(fd >= 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_path);
    assert(!bind(fd, (struct sockaddr *)&addr, sizeof(addr)));
    assert(!listen(fd, 1));
    pthread_t worker;
    assert(!pthread_create(&worker, NULL, serve, &fd));
    ns_scan_manager *m = ns_scan_manager_open(dir, socket_path, publish, NULL);
    assert(m);
    const char *request = "{\"request_id\":\"test1\",\"set\":{\"scan\":{\"start_freq_hz\":470000000,\"end_freq_hz\":470000000,\"mode\":\"500k\"}}}";
    handle(m, request);
    assert(strstr(last, "accepted"));
    handle(m, request);
    assert(strstr(last, "pending"));
    handle(m, "{\"request_id\":\"test1\",\"set\":{\"scan\":{\"start_freq_hz\":470000000,\"end_freq_hz\":471000000,\"mode\":\"500k\"}}}");
    assert(strstr(last, "request_id_conflict"));
    ns_scan_manager_tick(m);
    pthread_join(worker, NULL);
    assert(starts == 1);
    handle(m, request);
    assert(starts == 1 && strstr(last, "running"));
    fixture(dir, "exec-test1.txt", "470000000\n-120\n-121\n-122\n-123\n-124\n-125\n-126\n-127\n");
    fixture(dir, "exec-test1.json", "{\"request_id\":\"different\",\"state\":\"done\",\"result_ready\":true}");
    ns_scan_manager_tick(m);
    assert(results == 0);
    fixture(dir, "exec-test1.json", "{\"request_id\":\"test1\",\"state\":\"done\",\"result_ready\":true}");
    ns_scan_manager_tick(m);
    assert(results == 1 && strstr(last, "completed"));
    ns_scan_manager_tick(m);
    assert(results == 1);
    ns_scan_manager_close(m);
    m = ns_scan_manager_open(dir, socket_path, publish, NULL);
    assert(m);
    ns_scan_manager_tick(m);
    assert(results == 2);
    handle(m, "{\"scan_result_ack\":{\"request_id\":\"test1\"}}");
    ns_scan_manager_close(m);
    m = ns_scan_manager_open(dir, socket_path, publish, NULL);
    assert(m);
    ns_scan_manager_tick(m);
    assert(results == 2);
    handle(m, request);
    assert(strstr(last, "acknowledged") && starts == 1);
    fixture(dir, "exec-old.json", "{}");
    handle(m, "{\"request_id\":\"old\",\"set\":{\"scan\":{\"start_freq_hz\":470000000,\"end_freq_hz\":470000000,\"mode\":\"500k\"}}}");
    assert(strstr(last, "request_id_reserved"));
    const char *interrupted = "{\"request_id\":\"interrupted\",\"set\":{\"scan\":{\"start_freq_hz\":470000000,\"end_freq_hz\":470000000,\"mode\":\"500k\"}}}";
    handle(m, interrupted);
    ns_scan_manager_close(m);
    m = ns_scan_manager_open(dir, socket_path, publish, NULL);
    assert(m);
    ns_scan_manager_tick(m);
    assert(strstr(last, "interrupted") && starts == 1);
    ns_scan_manager_close(m);
    close(fd);
    unlink(socket_path);
    char missing[] = "/tmp/mqtt_scan_missing_XXXXXX";
    assert(mkdtemp(missing));
    m = ns_scan_manager_open(missing, socket_path, publish, NULL);
    assert(m);
    assert(!rmdir(missing));
    handle(m, request);
    assert(strstr(last, "storage_failed"));
    ns_scan_manager_close(m);
    fixture(dir, "job-broken.json", "{broken");
    assert(ns_scan_manager_open(dir, socket_path, publish, NULL) == NULL);
    printf("PASS manager: IPC start, duplicate/conflict, wrong-task isolation, 8-antenna result, restart resend, durable ACK; fixture=%s publications=%d\n", dir, publications);
    return 0;
}
