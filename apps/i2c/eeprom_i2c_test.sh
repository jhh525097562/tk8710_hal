#!/bin/bash

# Test an AT24C02C-like EEPROM with i2c-tools.
# The script scans for EEPROM addresses, reads Unique ID when available,
# performs two write/read checks, and restores original EEPROM data.

set -u

BUS="${1:-0}"
TEST_OFFSET1="${TEST_OFFSET1:-0xf0}"
TEST_OFFSET2="${TEST_OFFSET2:-0xf8}"
WRITE_DELAY_SEC="${WRITE_DELAY_SEC:-0.02}"

PASS_COUNT=0
FAIL_COUNT=0

log()
{
    printf '%s\n' "$*"
}

pass()
{
    PASS_COUNT=$((PASS_COUNT + 1))
    log "PASS: $*"
}

fail()
{
    FAIL_COUNT=$((FAIL_COUNT + 1))
    log "FAIL: $*"
}

die()
{
    fail "$*"
    exit 1
}

need_cmd()
{
    command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

hex_to_dec()
{
    printf '%d' "$1"
}

dec_to_hex_addr()
{
    printf '0x%02x' "$1"
}

scan_has_addr()
{
    addr_no_prefix=$(printf '%02x' "$(hex_to_dec "$1")")
    i2cdetect -y "$BUS" 2>/dev/null | while IFS= read -r line; do
        for token in $line; do
            [ "$token" = "$addr_no_prefix" ] && exit 0
        done
    done
}

read_bytes()
{
    dev_addr="$1"
    offset="$2"
    len="$3"
    i2ctransfer -y "$BUS" "w1@$dev_addr" "$offset" "r$len" 2>/dev/null
}

write_bytes()
{
    dev_addr="$1"
    offset="$2"
    shift 2
    count=$((1 + $#))
    i2ctransfer -y "$BUS" "w$count@$dev_addr" "$offset" "$@" >/dev/null 2>&1
}

normalize_bytes()
{
    for byte in $*; do
        printf '%02x ' "$(hex_to_dec "$byte")"
    done | sed 's/ $//'
}

restore_original()
{
    dev_addr="$1"
    offset="$2"
    original="$3"

    if [ -n "$original" ]; then
        # shellcheck disable=SC2086
        write_bytes "$dev_addr" "$offset" $original
        sleep "$WRITE_DELAY_SEC"
    fi
}

run_rw_test()
{
    dev_addr="$1"
    offset="$2"
    pattern="$3"
    len="$4"

    log ""
    log "Testing EEPROM data area: dev=$dev_addr offset=$offset len=$len"

    original=$(read_bytes "$dev_addr" "$offset" "$len") || {
        fail "read original data failed at offset $offset"
        return 1
    }

    original_norm=$(normalize_bytes $original)
    pattern_norm=$(normalize_bytes $pattern)
    log "Original: $original_norm"
    log "Write   : $pattern_norm"

    # shellcheck disable=SC2086
    if ! write_bytes "$dev_addr" "$offset" $pattern; then
        fail "write test pattern failed at offset $offset"
        return 1
    fi
    sleep "$WRITE_DELAY_SEC"

    readback=$(read_bytes "$dev_addr" "$offset" "$len") || {
        fail "read back test pattern failed at offset $offset"
        restore_original "$dev_addr" "$offset" "$original"
        return 1
    }

    readback_norm=$(normalize_bytes $readback)
    log "Readback: $readback_norm"

    if [ "$readback_norm" = "$pattern_norm" ]; then
        pass "write/read compare matched at offset $offset"
    else
        fail "write/read compare mismatch at offset $offset"
        log "Expected: $pattern_norm"
        log "Actual  : $readback_norm"
    fi

    restore_original "$dev_addr" "$offset" "$original"
    restored=$(read_bytes "$dev_addr" "$offset" "$len") || {
        fail "read restored data failed at offset $offset"
        return 1
    }
    restored_norm=$(normalize_bytes $restored)

    if [ "$restored_norm" = "$original_norm" ]; then
        pass "original data restored at offset $offset"
    else
        fail "restore compare mismatch at offset $offset"
        log "Original: $original_norm"
        log "Current : $restored_norm"
    fi
}

find_eeprom_addr()
{
    for addr in 0x50 0x51 0x52 0x53 0x54 0x55 0x56 0x57; do
        if scan_has_addr "$addr"; then
            printf '%s' "$addr"
            return 0
        fi
    done
    return 1
}

need_cmd i2cdetect
need_cmd i2ctransfer
need_cmd sed

log "I2C EEPROM test"
log "Bus: /dev/i2c-$BUS"
log "Scanning I2C bus..."
i2cdetect -y "$BUS" || die "i2cdetect failed on bus $BUS"

EEPROM_ADDR=$(find_eeprom_addr) || die "no EEPROM address found in 0x50-0x57"
EEPROM_DEC=$(hex_to_dec "$EEPROM_ADDR")
SPECIAL_ADDR=$(dec_to_hex_addr $((EEPROM_DEC + 8)))

pass "found EEPROM data address: $EEPROM_ADDR"

if scan_has_addr "$SPECIAL_ADDR"; then
    pass "found special address for identification/unique ID: $SPECIAL_ADDR"
    log ""
    log "Reading Unique ID area from $SPECIAL_ADDR, word address 0x80, len 16..."
    UID=$(read_bytes "$SPECIAL_ADDR" 0x80 16) || UID=""
    if [ -n "$UID" ]; then
        log "Unique ID: $(normalize_bytes $UID)"
        pass "unique ID read completed"
    else
        fail "unique ID read failed or returned empty data"
    fi
else
    fail "special address $SPECIAL_ADDR not found, skip unique ID read"
fi

run_rw_test "$EEPROM_ADDR" "$TEST_OFFSET1" "0xa5 0x5a 0x3c 0xc3" 4
run_rw_test "$EEPROM_ADDR" "$TEST_OFFSET2" "0x12 0x34 0x56 0x78" 4

log ""
log "Result: PASS=$PASS_COUNT FAIL=$FAIL_COUNT"

if [ "$FAIL_COUNT" -eq 0 ]; then
    log "EEPROM TEST PASS"
    exit 0
fi

log "EEPROM TEST FAIL"
exit 1
