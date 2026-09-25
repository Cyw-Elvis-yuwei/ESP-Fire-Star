"""Program only the reviewed CAN application sectors after a verified backup.

Reuses the proven LED programming sequence; the LED tool/guards remain intact.
No whole-chip erase, option-byte write, auto-unlock, or automatic recovery retry.
"""
import argparse
import datetime
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
PACK = Path(r'C:\Keil_v5\ARM\Pack\Keil\STM32F1xx_DFP\2.4.1')
REVIEWED_SHA256 = '231407bd9f23191dc61f92862b31c3241791c1bdfcd7c4599b46c537a6503b1e'
IMAGE_BYTES = 7044
ERASE_BYTES = 8192


def validate_inputs(out):
    info = json.loads((out / 'backup.json').read_text(encoding='utf-8'))
    original = (out / 'original-flash.bin').read_bytes()
    option_bytes = (out / 'option-bytes-before.bin').read_bytes()
    if (len(original) != SIZE or len(option_bytes) != 16
            or info['probe_id'] != PROBE or info['flash_start'] != BASE
            or info['flash_bytes'] != SIZE or not info['two_complete_reads_equal']
            or digest(original) != info['backup_sha256']
            or digest(option_bytes) != info['option_bytes_sha256']):
        raise RuntimeError('Invalid backup; no device write permitted')
    build = ROOT / 'firmware/stm32-can/build'
    image = (build / 'canbench.bin').read_bytes()
    if len(image) != IMAGE_BYTES or digest(image) != REVIEWED_SHA256:
        raise RuntimeError('CAN image is not the reviewed candidate')
    hashes = json.loads((build / 'artifact-sha256.json').read_text(encoding='utf-8-sig'))
    if {x['File'] for x in hashes} != {'canbench.bin', 'canbench.hex', 'canbench.axf'}:
        raise RuntimeError('Artifact manifest must contain exactly the reviewed image types')
    for entry in hashes:
        blob = (build / entry['File']).read_bytes()
        if len(blob) != entry['Bytes'] or digest(blob) != entry['SHA256']:
            raise RuntimeError('Artifact hash mismatch: ' + entry['File'])
    ihex = IntelHex(str(build / 'canbench.hex'))
    if ihex.segments() != [(BASE, BASE + IMAGE_BYTES)] or bytes(ihex.tobinarray()) != image:
        raise RuntimeError('HEX address/data mismatch')
    sp, pc = struct.unpack_from('<II', image)
    if not (0x20000000 < sp <= 0x20010000 and sp % 8 == 0
            and pc & 1 and BASE <= (pc & ~1) < BASE + len(image)):
        raise RuntimeError('Invalid vector table')
    with (build / 'canbench.axf').open('rb') as stream:
        elf = ELFFile(stream)
        symbols = {s.name: int(s['st_value'])
                   for s in elf.get_section_by_name('.symtab').iter_symbols()}
    for name in ('canbench_boot_stage', 'canbench_diag'):
        size = 4 if name == 'canbench_boot_stage' else 28
        address = symbols.get(name, 0)
        if not (0x20000000 <= address <= 0x20010000 - size and address % 4 == 0):
            raise RuntimeError('Missing/invalid RAM diagnostic symbol: ' + name)
    expected = image + original[len(image):]
    return info, original, option_bytes, image, expected, symbols


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backup', required=True, type=Path)
    parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    out = args.backup.resolve()
    info, original, option_bytes, image, expected, symbols = validate_inputs(out)
    if not args.execute:
        print(json.dumps({'preflight_valid': True, 'device_opened': False,
                          'image_sha256': digest(image), 'image_bytes': len(image),
                          'erase_start': hex(BASE), 'erase_bytes': ERASE_BYTES,
                          'preserved_tail_in_last_sector': ERASE_BYTES - len(image)}, indent=2))
        return
    result_path = out / 'program-result.json'
    page_path = out / 'application-sectors.bin'
    if result_path.exists() or page_path.exists():
        raise RuntimeError('Existing programming evidence protected; do not retry automatically')
    page_path.write_bytes(expected[:ERASE_BYTES])
    options = {'target_override': 'stm32f103ze', 'pack': str(PACK),
               'connect_mode': 'halt', 'frequency': 1000000,
               'auto_unlock': False, 'resume_on_disconnect': False,
               'pack.debug_sequences.enable': True, 'cache.enable_memory': False,
               'cmsis_dap.limit_packets': True,
               'cache.enable_register': False, 'cache.read_code_from_elf': False,
               'chip_erase': 'sector', 'keep_unwritten': True,
               'smart_flash': False, 'fast_program': False,
               'no_config': True, 'project_dir': str(out)}
    record = {'started_at': datetime.datetime.now().astimezone().isoformat(),
              'probe_id': PROBE, 'target': 'stm32f103ze', 'options': options,
              'backup_sha256': digest(original), 'application_sha256': digest(image),
              'application_bytes': len(image), 'expected_full_flash_sha256': digest(expected),
              'erase_start': BASE, 'erase_bytes': ERASE_BYTES,
              'phase': 'preflight', 'success': False, 'physical_can_verified': False}

    def save():
        result_path.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')

    session = ConnectHelper.session_with_chosen_probe(blocking=False, unique_id=PROBE, options=options)
    if session is None:
        raise RuntimeError('Expected DAP not found')
    try:
        with session:
            target = session.target
            identity = {'dbgmcu_idcode': target.read32(0xE0042000),
                        'cpuid': target.read32(0xE000ED00),
                        'flash_kb': target.read16(0x1FFFF7E0),
                        'uid_hex': bytes(target.read_memory_block8(0x1FFFF7E8, 12)).hex()}
            if identity != info['identity']:
                raise RuntimeError('Device identity differs from backup')
            # Explicit reset is needed by FileProgrammer API to clear old IRQ state.
            target.reset_and_halt(Target.ResetType.SYSRESETREQ)
            if target.get_state() != Target.State.HALTED:
                raise RuntimeError('Could not reset and halt target')
            region = target.get_memory_map().get_region_for_address(BASE)
            if (not region.is_flash or region.start != BASE or region.length != SIZE
                    or not region.are_erased_sectors_readable):
                raise RuntimeError('Unexpected memory map')
            for offset in range(0, ERASE_BYTES, 2048):
                if (region.flash.get_sector_info(BASE + offset).size != 2048
                        or region.flash.get_page_info(BASE + offset).size != 1024):
                    raise RuntimeError('Unexpected flash geometry')
            if read_flash(target, 'Pre-write compare') != original:
                raise RuntimeError('Flash changed since backup; no write performed')
            if bytes(target.read_memory_block8(0x1FFFF800, 16)) != option_bytes:
                raise RuntimeError('Option bytes changed since backup')
            record['phase'] = 'programming'
            save()
            print('PROGRAM: first four 2KB sectors only, preserving bytes outside 7044-byte application', flush=True)
            FileProgrammer(session, chip_erase='sector', smart_flash=False,
                           trust_crc=False, keep_unwritten=True).program(
                               str(page_path), file_format='bin', base_address=BASE)
            record['phase'] = 'readback'
            save()
            target.halt()
            actual = read_flash(target, 'Full readback')
            actual_options = bytes(target.read_memory_block8(0x1FFFF800, 16))
            (out / 'flash-after.bin').write_bytes(actual)
            record['readback_sha256'] = digest(actual)
            record['full_flash_matches_expected'] = actual == expected
            record['outside_image_unchanged'] = actual[len(image):] == original[len(image):]
            record['option_bytes_unchanged'] = actual_options == option_bytes
            if actual != expected or actual_options != option_bytes:
                raise RuntimeError('Readback mismatch; target left halted')
            record['phase'] = 'reset_and_observe'
            save()
            target.reset(Target.ResetType.SYSRESETREQ)
            time.sleep(0.3)
            before_state = target.get_state()
            was_running = before_state in (Target.State.RUNNING, Target.State.SLEEPING)
            target.halt()
            try:
                diag = target.read_memory_block32(symbols['canbench_diag'], 7)
                names = ['rx_frames', 'rx_dropped', 'rejected_frames', 'tx_completed',
                         'tx_busy_retries', 'last_hal_error', 'fault']
                runtime = {'was_running': was_running,
                           'state_before_check': before_state.name,
                           'boot_stage': target.read32(symbols['canbench_boot_stage']),
                           'pc': int(target.read_core_register('pc')),
                           'diagnostics': dict(zip(names, diag)),
                           'rcc_cfgr': target.read32(0x40021004),
                           'gpiob_odr': target.read32(0x40010C0C),
                           'can_mcr': target.read32(0x40006400),
                           'can_msr': target.read32(0x40006404),
                           'can_esr': target.read32(0x40006418),
                           'can_btr': target.read32(0x4000641C)}
            finally:
                target.resume()
            after_state = target.get_state()
            runtime['state_after_check'] = after_state.name
            runtime['running_after_check'] = after_state in (Target.State.RUNNING, Target.State.SLEEPING)
            record['runtime'] = runtime
            record['success'] = (was_running and runtime['running_after_check']
                                 and runtime['boot_stage'] == 3 and diag[6] == 0)
            record['phase'] = 'complete' if record['success'] else 'runtime_check_failed'
            save()
            print(json.dumps(record, indent=2), flush=True)
            if not record['success']:
                raise RuntimeError('Flash verified; runtime needs diagnosis, do not automatically reflash')
    except BaseException as error:
        record['error'] = str(error)
        save()
        raise


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
    main()
