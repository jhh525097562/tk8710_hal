/* Unit test of the actual service parser; unused hardware paths are GC-linked. */
#include "../../src/tk8710_scan_ipc_server.c"
#include <assert.h>

int main(void)
{
    ScanTaskParams p;
    assert(ParseStartParams("START", &p) == 0);
    assert(ParseStartParams("START freq_start=470M freq_stop=510M mode=500k", &p) == 0);
    assert(ParseStartParams("START freq_start=470M freq_stop=510M mode=500k request_id=scan-001", &p) == 0);
    assert(p.start_freq == 470000000 && p.end_freq == 510000000 && p.sweep_mode == 3);
    assert(!strcmp(p.request_id, "scan-001"));
    assert(ParseStartParams("START request_id=only-id", &p) != 0);
    assert(ParseStartParams("START freq_start=nanM", &p) != 0);
    assert(ParseStartParams("START freq_start=infM", &p) != 0);
    assert(ParseStartParams("START freq_start=9999999M", &p) != 0);
    assert(ParseStartParams("START freq_start=510M freq_stop=470M", &p) != 0);
    assert(ParseStartParams("START mode=500k mode=125k", &p) != 0);
    assert(ParseStartParams("START request_id=../escape", &p) != 0);
    assert(ParseStartParams("START mode=62.5k freq_start=1M freq_stop=4000M", &p) != 0);
    char command[400]; memset(command, 'a', sizeof(command)); command[399] = 0;
    assert(ParseStartParams(command, &p) != 0);
    uint32_t hz;
    assert(ParseFreqToHz("470.0625M", &hz) == 0 && hz == 470062500);
    puts("PASS scan service params: legacy Web, NS identity, strict numbers, duplicate keys, overflow, capacity");
    return 0;
}
