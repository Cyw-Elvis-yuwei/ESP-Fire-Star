"""Program the reviewed LED image after a verified backup, preserving other flash bytes."""
import argparse
import datetime
import hashlib
import json
import logging
from pathlib import Path
import struct
import time

from elftools.elf.elffile import ELFFile
from intelhex import IntelHex
from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
from pyocd.flash.file_programmer import FileProgrammer

from backup_stm32_flash import BASE, SIZE, PROBE, digest, read_flash

ROOT = Path(__file__).resolve().parents[1]
PACK = Path(r"C:\Keil_v5\ARM\Pack\Keil\STM32F1xx_DFP\2.4.1")
REVIEWED_BIN_SHA256 = "d659ad7df99108cf90f1b5005c441315b53cfa3c67c3b10e37ebaa704f22716a"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backup", required=True, type=Path)
    parser.add_argument("--execute", action="store_true", help="Explicitly perform the application write")
    parser.add_argument("--recover-page0", action="store_true", help="Resume the documented interrupted first-page write from the trusted backup")
    args = parser.parse_args()
    if not args.execute:
        parser.error("Application writing requires --execute")
    out = args.backup.resolve()
    result_path = out / "program-result.json"
    if result_path.exists():
        raise RuntimeError("A program result already exists; do not overwrite or rerun automatically")
    backup_info = json.loads((out / "backup.json").read_text(encoding="utf-8"))
    original = (out / "original-flash.bin").read_bytes()
    original_options = (out / "option-bytes-before.bin").read_bytes()
    if (len(original) != SIZE or not backup_info["two_complete_reads_equal"]
            or digest(original) != backup_info["backup_sha256"]
            or digest(original_options) != backup_info["option_bytes_sha256"]):
        raise RuntimeError("Backup validation failed")
    build = ROOT / "firmware" / "led-baseline" / "build"
    image = (build / "led.bin").read_bytes()
    if len(image) != 692 or digest(image) != REVIEWED_BIN_SHA256:
        raise RuntimeError("LED artifact differs from the reviewed image")
    hashes = json.loads((build / "artifact-sha256.json").read_text(encoding="utf-8-sig"))
    for entry in hashes:
        path = build / Path(entry["Path"]).name
        if digest(path.read_bytes()) != entry["Hash"].lower():
            raise RuntimeError("Artifact hash mismatch: " + path.name)
    hex_image = IntelHex(str(build / "led.hex"))
    if (hex_image.segments() != [(BASE, BASE + len(image))]
            or bytes(hex_image.tobinarray()) != image):
        raise RuntimeError("HEX addresses or data differ from the reviewed application")
    initial_sp, reset_vector = struct.unpack_from("<II", image)
    if not (0x20000000 < initial_sp <= 0x20010000 and reset_vector & 1
            and BASE <= (reset_vector & ~1) < BASE + len(image)):
        raise RuntimeError("Application vector table is invalid")
    with (build / "led.axf").open("rb") as stream:
        symbols = ELFFile(stream).get_section_by_name(".symtab").get_symbol_by_name("main")
        if not symbols or not symbols[0]["st_size"]:
            raise RuntimeError("Cannot establish main() address range")
        main_start = int(symbols[0]["st_value"]) & ~1
        main_end = main_start + int(symbols[0]["st_size"])
    expected = image + original[len(image):]
    page_image = out / "page0-with-led.bin"
    if page_image.exists() and page_image.read_bytes() != expected[:2048]:
        raise RuntimeError("Existing recovery page differs from the trusted backup plus LED image")
    page_image.write_bytes(expected[:2048])
    options = {"target_override": "stm32f103ze", "pack": str(PACK),
               "connect_mode": "halt", "frequency": 1000000,
               "auto_unlock": False, "resume_on_disconnect": False,
               # This reviewed pack needs DebugPortSetup/Start for SWD handshake.
               # Its DebugDeviceUnlock override only calls read-only CheckID.
               "pack.debug_sequences.enable": True, "cache.enable_memory": False,
               "cache.enable_register": False, "cache.read_code_from_elf": False,
               "chip_erase": "sector", "keep_unwritten": True,
               "smart_flash": False, "fast_program": False,
               "no_config": True, "project_dir": str(out)}
    record = {"started_at": datetime.datetime.now().astimezone().isoformat(),
              "probe_id": PROBE, "target": "stm32f103ze", "options": options,
              "backup_sha256": digest(original), "application_sha256": digest(image),
              "application_bytes": len(image), "expected_full_flash_sha256": digest(expected),
              "first_page_sha256": digest(expected[:2048]), "recover_page0": args.recover_page0,
              "physical_led_confirmed": False, "phase": "preflight", "success": False}
    def save():
        result_path.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    session = ConnectHelper.session_with_chosen_probe(blocking=False, unique_id=PROBE, options=options)
    if session is None:
        raise RuntimeError("Expected probe not found")
    try:
        with session:
            target = session.target
            identity = {"dbgmcu_idcode": target.read32(0xE0042000),
                        "cpuid": target.read32(0xE000ED00),
                        "flash_kb": target.read16(0x1FFFF7E0),
                        "uid_hex": bytes(target.read_memory_block8(0x1FFFF7E8, 12)).hex()}
            if identity != backup_info["identity"]:
                raise RuntimeError("Connected chip does not match the backup")
            # FileProgrammer API does not apply the CLI's load.pre_reset option.
            # Clear the old application's SysTick/NVIC state before running a RAM algorithm.
            target.reset_and_halt(Target.ResetType.SYSRESETREQ)
            if target.get_state() != Target.State.HALTED:
                raise RuntimeError("Target is not halted")
            region = target.get_memory_map().get_region_for_address(BASE)
            if (not region.is_flash or region.start != BASE or region.length != SIZE
                    or not region.are_erased_sectors_readable):
                raise RuntimeError("Unexpected application flash region")
            sector = region.flash.get_sector_info(BASE)
            page = region.flash.get_page_info(BASE)
            if sector.size != 2048 or page.size != 1024:
                raise RuntimeError("Flash algorithm geometry differs from the reviewed pack")
            record["erase_region"] = {"start": BASE, "bytes": sector.size, "program_page_bytes": page.size}
            before_write = read_flash(target, "Before-write comparison")
            if args.recover_page0:
                if before_write[2048:] != original[2048:]:
                    raise RuntimeError("Flash outside the authorized first page differs from backup")
            elif before_write != original:
                raise RuntimeError("Flash has changed since backup; no write performed")
            if bytes(target.read_memory_block8(0x1FFFF800, 16)) != original_options:
                raise RuntimeError("Option bytes have changed since backup")
            record["phase"] = "programming"
            save()
            print("PROGRAM: first 2KB page from trusted backup plus 692-byte LED image; sector erase only", flush=True)
            FileProgrammer(session, chip_erase="sector", smart_flash=False,
                           trust_crc=False, keep_unwritten=True).program(str(page_image), file_format="bin", base_address=BASE)
            record["phase"] = "readback"
            save()
            target.halt()
            actual = read_flash(target, "After-write verification")
            after_options = bytes(target.read_memory_block8(0x1FFFF800, 16))
            record["readback_sha256"] = digest(actual)
            record["full_flash_matches_expected"] = actual == expected
            record["bytes_outside_image_unchanged"] = actual[len(image):] == original[len(image):]
            record["option_bytes_unchanged"] = after_options == original_options
            (out / "flash-after.bin").write_bytes(actual)
            if actual != expected or after_options != original_options:
                raise RuntimeError("Readback verification failed; target left halted")
            record["phase"] = "reset_and_observe"
            save()
            target.reset(Target.ResetType.SYSRESETREQ)
            time.sleep(0.3)
            was_running = target.get_state() == Target.State.RUNNING
            target.halt()
            runtime = {"was_running": was_running,
                       "pc": int(target.read_core_register("pc")),
                       "rcc_apb2enr": target.read32(0x40021018),
                       "gpiob_crl": target.read32(0x40010C00),
                       "gpiob_odr": target.read32(0x40010C0C)}
            runtime["pc_in_main"] = main_start <= (runtime["pc"] & ~1) < main_end
            runtime["pb0_output_low"] = bool(runtime["rcc_apb2enr"] & 8) and (runtime["gpiob_crl"] & 15) == 1 and not (runtime["gpiob_odr"] & 1)
            target.resume()
            runtime["running_after_check"] = target.get_state() == Target.State.RUNNING
            record["runtime"] = runtime
            record["success"] = all(runtime[k] for k in ("was_running", "pc_in_main", "pb0_output_low", "running_after_check"))
            record["phase"] = "complete" if record["success"] else "runtime_check_failed"
            save()
            print(json.dumps({"full_flash_matches_expected": record["full_flash_matches_expected"],
                              "option_bytes_unchanged": record["option_bytes_unchanged"],
                              "runtime": runtime, "physical_led_confirmed": False}, indent=2), flush=True)
            if not record["success"]:
                raise RuntimeError("Flash bytes verified but runtime did not match the LED baseline")
            print("PROGRAM AND READBACK VERIFIED; waiting for user LED observation", flush=True)
    except BaseException as error:
        record["error"] = str(error)
        save()
        raise


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
    main()
