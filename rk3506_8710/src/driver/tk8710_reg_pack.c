/* Auto-generated helper functions for tk8710_reg_pack.h. */
#include "../inc/driver/tk8710_reg_pack.h"

uint32_t TK8710RegPackSet(uint32_t reg, uint32_t shift, uint32_t width, uint32_t value)
{
    uint32_t mask = TK8710_REG_FIELD_MASK(width);
    return (reg & ~(mask << shift)) | ((value & mask) << shift);
}

uint32_t TK8710RegPackGet(uint32_t reg, uint32_t shift, uint32_t width)
{
    return (reg >> shift) & TK8710_REG_FIELD_MASK(width);
}

int32_t TK8710RegPackGetSigned(uint32_t reg, uint32_t shift, uint32_t width)
{
    uint32_t value = TK8710RegPackGet(reg, shift, width);
    uint32_t sign;
    if ((width == 0U) || (width >= 32U)) {
        return (int32_t)value;
    }
    sign = 1UL << (width - 1U);
    if ((value & sign) != 0U) {
        value |= ~TK8710_REG_FIELD_MASK(width);
    }
    return (int32_t)value;
}
