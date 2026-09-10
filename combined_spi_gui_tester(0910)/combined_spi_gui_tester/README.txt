TMS570 SPI1/SPI2 Combined GUI Tester

Purpose

This tool combines the previous SPI1 FPGA tester and SPI2 data-transfer tester
into one Windows GUI. It keeps separate logs for:

  SPI1: FPGA remote-control commands and telemetry frames
  SPI2: data-transfer frames and decoded records

Build

  powershell -ExecutionPolicy Bypass -File .\tools\combined_spi_gui_tester\build.ps1

Run

  .\tools\combined_spi_gui_tester\combined_spi_gui_tester.exe

Local self-test, no hardware required

  .\tools\combined_spi_gui_tester\combined_spi_gui_tester.exe selftest

Report tests

  The GUI has a Run Report Tests button. The UART COM 下拉框会自动枚举 Windows 串口;
  choose the DUT UART port such as COM7 and enter the baud rate before running.
  The default DUT UART baud rate is 1000000. The runner opens the UART port
  exclusively, so close SSCOM or any other serial terminal first. It also uses
  the selected SPI1/SPI2 devices when available.

  The first-version runner generates:

    reports\report_YYYYMMDD_HHMMSS.md
    reports\raw_YYYYMMDD_HHMMSS.log

  The report follows the item set from docs\载荷基带软件测试报告_v0.1.md: each
  report item appears once. Local command-packing and parser checks are covered by
  selftest, but they are not report test items because they do not touch the DUT.
  PASS means a selected UART/SPI operation completed and, where applicable, the
  SPI1 telemetry readback matched the commanded value. Rows that need RF
  equipment, terminals, external reset control, bootloader CAN integration, or
  24-hour operation are marked SKIP with a manual-test reason. Failed owned I/O
  operations are marked FAILED.

  Range parameters are traversed inside one report row and summarized in Detail.
  For example, TC-01 traverses workMode 0..6 and only passes if every value is
  sent through SPI1 and DATA20 reads back the same value. TC-02 traverses rate
  0..2 and checks DATA21. TC-06 traverses representative RF mask values 0x00,
  0x01, 0x02, 0x55, 0xAA, and 0xFF and checks DATA28. If traversal stops on a
  failure, Detail records the failing value and how many values had passed.

Device selection

The JTOOL DLL exposes SPI master APIs. It does not expose a reliable role name
that says which USB adapter is wired to SPI1 or SPI2.

The GUI therefore does both:

  1. It parses SN values from DevicesScan output, labels them as SN1, SN2, etc.,
     and opens those entries with
     DevOpen(dev_spi, SN, 0). This is preferred because numeric IDs can map to
     the same physical adapter when two identical SPI-USB adapters are plugged in.
  2. It also calls DevicesScan(dev_all) as a fallback. Some driver versions show
     both adapters there even when DevicesScan(dev_spi) reports only one.
  3. If fewer than two SN entries are available, the GUI still shows Manual ID #0
     through Manual ID #3. These are last-resort entries opened with
     DevOpen(dev_spi, NULL, Id).
  4. SPI1 defaults to the first selectable entry and SPI2 defaults to the second
     selectable entry when available.
  5. If the physical USB roles are swapped, choose different entries in the
     SPI1 device and SPI2 device drop-down boxes.
  6. If only one physical USB adapter is connected, select the first entry for the bus you
     are using and stop the other monitor.

The GUI automatically rescans about every 2 seconds and also reacts to Windows
USB device-change messages. You can start with SPI1 connected, open the GUI,
then plug SPI2; the drop-down list should refresh without restarting the tool.

If two adapters are connected, prefer SN entries:

  SPI1 device: SN1 <adapter wired to SPI1>
  SPI2 device: SN2 <adapter wired to SPI2>

If only Manual ID entries are shown, they are fallback entries. Try Manual ID #0
and Manual ID #1, but if both point to the same adapter then the DLL is not
exposing a distinct second SPI device through numeric IDs; use SN entries from
the refreshed scan output instead.

