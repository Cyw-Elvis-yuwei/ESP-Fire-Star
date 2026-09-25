#!/usr/bin/env python3
"""Run the GUI with one owned virtual node; keep physical CAN out of the demo."""
import argparse
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import time

from sim_node import require_vcan


def stop_process(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--interface', default='vcan0')
    parser.add_argument('--capture', type=Path, help='Save an offscreen GUI capture and exit')
    parser.add_argument('--quit-after', type=int, default=2200)
    args = parser.parse_args()
    require_vcan(args.interface)
    root = Path(__file__).resolve().parent.parent
    app = root / 'build-app' / 'can-diagnostic-bench'
    if not app.is_file():
        parser.error('Run bash tools/build.sh first')
    if args.capture:
        if args.capture.exists():
            parser.error('Capture already exists; choose a new filename')
        if args.quit_after < 500:
            parser.error('--quit-after must be at least 500 ms')
        args.capture.parent.mkdir(parents=True, exist_ok=True)
    node = gui = None
    try:
        node = subprocess.Popen([sys.executable, str(root / 'tools' / 'sim_node.py'),
                                 '--interface', args.interface, '--scenario', 'normal'],
                                stdout=subprocess.PIPE, stderr=None, universal_newlines=True,
                                bufsize=1)
        with selectors.DefaultSelector() as selector:
            selector.register(node.stdout, selectors.EVENT_READ)
            deadline = time.monotonic() + 5
            ready = False
            while time.monotonic() < deadline:
                for key, _ in selector.select(timeout=0.2):
                    line = key.fileobj.readline()
                    if line:
                        print(line.rstrip(), flush=True)
                    if line.startswith('READY '):
                        ready = True
                        break
                if ready or node.poll() is not None:
                    break
            if not ready:
                raise RuntimeError('Virtual node did not become ready; no GUI session started')
        command = [str(app), '--interface', args.interface]
        env = os.environ.copy()
        if args.capture:
            env['QT_QPA_PLATFORM'] = 'offscreen'
            command += ['--capture', str(args.capture.resolve()), '--quit-after', str(args.quit_after)]
        gui = subprocess.Popen(command, env=env)
        return gui.wait()
    finally:
        stop_process(gui)
        stop_process(node)


if __name__ == '__main__':
    def interrupted(_signum, _frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupted)
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print('demo error: {}'.format(error), file=sys.stderr)
        sys.exit(2)
