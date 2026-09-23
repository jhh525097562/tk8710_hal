#define _POSIX_C_SOURCE 200809L

#include "ns_scan_manager.h"

#include "ns_scan_protocol.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MAX_JOBS 32

typedef struct {
    ns_scan_request request;
    struct json_object *record;
    time_t last_publish;
    int recovered;
} scan_job;

struct ns_scan_manager {
    char directory[256];
    char socket_path[108];
    pthread_mutex_t lock;
    ns_scan_publish_fn publish;
    void *context;
    int count;
    scan_job jobs[MAX_JOBS];
};

static const char *string_field(struct json_object *object, const char *key)
{
    struct json_object *v;
    if (!json_object_object_get_ex(object, key, &v) || !json_object_is_type(v, json_type_string)) {
        return "";
    }
    return json_object_get_string(v);
}

static void job_path(ns_scan_manager *m, char *path, size_t length, const char *prefix,
                     const char *id, const char *extension)
{
    snprintf(path, length, "%s/%s-%s.%s", m->directory, prefix, id, extension);
}

static struct json_object *load_json(const char *path)
{
    struct stat st;
    if (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size > NS_SCAN_MAX_JSON + 4096) {
        return NULL;
    }
    return json_object_from_file(path);
}

static int save_job(ns_scan_manager *m, scan_job *j)
{
    char path[384];
    char tmp[400];
    const char *data;
    FILE *fp;
    int failed = 0;
    int fd;
    int ret;

    job_path(m, path, sizeof(path), "job", j->request.request_id, "json");
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    data = json_object_to_json_string_ext(j->record, JSON_C_TO_STRING_PLAIN);
    if (data == NULL || strlen(data) > NS_SCAN_MAX_JSON + 4096) {
        return -1;
    }
    fp = fopen(tmp, "w");
    if (fp == NULL) {
        return -1;
    }
    if (fputs(data, fp) == EOF) {
        failed = 1;
    }
    if (fflush(fp) || fsync(fileno(fp))) {
        failed = 1;
    }
    if (fclose(fp)) {
        failed = 1;
    }
    if (!failed && rename(tmp, path)) {
        failed = 1;
    }
    if (failed) {
        unlink(tmp);
        return -1;
    }
    fd = open(m->directory, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return -1;
    }
    ret = fsync(fd);
    close(fd);
    return ret;
}

static void publish_ack(ns_scan_manager *m, const char *id, const char *state, const char *error)
{
    struct json_object *root = json_object_new_object();
    struct json_object *ack = json_object_new_object();

    if (root == NULL || ack == NULL) {
        json_object_put(root);
        json_object_put(ack);
        return;
    }
    json_object_object_add(root, "scan_ack", ack);
    json_object_object_add(ack, "request_id", json_object_new_string(id));
    json_object_object_add(ack, "state", json_object_new_string(state));
    if (error != NULL) {
        json_object_object_add(ack, "error_code", json_object_new_string(error));
    }
    m->publish(m->context, json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN), 1);
    json_object_put(root);
}

static int active(scan_job *j)
{
    const char *phase = string_field(j->record, "phase");
    return strcmp(phase, "pending") == 0 || strcmp(phase, "running") == 0;
}

static void finish(ns_scan_manager *m, scan_job *j, struct json_object *result, const char *error)
{
    if (result == NULL) {
        struct json_object *body;

        result = json_object_new_object();
        body = json_object_new_object();
        if (result == NULL || body == NULL) {
            json_object_put(result);
            json_object_put(body);
            return;
        }
        json_object_object_add(result, "scan_result", body);
        json_object_object_add(body, "request_id", json_object_new_string(j->request.request_id));
        json_object_object_add(body, "state", json_object_new_string("failed"));
        json_object_object_add(body, "error_code", json_object_new_string(error));
        json_object_object_add(body, "start_freq_hz", json_object_new_int64(j->request.start_hz));
        json_object_object_add(body, "end_freq_hz", json_object_new_int64(j->request.end_hz));
        json_object_object_add(body, "mode", json_object_new_string(j->request.mode));
    }
    json_object_object_add(j->record, "result", result);
    json_object_object_add(j->record, "phase", json_object_new_string("ready"));
    j->last_publish = 0;
    if (save_job(m, j)) {
        fprintf(stderr, "scan journal write failed: %s\n", j->request.request_id);
    }
}

