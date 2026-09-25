"""Use two briefly halted snapshots, separated by execution, to verify CAN state.

No reset or flash operation. This target returns all-ones from UID while asleep.
"""
import argparse
import json
from pathlib import Path
import time
from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
from program_can_baseline import validate_inputs
from backup_stm32_flash import BASE, PROBE, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backup', required=True, type=Path)
    args = parser.parse_args()
    out = args.backup.resolve()
    info, _, _, image, _, symbols = validate_inputs(out)
    path = out / 'runtime-verification.json'
    if path.exists():
        raise RuntimeError('Existing verification protected')
    flashed = json.loads((out / 'program-result.json').read_text(encoding='utf-8'))
    if not all(flashed.get(k) for k in ('full_flash_matches_expected', 'outside_image_unchanged', 'option_bytes_unchanged')):
        raise RuntimeError('The prior full readback must have passed')
    options = {'target_override': 'cortex_m', 'connect_mode': 'attach',
               'frequency': 100000, 'auto_unlock': False,
               'resume_on_disconnect': False, 'pack.debug_sequences.enable': False,
               'cmsis_dap.limit_packets': True, 'cache.enable_memory': False,
               'cache.enable_register': False, 'no_config': True, 'project_dir': str(out)}
    session = ConnectHelper.session_with_chosen_probe(blocking=False, unique_id=PROBE, options=options)
    if session is None:
        raise RuntimeError('Expected DAP absent')
    with session:
        target = session.target
        before = target.get_state()
        if before not in (Target.State.RUNNING, Target.State.SLEEPING):
            raise RuntimeError('Target was not running or waiting for interrupts')
        target.halt()
        try:
            uid = bytes(target.read_memory_block8(0x1FFFF7E8, 12)).hex()
            if uid != info['identity']['uid_hex']:
                raise RuntimeError('Chip identity differs while halted')
            actual_image = bytes(target.read_memory_block8(BASE, len(image)))
            tick_before = target.read32(symbols['uwTick'])
        finally:
            target.resume()
        time.sleep(0.15)
        # Running/sleeping reads also returned zero for some SRAM locations on
        # this target. Capture RAM only while halted, without changing firmware.
        target.halt()
        try:
            tick_after = target.read32(symbols['uwTick'])
            diag = target.read_memory_block32(symbols['canbench_diag'], 7)
            stage = target.read32(symbols['canbench_boot_stage'])
        finally:
            target.resume()
        names = ['rx_frames', 'rx_dropped', 'rejected_frames', 'tx_completed',
                 'tx_busy_retries', 'last_hal_error', 'fault']
        after = target.get_state()
    ticks = (tick_after - tick_before) & 0xffffffff
    active = (Target.State.RUNNING, Target.State.SLEEPING)
    result = {'application_matches_reviewed': actual_image == image,
              'application_sha256': digest(actual_image), 'state_before': before.name,
              'state_after': after.name, 'tick_delta_ms': ticks, 'boot_stage': stage,
              'diagnostics': dict(zip(names, diag)),
              'reset_performed': False, 'brief_halted_snapshots': 2, 'flashed': False,
              'success': actual_image == image and before in active and after in active
                         and 50 <= ticks <= 5000 and stage == 3 and diag[6] == 0}
    path.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))
    if not result['success']:
        raise RuntimeError('Runtime verification failed; no reflash attempted')


if __name__ == '__main__':
    main()
