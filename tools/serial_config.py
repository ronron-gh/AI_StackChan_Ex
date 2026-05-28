#!/usr/bin/env python3
"""Serial 経由で AI_StackChan_Ex の YAML 設定を読み書きするツール。

Usage:
    python3 tools/serial_config.py [--port PORT] get <ex|basic|sec> [output.yaml]
    python3 tools/serial_config.py [--port PORT] set <ex|basic|sec> input.yaml
    python3 tools/serial_config.py [--port PORT] restart
    python3 tools/serial_config.py [--port PORT] help

Examples:
    # 現在の ExConfig を取得して標準出力 / ファイルへ保存
    python3 tools/serial_config.py get ex
    python3 tools/serial_config.py get ex /tmp/ex.yaml

    # /tmp/ex.yaml で SC_ExConfig.yaml を上書き
    python3 tools/serial_config.py set ex /tmp/ex.yaml

事前に: pip install pyserial （または pip3 install pyserial）
"""
import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.stderr.write("ERROR: pyserial が必要です。'pip3 install pyserial' で入れてください。\n")
    sys.exit(1)


DEFAULT_PORT = "/dev/cu.usbmodem11401"
BAUD = 115200
READ_TIMEOUT = 0.5
TRANSACTION_TIMEOUT = 10.0


def open_port(port: str) -> "serial.Serial":
    ser = serial.Serial(port, BAUD, timeout=READ_TIMEOUT)
    time.sleep(0.2)
    # 受信バッファを空に
    while ser.in_waiting:
        ser.read(ser.in_waiting)
    return ser


def read_until_marker(ser, end_marker: str, deadline: float):
    """end_marker を含む行が来るまで読み続けて、行リストを返す（end_marker 行は除外）。"""
    lines = []
    buf = ""
    while time.monotonic() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf += chunk.decode("utf-8", errors="replace")
            while "\n" in buf:
                line, buf = buf.split("\n", 1)
                line = line.rstrip("\r")
                if line == end_marker:
                    return lines
                lines.append(line)
    raise TimeoutError(f"Timeout waiting for marker {end_marker!r}")


def cmd_get(ser, conf_type: str, out_path: str = None):
    ser.write(f":CONFIG GET {conf_type}\n".encode())
    deadline = time.monotonic() + TRANSACTION_TIMEOUT
    # :BEGIN <type> を待ち、その後 :END まで蓄積
    begin_seen = False
    yaml_lines = []
    buf = ""
    while time.monotonic() < deadline:
        chunk = ser.read(256)
        if not chunk:
            continue
        buf += chunk.decode("utf-8", errors="replace")
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            line = line.rstrip("\r")
            if not begin_seen:
                if line.startswith(":BEGIN "):
                    begin_seen = True
                elif line.startswith(":ERR"):
                    raise RuntimeError(f"Device returned error: {line}")
                continue
            if line == ":END":
                content = "\n".join(yaml_lines)
                if out_path:
                    with open(out_path, "w") as f:
                        f.write(content)
                    print(f"Wrote {len(content)} bytes to {out_path}")
                else:
                    sys.stdout.write(content)
                return
            yaml_lines.append(line)
    raise TimeoutError("Timeout while receiving yaml")


def cmd_set(ser, conf_type: str, in_path: str):
    with open(in_path, "r") as f:
        content = f.read()
    if "\n:END" in content or content.startswith(":END"):
        raise ValueError("input file contains :END marker, abort")
    # 1) SET コマンド送信
    ser.write(f":CONFIG SET {conf_type}\n".encode())
    # 2) :READY を待つ
    deadline = time.monotonic() + 5
    buf = ""
    ready = False
    while time.monotonic() < deadline and not ready:
        chunk = ser.read(256)
        if not chunk:
            continue
        buf += chunk.decode("utf-8", errors="replace")
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            line = line.rstrip("\r")
            if line.startswith(":READY"):
                ready = True
                break
            elif line.startswith(":ERR"):
                raise RuntimeError(f"Device returned error: {line}")
    if not ready:
        raise TimeoutError("Device did not respond :READY")
    # 3) yaml 本体を送信
    if not content.endswith("\n"):
        content += "\n"
    # 行単位で 50ms の小休止を入れて UART バッファ溢れを避ける
    for line in content.split("\n"):
        ser.write((line + "\n").encode())
        time.sleep(0.02)
    ser.write(b":END\n")
    # 4) :OK を待つ
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        chunk = ser.read(256)
        if not chunk:
            continue
        buf += chunk.decode("utf-8", errors="replace")
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            line = line.rstrip("\r")
            if line.startswith(":OK"):
                print(line)
                return
            elif line.startswith(":ERR"):
                raise RuntimeError(f"Device returned error: {line}")
    raise TimeoutError("Did not receive :OK")


def cmd_help(ser):
    ser.write(b":CONFIG HELP\n")
    time.sleep(0.5)
    while ser.in_waiting:
        sys.stdout.write(ser.read(ser.in_waiting).decode("utf-8", errors="replace"))


def cmd_restart(ser):
    ser.write(b":CONFIG RESTART\n")
    time.sleep(0.3)
    while ser.in_waiting:
        sys.stdout.write(ser.read(ser.in_waiting).decode("utf-8", errors="replace"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default=DEFAULT_PORT, help=f"serial port (default {DEFAULT_PORT})")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sg = sub.add_parser("get"); sg.add_argument("type", choices=["ex", "basic", "sec"])
    sg.add_argument("out", nargs="?")
    ss = sub.add_parser("set"); ss.add_argument("type", choices=["ex", "basic", "sec"])
    ss.add_argument("input")
    sub.add_parser("help")
    sub.add_parser("restart")
    args = ap.parse_args()
    ser = open_port(args.port)
    try:
        if args.cmd == "get":
            cmd_get(ser, args.type, args.out)
        elif args.cmd == "set":
            cmd_set(ser, args.type, args.input)
        elif args.cmd == "help":
            cmd_help(ser)
        elif args.cmd == "restart":
            cmd_restart(ser)
    finally:
        ser.close()


if __name__ == "__main__":
    main()
