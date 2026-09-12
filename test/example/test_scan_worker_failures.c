#define SCAN_STATUS_FILE "/tmp/scan-worker-unit-status.json"
#define SCAN_STATUS_TMP_FILE "/tmp/scan-worker-unit-status.tmp"
#include "../../src/tk8710_scan_ipc_server.c"
#include <assert.h>
static int start_result, restore_result, restore_count, stop_count;
int TK8710ScanStart(uint32_t a, uint32_t b, int mode)
{ (void)a; (void)b; (void)mode; return start_result; }
int TRM_GetSweepState(TRM_SweepState *state) { (void)state; return -1; }
int TRM_StopFrequencySweep(void) { stop_count++; return 0; }
static int Restore(void) { restore_count++; return restore_result; }
static void Check(const char *expected)
{
    g_scan_ipc_ctx.task_running = 1;
    ScanTaskThread(&g_scan_ipc_ctx);
    assert(!g_scan_ipc_ctx.task_running);
    char text[1024] = {0}; FILE *f = fopen(SCAN_STATUS_FILE, "r"); assert(f);
    assert(fread(text, 1, sizeof(text)-1, f) > 0); fclose(f);
    assert(strstr(text, "\"state\": \"failed\""));
    assert(strstr(text, expected));
}
int main(void)
{
    pthread_mutex_init(&g_scan_ipc_ctx.lock, NULL);
    g_scan_ipc_ctx.initialized = 1;
    g_restore_handler = Restore;
    start_result = -1; Check("scan_start_failed"); assert(restore_count == 1);
    start_result = 0; Check("scan_failed_or_timeout"); assert(restore_count == 2);
    restore_result = -1; Check("restore_failed"); assert(restore_count == 3);
    restore_result = 0; g_scan_ipc_ctx.stopping = 1;
    Check("scan_failed_or_timeout"); assert(stop_count == 1 && restore_count == 4);
    unlink(SCAN_STATUS_FILE); unlink(SCAN_STATUS_TMP_FILE);
    puts("PASS worker: start failure, status failure, restore failure, stop and restore");
    return 0;
}
