"""Capture real board UART evidence for the RX sensitivity work mode."""
import argparse
import datetime
from pathlib import Path
import time
import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM22')
    parser.add_argument('--duration', type=float, default=60)
    parser.add_argument('--listen-only', action='store_true')
    args = parser.parse_args()
    folder = Path(__file__).resolve().parents[1] / 'reports' / 'rx_sensitivity'
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / (datetime.datetime.now().strftime('%Y%m%d_%H%M%S') + '.log')
    print('LOG=' + str(path), flush=True)
    with path.open('w', encoding='utf-8') as log, serial.Serial(args.port, 1000000, timeout=0.1) as port:
        def record(text):
            log.write(text)
            log.flush()
            print(text, end='', flush=True)

        def receive(seconds):
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                data = port.read(min(max(port.in_waiting, 1), 8192))
                if data:
                    record(data.decode('utf-8', errors='replace'))

        def command(text, wait=3):
            record('\n[HOST ' + datetime.datetime.now().isoformat() + '] ' + text + '\n')
            port.write((text + '\r\n').encode('ascii'))
            port.flush()
            receive(wait)

        if args.listen_only:
            receive(args.duration)
        else:
            command('AT+VER')
            command('AT+STOP')
            command('AT+SETPARAM=6,0,11776,0,0,0,0,22,69536,509100000,126,42,255,255')
            command('AT+SETMODE=7', 10)
            command('AT+STATE')
            command('AT+TM')
            receive(args.duration)
            command('AT+STATE')
            command('AT+TM')
            command('AT+FPGATM')


if __name__ == '__main__':
    main()