If the logs are reversed, swap SPI1 and SPI2 selections.

Window layout

The log panes resize with the window. Maximizing the GUI expands the SPI1 and
SPI2 log areas to fill the available client area below the controls.

SPI mode

The GUI uses fixed SPI clock polarity/phase modes for the JTOOL DLL:

  0 LOW_1EDG
  1 LOW_2EDG
  2 HIGH_1EDG
  3 HIGH_2EDG

SPI1 is fixed at 0 LOW_1EDG. SPI2 slave-test capture is fixed at 2 HIGH_1EDG.

File logs

The GUI writes daily log files under the tool's working directory:

  logs\spi1_YYYYMMDD.log
  logs\spi2_YYYYMMDD.log

Every GUI log line and file log line starts with local time in this format:

  YYYY-MM-DD HH:MM:SS message

The file changes automatically when the date changes.

SPI1 telemetry

Click Start SPI1 TM. The same button changes to Stop SPI1 TM while the SPI1
monitor thread is running. The monitor performs continuous 128-byte SPI reads and
also sends queued SPI1 commands.

Telemetry decode/log output has its own Start TM RX / Stop TM RX toggle. When TM
RX is enabled, the monitor searches for telemetry page 0 header EB 90, combines
page 0 and page 1 into one 144-byte telemetry frame, validates checksum, then
prints decoded telemetry and raw TM bytes into the SPI1 log. When TM RX is
disabled, the SPI1 monitor keeps running for command traffic but skips telemetry
decode and TM frame logging. To keep the GUI responsive during long runs,
telemetry log output is rate-limited to about once per second and each log window
is capped to a bounded text length.

SPI1 commands

Select a command in the SPI1 command drop-down, enter PARAM bytes in Params, and
click Send.

If Start SPI1 TM is running, Send does not open a second SPI handle. It queues
the command for the SPI1 monitor thread, and the monitor sends it on the next SPI
transaction. This works even when TM RX is stopped, and avoids racing two handles
on the same USB-SPI adapter.

The SPI1 log prints both lines when this path is used:

  queued CMD_01 for SPI1 monitor repeat=1: ...
  queued command sent by SPI1 monitor: ...

The text under Params shows the required format for the selected command. The
GUI packs multi-byte values automatically, so normal use is value-oriented, not
raw-byte-oriented. Examples:

  CMD_01 work mode:       3    (valid DUT range 0..6; GUI allows out-of-range negative tests)
  CMD_02 rate:            2    (valid DUT range 0..2; DUT maps to TK8710 rateMode 6/7/8; telemetry shows 0..2)
  CMD_03 slotConfig:      64 or 0x40
  CMD_04 txPower:         26
  CMD_05 frequency Hz:    473200000
  CMD_06 rfMask:          0xFF
  CMD_07 reset:           leave empty
  CMD_08 upgrade:         01 02 03 04 05 06
  CMD_09 data transfer:   1
  CMD_0A write register:  0x1234 0xAABBCCDD
  CMD_0B read register:   0x1234
  CMD_0C rollback:        01 02 03 04 05 06
  CMD_0D UTC seconds:     1723246576
  CMD_0E DC params:       1 0x0010 0xFFF0

The Manual raw SPI1 frame entry is raw-byte-oriented. It accepts 1 through 128
hex bytes and sends exactly that many bytes on SPI1 MOSI; the GUI does not add a
frame header, pad to 10 bytes, recompute checksum, or truncate to the RC frame
length. This is intended for malformed-frame test cases, including frames shorter
than 10 bytes and frames longer than 10 bytes. Examples:

  Manual short frame:      76 25 09
  Manual valid CMD_09 on:  76 25 09 01 00 00 00 00 00 0A
  Manual long frame:       76 25 09 01 00 00 00 00 00 0A AA BB

