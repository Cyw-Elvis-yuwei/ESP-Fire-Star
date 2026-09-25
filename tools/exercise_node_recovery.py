"""Bounded real-node outage via debug halt, followed by SYSRESETREQ; no flash writes.

Run only after the recovery-check GUI has passed its baseline query and is armed.
This is NOT a physical cable/USB-disconnect test.
"""
import argparse
import datetime
import json
import time
from pathlib import Path

from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
from backup_stm32_flash import PROBE, BASE
from program_can_baseline import validate_inputs, ROOT


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--hold-ms', type=int, default=3000)
    parser.add_argument('--reset-only', action='store_true',
                        help='Restore a node after an external outage; no deliberate 3-second halt')
    args = parser.parse_args()
    if not 1500 <= args.hold_ms <= 5000:
        raise ValueError('Hold must be 1500..5000 ms')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    result_path = out / 'fault-action.json'
    if result_path.exists():
        raise RuntimeError('Existing action evidence protected')
    info, _, _, image, _, symbols = validate_inputs(ROOT / 'artifacts/can-flash-2026-09-25')
    options = {'target_override': 'cortex_m', 'connect_mode': 'attach',
               'frequency': 100000, 'auto_unlock': False, 'resume_on_disconnect': True,
               'pack.debug_sequences.enable': False, 'cmsis_dap.limit_packets': True,
               'cache.enable_memory': False, 'cache.enable_register': False,
               'no_config': True, 'project_dir': str(out)}
    hold_ms = 0 if args.reset_only else args.hold_ms
    result = {'started_utc': utc(), 'probe_id': PROBE, 'hold_ms': hold_ms,
              'mode': 'reset_only' if args.reset_only else 'halt_then_reset',
              'interruption_kind': 'brief_identity_check' if args.reset_only else 'debug_halt', 'recovery_action': 'SYSRESETREQ',
              'physical_unplug': False, 'flashed': False, 'reset_requested': False,
              'reset_completed': False, 'success': False}
    try:
        session = ConnectHelper.session_with_chosen_probe(blocking=False, unique_id=PROBE, options=options)
        if session is None:
            raise RuntimeError('Expected DAP absent')
        with session:
            target = session.target
            before = target.get_state()
            result['state_before'] = before.name
            if before not in (Target.State.RUNNING, Target.State.SLEEPING):
                raise RuntimeError('Target was not executing or waiting for interrupts')
            target.halt()
            result['halted_utc'] = utc()
            try:
                uid = bytes(target.read_memory_block8(0x1FFFF7E8, 12)).hex()
                prefix = bytes(target.read_memory_block8(BASE, 32))
                if uid != info['identity']['uid_hex'] or prefix != image[:32]:
                    raise RuntimeError('Chip identity or reviewed vector prefix differs')
                result['identity_and_vector_prefix_match'] = True
                diag = target.read_memory_block32(symbols['canbench_diag'], 7)
                result['diagnostics_before_reset'] = dict(zip(
                    ['rx_frames', 'rx_dropped', 'rejected_frames', 'tx_completed',
                     'tx_busy_retries', 'last_hal_error', 'fault'], diag))
                if hold_ms:
                    time.sleep(hold_ms / 1000.0)
                result['reset_requested'] = True
                result['reset_requested_utc'] = utc()
                target.reset_and_halt(reset_type=Target.ResetType.SYSRESETREQ)
                result['reset_completed'] = True
            finally:
                target.resume()
                result['resumed_utc'] = utc()
            time.sleep(0.1)
            after = target.get_state()
            result['state_after'] = after.name
            result['success'] = result['reset_completed'] and after in (Target.State.RUNNING, Target.State.SLEEPING)
    except Exception as exc:
        result['error'] = str(exc)
        raise
    finally:
        result['finished_utc'] = utc()
        result_path.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(result, indent=2))
    if not result['success']:
        raise RuntimeError('Node state unexpected; inspect evidence, do not reflash automatically')


if __name__ == '__main__':
    main()
