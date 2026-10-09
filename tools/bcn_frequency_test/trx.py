"""TRX reception outcomes. Count S0 BCN and S1 broadcast separately from S3."""
import collections
import re

MARKER = re.compile(r"\+(?:bcn|data)\s*:", re.IGNORECASE)
HEADER = re.compile(r"\+(bcn|data)\s*:\s*(\d+)\s*-\s*(\d+)\s*\|\s*([sf])(?=\s*(?:,|$))", re.IGNORECASE)


def parse_trx(text):
    match = HEADER.search(text)
    if not match:
        return None
    name, frame, slot, outcome = match.groups()
    name, outcome = name.lower(), outcome.lower()
    row = dict(trx_kind=name, frame=int(frame), slot=int(slot), outcome=outcome,
               success=outcome == "s", snr=None, rssi=None, cfo_hz=None,
               bcnbits=None, data_hex=None, pkt=None, crc=None)
    tail = text[match.end():]
    for field in ("snr", "rssi", "cfo", "pkt"):
        value = re.search(r"(?:^|,)\s*"+field+r"\s*:\s*(-?\d+)(?=\s*(?:,|$))", tail, re.I)
        if value:
            row["cfo_hz" if field == "cfo" else field] = int(value[1])
    crc = re.search(r"(?:^|,)\s*crc\s*:\s*([0-9a-f]+)(?=\s*(?:,|$))", tail, re.I)
    if crc:
        row["crc"] = crc[1]
    if name == "bcn":
        # data is hex, not decimal. Failure bytes are retained but never decoded.
        data = re.search(r"(?:^|,)\s*data\s*:\s*((?:0x)?[0-9a-f]+)(?=\s*(?:[.,]|$))", tail, re.I)
        if data:
            row["data_hex"] = data[1]
            if row["success"]:
                row["bcnbits"] = int(data[1], 16)
    return row


def _outcomes(rows):
    total = len(rows)
    successes = sum(r["success"] for r in rows)
    failures = total-successes
    longest = streak = 0
    previous_segment = None
    for row in rows:
        if row["segment"] != previous_segment:
            streak = 0
        streak = 0 if row["success"] else streak+1
        longest = max(longest, streak)
        previous_segment = row["segment"]
    return dict(records=total, success=successes, failure=failures,
                success_ratio=successes/total if total else None,
                failure_ratio=failures/total if total else None,
                max_consecutive_failure_records=longest,
                status="observed" if total else "no_records")


def summarize_trx(rows, bits_a, bits_b):
    bcn = [r for r in rows if r["trx_kind"] == "bcn" and r["slot"] == 0]
    broadcast = [r for r in rows if r["trx_kind"] == "data" and r["slot"] == 1]
    other = [r for r in rows if not (r["trx_kind"] == "bcn" and r["slot"] == 0)
             and not (r["trx_kind"] == "data" and r["slot"] == 1)]
    valid_bits = [r for r in bcn if r["success"] and r["bcnbits"] is not None]
    errors = sum(r["bcnbits"] not in (bits_a, bits_b) for r in valid_bits)
    bcn_stats = _outcomes(bcn)
    # Never count RX_TDD again as another BCN or broadcast reception.
    bcn_stats.update(bcnbits_samples=len(valid_bits), bcnbits_errors=errors,
                     bcnbits_error_ratio=errors/len(valid_bits) if valid_bits else None,
                     successful_missing_bcnbits=sum(r["success"] and r["bcnbits"] is None for r in bcn),
                     bcnbits_distribution=dict(collections.Counter(str(r["bcnbits"]) for r in valid_bits)))
    changes = bits_changes = 0
    previous = None
    for row in bcn:
        if not row["success"] or row["bcnbits"] is None or row["cfo_hz"] is None:
            previous = None
            continue
        if previous and row["segment"] == previous["segment"]:
            changes += row["cfo_hz"] != previous["cfo_hz"]
            bits_changes += row["bcnbits"] != previous["bcnbits"]
        previous = row
    bcn_stats.update(cfo_changes=changes, bcnbits_changes=bits_changes,
                     cfo_bcnbits_distribution=[dict(cfo_hz=cfo, bcnbits=bits, count=count)
                         for (cfo, bits), count in sorted(collections.Counter(
                             (r["cfo_hz"], r["bcnbits"]) for r in valid_bits
                             if r["cfo_hz"] is not None).items())])
    return dict(bcn=bcn_stats, broadcast=_outcomes(broadcast), other_slots=_outcomes(other),
                denominator="explicit_s_or_f_records_only",
                missing_record_policy="unknown_not_inferred_as_failure")


def flat_trx(stats):
    return {f"{name}_{field}": stats[name][field]
            for name in ("bcn", "broadcast")
            for field in ("records", "success", "failure", "success_ratio", "failure_ratio")}


def reception_html(summary):
    def ratio(value):
        return "无记录" if value is None else f"{value:.2%}"
    result = ['<h2>BCN 与广播接收（TRX 明确结果）</h2>',
              '<table border="1" cellspacing="0" cellpadding="6"><tr><th>范围</th><th>类型</th>'
              '<th>总记录</th><th>成功</th><th>失败</th><th>失败率</th></tr>']
    groups = [("全部有效窗口" if summary["windows"] else "日志样本", summary["trx"])]
    groups += [(f"{point['offset_hz']:+d} Hz", point["trx"]) for point in summary["points"]]
    for label, stats in groups:
        for key, title in (("bcn", "BCN（时隙0）"), ("broadcast", "广播（时隙1）")):
            item = stats[key]
            result.append(f"<tr><td>{label}</td><td>{title}</td><td>{item['records']}</td>"
                          f"<td>{item['success']}</td><td>{item['failure']}</td>"
                          f"<td>{ratio(item['failure_ratio'])}</td></tr>")
    bcn = summary["trx"]["bcn"]
    result.append(f'</table><p>BCN 成功且 bcnbits 可解析：{bcn["bcnbits_samples"]} 条；'
                  f'bcnbits 集合外错误：{bcn["bcnbits_errors"]} 条；'
                  f'BCN 成功但 bcnbits 无法解析：{bcn["successful_missing_bcnbits"]} 条。</p>')
    result.append('<p>只以对应 TRX 行明确打印的 s/f 为成功/失败依据；时隙3不计入广播。'
                  '无日志为未知，旧 RX_TDD 日志无法补推明确失败数。BCN失败时 data 不作为有效 bcnbits。'
                  'BCN成功但广播失败仍保留该 BCN 的 CFO/bcnbits 样本；RX_TDD 不重复累计。</p>')
    return "".join(result)