ns_scan_manager *ns_scan_manager_open(const char *directory, const char *socket_path,
                                      ns_scan_publish_fn publish, void *context)
{
    DIR *dir;
    ns_scan_manager *m;
    struct dirent *e;

    if (directory == NULL || socket_path == NULL || publish == NULL ||
        strlen(directory) >= 256 || strlen(socket_path) >= 108) {
        return NULL;
    }
    if (mkdir(directory, 0700) && errno != EEXIST) {
        return NULL;
    }
    dir = opendir(directory);
    if (dir == NULL) {
        return NULL;
    }
    m = calloc(1, sizeof(*m));
    if (m == NULL) {
        closedir(dir);
        return NULL;
    }
    strcpy(m->directory, directory);
    strcpy(m->socket_path, socket_path);
    m->publish = publish;
    m->context = context;
    pthread_mutex_init(&m->lock, NULL);
    while ((e = readdir(dir)) != NULL) {
        size_t n = strlen(e->d_name);
        char path[512];
        struct json_object *record;
        struct json_object *request;
        scan_job *j;
        const char *error;
        char expected[80];
        const char *phase;

        if (n < 10 || strncmp(e->d_name, "job-", 4) || strcmp(e->d_name + n - 5, ".json")) {
            continue;
        }
        if (m->count >= MAX_JOBS) {
            goto fail;
        }
        snprintf(path, sizeof(path), "%s/%s", directory, e->d_name);
        record = load_json(path);
        j = &m->jobs[m->count];
        if (record == NULL || !json_object_object_get_ex(record, "request", &request) ||
            ns_scan_parse(request, &j->request, &error) != 1) {
            json_object_put(record);
            goto fail;
        }
        snprintf(expected, sizeof(expected), "job-%s.json", j->request.request_id);
        phase = string_field(record, "phase");
        if (strcmp(expected, e->d_name) ||
            (strcmp(phase, "pending") && strcmp(phase, "running") &&
             strcmp(phase, "ready") && strcmp(phase, "acknowledged"))) {
            json_object_put(record);
            goto fail;
        }
        j->record = record;
        j->recovered = 1;
        m->count++;
    }
    closedir(dir);
    return m;

fail:
    closedir(dir);
    ns_scan_manager_close(m);
    return NULL;
}

int ns_scan_manager_handle(ns_scan_manager *m, struct json_object *message)
{
    struct json_object *ack;
    struct json_object *id;
    ns_scan_request request;
    const char *error;
    int ret;

    if (m == NULL) {
        return 0;
    }
    pthread_mutex_lock(&m->lock);
    if (json_object_object_get_ex(message, "scan_result_ack", &ack)) {
        if (json_object_object_get_ex(ack, "request_id", &id) && json_object_is_type(id, json_type_string) &&
            strlen(json_object_get_string(id)) == (size_t)json_object_get_string_len(id) &&
            ns_scan_id_valid(json_object_get_string(id))) {
            for (int i = 0; i < m->count; ++i) {
                scan_job *j = &m->jobs[i];
                if (strcmp(j->request.request_id, json_object_get_string(id)) == 0 &&
                    strcmp(string_field(j->record, "phase"), "ready") == 0) {
                    json_object_object_add(j->record, "phase", json_object_new_string("acknowledged"));
                    if (save_job(m, j)) {
                        json_object_object_add(j->record, "phase", json_object_new_string("ready"));
                    }
                    break;
                }
            }
        }
        pthread_mutex_unlock(&m->lock);
        return 1;
    }

    ret = ns_scan_parse(message, &request, &error);
    if (!ret) {
        pthread_mutex_unlock(&m->lock);
        return 0;
    }
    if (ret < 0) {
        publish_ack(m, request.request_id, "rejected", error);
        goto done;
    }
    for (int i = 0; i < m->count; ++i) {
        if (strcmp(m->jobs[i].request.request_id, request.request_id) == 0) {
            int same = ns_scan_same(&m->jobs[i].request, &request);
            publish_ack(m, request.request_id, same ? string_field(m->jobs[i].record, "phase") : "rejected",
                        same ? NULL : "request_id_conflict");
            goto done;
        }
    }
    for (int i = 0; i < m->count; ++i) {
        if (active(&m->jobs[i])) {
            publish_ack(m, request.request_id, "busy", "scan_running");
            goto done;
        }
    }
    if (m->count >= MAX_JOBS) {
        publish_ack(m, request.request_id, "rejected", "history_full");
        goto done;
    }
    {
        char existing[384];
        scan_job *j;

        job_path(m, existing, sizeof(existing), "exec", request.request_id, "json");
        if (access(existing, F_OK) == 0) {
            publish_ack(m, request.request_id, "rejected", "request_id_reserved");
            goto done;
        }
        j = &m->jobs[m->count];
        j->request = request;
        j->record = json_object_new_object();
        if (j->record == NULL) {
            publish_ack(m, request.request_id, "rejected", "no_memory");
            goto done;
        }
        json_object_object_add(j->record, "request", json_object_get(message));
        json_object_object_add(j->record, "phase", json_object_new_string("pending"));
        json_object_object_add(j->record, "created_at", json_object_new_int64(time(NULL)));
        if (save_job(m, j)) {
            json_object_put(j->record);
            memset(j, 0, sizeof(*j));
            publish_ack(m, request.request_id, "rejected", "storage_failed");
            goto done;
        }
        m->count++;
        publish_ack(m, request.request_id, "accepted", NULL);
    }

done:
    pthread_mutex_unlock(&m->lock);
    return 1;
}

