"""Back up this project's STM32F103ZE application flash; never program or erase."""
import argparse
import os
import datetime
import hashlib
import json
from pathlib import Path
import struct
import time

from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target

BASE = 0x08000000
SIZE = 0x80000
PROBE = os.environ.get("CANBENCH_DAP_ID", "UNCONFIGURED_DAP")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_flash(target, label):
    output = bytearray()
    for offset in range(0, SIZE, 0x4000):
        chunk = target.read_memory_block8(BASE + offset, 0x4000)
        if len(chunk) != 0x4000:
            raise RuntimeError("Incomplete flash read")
        output.extend(chunk)
        if len(output) % 0x10000 == 0:
            print(f"{label}: {len(output) // 1024}/512 KB", flush=True)
    return bytes(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--limit-packets", action="store_true",
                        help="Limit CMSIS-DAP to one outstanding packet for probe compatibility")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    for name in ("original-flash.bin", "option-bytes-before.bin", "backup.json"):
        if (out / name).exists():
            raise RuntimeError("Existing backup protected: " + name)
    options = {"target_override": "cortex_m", "connect_mode": "attach",
               "frequency": 1000000, "auto_unlock": False, "resume_on_disconnect": False,
               "pack.debug_sequences.enable": False, "cache.enable_memory": False,
               "cmsis_dap.limit_packets": args.limit_packets,
               "no_config": True, "project_dir": str(out)}
    session = ConnectHelper.session_with_chosen_probe(blocking=False, unique_id=PROBE, options=options)
    if session is None:
        raise RuntimeError("Expected probe not found")
    with session:
        target = session.target
        identity = {"dbgmcu_idcode": target.read32(0xE0042000),
                    "cpuid": target.read32(0xE000ED00),
                    "flash_kb": target.read16(0x1FFFF7E0),
                    "uid_hex": bytes(target.read_memory_block8(0x1FFFF7E8, 12)).hex()}
        if (identity["dbgmcu_idcode"] & 0xfff) != 0x414 or identity["flash_kb"] != 512:
            raise RuntimeError("Target identity does not match the confirmed 512 KB high-density STM32F1")
        previous = target.get_state()
        target.halt()
        deadline = time.monotonic() + 2
        while target.get_state() != Target.State.HALTED:
            if time.monotonic() > deadline:
                raise RuntimeError("Could not halt target for a consistent backup")
            time.sleep(0.02)
        try:
            original = read_flash(target, "Backup")
            verification = read_flash(target, "Backup verification")
            if original != verification:
                raise RuntimeError("The two complete flash reads differ; no backup is accepted")
            sp, pc = struct.unpack_from("<II", original)
            if not (0x20000000 < sp <= 0x20010000 and sp % 4 == 0 and pc & 1
                    and BASE <= (pc & ~1) < BASE + SIZE):
                raise RuntimeError("Existing vector table does not look like the expected application")
            option_bytes = bytes(target.read_memory_block8(0x1FFFF800, 16))
            with (out / "original-flash.bin").open("xb") as file:
                file.write(original)
            with (out / "option-bytes-before.bin").open("xb") as file:
                file.write(option_bytes)
            if digest((out / "original-flash.bin").read_bytes()) != digest(original):
                raise RuntimeError("Saved backup does not match the verified flash read")
            record = {"captured_at": datetime.datetime.now().astimezone().isoformat(),
                      "probe_id": PROBE, "identity": identity, "flash_start": BASE,
                      "flash_bytes": SIZE, "two_complete_reads_equal": True,
                      "backup_sha256": digest(original), "option_bytes_sha256": digest(option_bytes),
                      "initial_sp": hex(sp), "reset_vector": hex(pc),
                      "state_before": previous.name, "programmed": False, "erased": False}
            record["probe_options"] = options
            with (out / "backup.json").open("x", encoding="utf-8") as file:
                json.dump(record, file, ensure_ascii=False, indent=2)
                file.write("\n")
            print("BACKUP VERIFIED: " + digest(original), flush=True)
        finally:
            if previous == Target.State.RUNNING:
                target.resume()


if __name__ == "__main__":
    main()