Manual frame logs include the actual wire length:

  queued manual SPI1 frame len=3 for SPI1 monitor repeat=1: 76 25 09
  queued manual SPI1 frame sent by SPI1 monitor len=3: 76 25 09

CMD_09 data transfer uses PARAM[0] as the switch. PARAM[0]=1 opens continuous
data transfer, and PARAM[0]=0 closes it. The normal GUI command sends:

  76 25 09 01 00 00 00 00 00 0A

CMD_09 repeat is fixed at 1. If Start SPI2 DT is running, sending CMD_09 on keeps
the SPI2 capture active after the fixed arm delay until CMD_09 off or Stop SPI2 DT.
A manual raw frame only starts continuous SPI2 capture when it exactly matches the
valid 10-byte CMD_09 on frame above. Short, long, off, or otherwise malformed
manual frames are sent on SPI1 only.

SPI2 data transfer

The SPI2 log decodes data-transfer frames:

  sync[4] + dataLength[2] + packetSeq[2] + data[0..501] + checksum[2]

Each frame carries up to 502 bytes of record stream. Large records can span
frames; for example, AT+DTFILL64K creates 520-byte records
(8-byte record header + 512-byte payload), so at least two SPI2 frames are needed
before the first decoded RECORD line can appear.

Decoded records are printed as:

  RECORD index=N rawTime=0x... format=string|hex type=0x.. length=N content=...

SPI2 bench transfer mode

Use this only with firmware built using -DataTransferSpi2SlaveTest. The PC/JTOOL
acts as SPI master and clocks one byte at a time. The firmware services SPI2
slave-test with DMA, so the GUI no longer exposes SPI2 read mode or byte-delay
controls. It keeps the SPI2 handle open from Start SPI2 DT until Stop SPI2 DT, and
continuous capture starts after CMD_09 on is sent.
Start SPI2 DT is a toggle button; while SPI2 monitoring is active the same button
shows Stop SPI2 DT.

Important limitation

The current jtool.dll has no SPI slave or passive-sniffer API. Therefore this GUI
cannot capture the official production path where TMS570 SPI2 is master and FPGA
is slave. For PC/JTOOL validation of SPI2 data transfer, use the temporary
-DataTransferSpi2SlaveTest firmware and the fixed SPI2 slave-test GUI capture.

Recommended bench sequence

1. Build and flash the temporary SPI2 slave-test firmware:

   powershell -ExecutionPolicy Bypass -File .\scripts\build-tms570.ps1 -Configuration Debug -FpgaSelfTest -DataTransferSpi2SlaveTest

2. Build and run this GUI.

3. Click Scan/Auto.

4. Choose the correct SPI1 and SPI2 USB devices from the drop-down boxes. If the
   logs do not match the expected bus, swap the selections.

5. Click Start SPI2 DT. The button changes to Stop SPI2 DT while active. The GUI
   uses fixed SPI2 mode 2 HIGH_1EDG.

6. Prepare pending data on UART:

   AT+DTWRITE=H,1,1A2B3C

7. Select CMD_09 data transfer in the SPI1 command section and click Send.

8. Expected SPI2 log output includes one or more DT frame lines and decoded
   records once enough record bytes have been captured:

   DT frame=1 captureFrame=1 seq=0 payload=...

   RECORD index=0 rawTime=0x... format=hex type=0x01 length=3 content=1A 2B 3C

9. While CMD_09 remains on, additional UART writes such as AT+DTWRITE=S,1,hello
   should produce new SPI2 DT frame/RECORD lines without another CMD_09 on click.

10. For AT+DTFILL64K, use one CMD_09 on click and wait. The GUI should keep
    reading multiple SPI2 frames and eventually print records with type=0x7E
    length=512.

11. If no DT frame appears, check SPI2 wiring, CS0, mode 2, and AT+FPGATM DMA
    diagnostics such as dmaRxRem, dmaTxRem, dmaBTC, and slaveOff. If DT frame
    lines appear but no RECORD appears, check whether the capture stopped before
    enough bytes were read for the next full record.
