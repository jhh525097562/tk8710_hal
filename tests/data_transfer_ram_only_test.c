#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "data_transfer.h"

static uint8_t g_payload[65527U];

int main(void)
{
    uint32_t before;

    DataTransfer_TestReset();
    DataTransfer_Init();

    assert(DATA_TRANSFER_SPI_FLASH_ENABLED == 0U);
    assert(DataTransfer_AppendBytes(0x80U, g_payload,
                                    (uint16_t)sizeof(g_payload)) == 0);
    before = DataTransfer_GetPendingLength();
    assert(before == 65535U);

    assert(DataTransfer_AppendString(0x80U, "X", 1U) == -1);
    assert(DataTransfer_GetPendingLength() == before);

    puts("data_transfer_ram_only_test: PASS");
    return 0;
}
