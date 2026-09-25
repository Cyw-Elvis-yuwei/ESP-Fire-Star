#!/usr/bin/env python3
"""Protocol-v1 mock for a verified Linux vcan interface; never physical CAN."""
import argparse
import fcntl
import json
import os
import re
import select
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

FRAME = struct.Struct("=IB3x8s")
SCENARIOS = ("normal", "drop-response", "pause-heartbeat", "restart", "wrong-seq", "late-response")


def require_vcan(interface):
    if not re.fullmatch(r"[A-Za-z0-9_.:-]{1,15}", interface):
        raise ValueError("invalid interface name")
    result = subprocess.run(["ip", "-j", "-d", "link", "show", "dev", interface],
                            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, timeout=3)
    links = json.loads(result.stdout)
    if len(links) != 1 or links[0].get("linkinfo", {}).get("info_kind") != "vcan":
        raise ValueError("refusing interface that is not verified info_kind=vcan")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interface", default="vcan0")
    parser.add_argument("--scenario", choices=SCENARIOS, default="normal")
    args = parser.parse_args()
    require_vcan(args.interface)
    # Keep the lock inode: unlinking it would let another process lock a new inode.
    lock_path = os.path.join(tempfile.gettempdir(), "canbench-sim-{}.lock".format(args.interface))
    with open(lock_path, "a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError("a simulator already owns this interface")
        with socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW) as bus:
            bus.bind((args.interface,))
            bus.setblocking(False)
            running = [True]
            def stop(_signum, _frame):
                running[0] = False
            signal.signal(signal.SIGTERM, stop)
            signal.signal(signal.SIGINT, stop)
            started = boot = next_heartbeat = time.monotonic()
            counter, led, requests = 0, 0, 0
            restarted = False
            delayed = []
            print("READY " + json.dumps({"interface": args.interface, "scenario": args.scenario,
                                          "simulated": True, "hardware_verified": False}), flush=True)
            while running[0]:
                now = time.monotonic()
                elapsed = now - started
                if args.scenario == "restart" and elapsed >= 0.9 and not restarted:
                    boot, counter, led, restarted = now, 0, 0, True
                    print("EVENT restart", flush=True)
                if now >= next_heartbeat:
                    if not (args.scenario == "pause-heartbeat" and 0.55 <= elapsed < 1.45):
                        heartbeat = struct.pack("<BBHI", 1, led, counter, int((now - boot) * 1000) & 0xffffffff)
                        bus.send(FRAME.pack(0x123, 8, heartbeat))
                        counter = (counter + 1) & 0xffff
                    next_heartbeat = now + 0.1
                due = [item for item in delayed if item[0] <= now]
                delayed = [item for item in delayed if item[0] > now]
                for _, payload in due:
                    bus.send(FRAME.pack(0x322, 8, payload))
                readable, _, _ = select.select([bus], [], [], 0.02)
                if not readable:
                    continue
                raw = bus.recv(FRAME.size)
                if len(raw) != FRAME.size:
                    continue
                can_id, dlc, payload = FRAME.unpack(raw)
                if can_id != 0x321 or dlc != 8:
                    continue  # Flags are part of can_id, so EFF/RTR/ERR cannot pass.
                version, op, seq, arg0, arg1, arg2, arg3 = struct.unpack("<BBHBBBB", payload)
                result = 0
                if version != 1:
                    result = 3
                elif op not in (1, 2):
                    result = 1
                elif arg1 or arg2 or arg3 or (op == 1 and arg0 > 1) or (op == 2 and arg0):
                    result = 2
                elif op == 1:
                    led = arg0
                requests += 1
                reply = struct.pack("<BBHBBBB", 1, op, seq, result, led, 0, 0)
                if args.scenario == "drop-response" and requests == 1:
                    print("EVENT dropped first response", flush=True)
                    continue
                if args.scenario == "wrong-seq" and requests == 1:
                    reply = struct.pack("<BBHBBBB", 1, op, (seq + 777) & 0xffff, result, led, 0, 0)
                if args.scenario == "late-response" and requests <= 2:
                    delayed.append((time.monotonic() + (0.7 if requests == 1 else 0.35), reply))
                else:
                    bus.send(FRAME.pack(0x322, 8, reply))
            print("STOPPED", flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print("simulator error: {}".format(error), file=sys.stderr, flush=True)
        sys.exit(2)
