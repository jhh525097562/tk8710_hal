/* TI Hercules TMS570LS31x bootloader linker command file
 * Reconstructed for the official ls31_can_boot example layout.
 */

--retain="*(.intvecs)"

MEMORY
{
    VECTORS (X)  : origin = 0x00000000 length = 0x00000020
    FLASH0  (RX) : origin = 0x00000020 length = 0x0000FFE0
    STACKS  (RW) : origin = 0x08000000 length = 0x00001500
    RAM     (RW) : origin = 0x08001500 length = 0x0003EB00

    ECC_VEC    (R) : origin = 0xF0400000 length = 0x00000004 ECC = { input_range=VECTORS, algorithm=algoR4F021 }
    ECC_FLASH0 (R) : origin = 0xF0400004 length = 0x00001FFC ECC = { input_range=FLASH0, algorithm=algoR4F021 }
}

ECC
{
    algoR4F021 : address_mask = 0x003FFFF8
                  hamming_mask = R4
                  parity_mask  = 0x0C
                  mirroring    = F021
}

SECTIONS
{
    .intvecs    : {} > VECTORS
    .text       : {} > FLASH0
    .cinit      : {} > FLASH0
    .pinit      : {} > FLASH0
    .binit      : {} > FLASH0
    .init_array : {} > FLASH0

    .const      : LOAD = FLASH0,
                  RUN = RAM,
                  LOAD_START(constLoadStart),
                  RUN_START(constRunStart),
                  LOAD_SIZE(constLoadSize)

    .flashAPI   : LOAD = FLASH0,
                  RUN = RAM,
                  LOAD_START(apiLoadStart),
                  RUN_START(apiRunStart),
                  LOAD_SIZE(apiLoadSize)
    {
        "__F021_LIBRARY__"(.text)
        Fapi_UserDefinedFunctions.obj(.text)
    }

    .bss        : {} > RAM
    .data       : {} > RAM
    .sysmem     : {} > RAM
    .args       : {} > RAM
    .cio        : {} > RAM
    .stack      : {} > STACKS
}