static int start_job(ns_scan_manager *m, scan_job *j)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct timeval timeout = {2, 0};
    struct sockaddr_un addr = {0};
    char command[256];
    char response[256] = {0};
    int n;
    size_t sent = 0;
    size_t used = 0;

    if (fd < 0) {
        return -1;
    }
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, m->socket_path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        close(fd);
        return -1;
    }
    n = snprintf(command, sizeof(command), "START freq_start=%.6fM freq_stop=%.6fM mode=%s request_id=%s\n",
                 j->request.start_hz / 1000000.0, j->request.end_hz / 1000000.0,
                 j->request.mode, j->request.request_id);
    if (n < 0 || (size_t)n >= sizeof(command)) {
        close(fd);
        return -1;
    }
    while (sent < (size_t)n) {
        ssize_t s = send(fd, command + sent, n - sent, MSG_NOSIGNAL);
        if (s < 0 && errno == EINTR) {
            continue;
        }
        if (s <= 0) {
            close(fd);
            return -1;
        }
        sent += s;
    }
    while (used < sizeof(response) - 1) {
        ssize_t r = recv(fd, response + used, sizeof(response) - 1 - used, 0);
        if (r < 0 && errno == EINTR) {
            continue;
        }
        if (r <= 0) {
            break;
        }
        used += r;
        if (memchr(response, '\n', used)) {
            break;
        }
    }
    close(fd);
    if (strcmp(response, "OK\n") == 0) {
        return 0;
    }
    if (strcmp(response, "BUSY\n") == 0) {
        return 1;
    }
    return -1;
}

void ns_scan_manager_tick(ns_scan_manager *m)
{
    time_t now;

    if (m == NULL) {
        return;
    }
    pthread_mutex_lock(&m->lock);
    now = time(NULL);
    for (int i = 0; i < m->count; ++i) {
        scan_job *j = &m->jobs[i];
        if (active(j)) {
            char path[384];
            struct json_object *status;

            job_path(m, path, sizeof(path), "exec", j->request.request_id, "json");
            status = load_json(path);
            if (status != NULL && strcmp(string_field(status, "request_id"), j->request.request_id) == 0) {
                const char *state = string_field(status, "state");
                struct json_object *ready;

                if (j->recovered && strcmp(state, "running") == 0) {
                    json_object_object_add(j->record, "phase", json_object_new_string("running"));
                    save_job(m, j);
                    j->recovered = 0;
                }
                if (strcmp(state, "done") == 0 && json_object_object_get_ex(status, "result_ready", &ready) &&
                    json_object_get_boolean(ready)) {
                    const char *error;
                    struct json_object *result;

                    job_path(m, path, sizeof(path), "exec", j->request.request_id, "txt");
                    result = ns_scan_result(&j->request, path, &error);
                    finish(m, j, result, error);
                } else if (strcmp(state, "failed") == 0) {
                    finish(m, j, NULL, "executor_failed");
                }
            } else if (j->recovered) {
                finish(m, j, NULL, "interrupted");
            }
            json_object_put(status);
            if (strcmp(string_field(j->record, "phase"), "pending") == 0) {
                int ret;

                json_object_object_add(j->record, "phase", json_object_new_string("running"));
                if (save_job(m, j)) {
                    json_object_object_add(j->record, "phase", json_object_new_string("pending"));
                    continue;
                }
                ret = start_job(m, j);
                if (ret) {
                    finish(m, j, NULL, ret == 1 ? "executor_busy" : "executor_unavailable");
                }
            }
            {
                struct json_object *created;
                if (active(j) && json_object_object_get_ex(j->record, "created_at", &created) &&
                    now - json_object_get_int64(created) > 600) {
                    finish(m, j, NULL, "timeout");
                }
            }
        }
        if (strcmp(string_field(j->record, "phase"), "ready") == 0 &&
            (j->last_publish == 0 || now - j->last_publish >= 10)) {
            struct json_object *result;
            if (!save_job(m, j) && json_object_object_get_ex(j->record, "result", &result)) {
                const char *data = json_object_to_json_string_ext(result, JSON_C_TO_STRING_PLAIN);
                m->publish(m->context, data, 1);
                j->last_publish = now;
            }
        }
    }
    pthread_mutex_unlock(&m->lock);
}

void ns_scan_manager_close(ns_scan_manager *m)
{
    if (m == NULL) {
        return;
    }
    for (int i = 0; i < m->count; ++i) {
        json_object_put(m->jobs[i].record);
    }
    pthread_mutex_destroy(&m->lock);
    free(m);
}
