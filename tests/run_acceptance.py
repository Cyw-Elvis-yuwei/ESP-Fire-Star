#!/usr/bin/env python3
"""Run bounded, simulated-only SocketCAN acceptance; never create an interface."""
import argparse
import datetime
import json
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from sim_node import require_vcan

CASES = (("normal", "normal"),
         ("response-timeout", "drop-response"),
         ("heartbeat-timeout", "pause-heartbeat"),
         ("restart", "restart"),
         ("wrong-seq", "wrong-seq"),
         ("late-response", "late-response"))


def stop_owned(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=2)


def wait_ready(process, log):
    deadline = time.monotonic() + 5
    with selectors.DefaultSelector() as selector:
        selector.register(process.stdout, selectors.EVENT_READ)
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError("simulator exited before READY")
            if not selector.select(timeout=0.1):
                continue
            line = process.stdout.readline()
            log.write(line)
            log.flush()
            if line.startswith("READY "):
                return
    raise RuntimeError("simulator did not become READY within 5 seconds")


def run_case(cli, interface, directory, case, scenario):
    directory.mkdir()
    simulator = None
    client = None
    entry = {"case": case, "scenario": scenario, "pass": False,
             "simulated": True, "hardware_verified": False, "directory": case}
    started = time.monotonic()
    interrupted = False
    with (directory / "simulator.stdout.log").open("w", encoding="utf-8") as sim_out, \
         (directory / "simulator.stderr.log").open("w", encoding="utf-8") as sim_err, \
         (directory / "cli.stdout.log").open("w", encoding="utf-8") as cli_out, \
         (directory / "cli.stderr.log").open("w", encoding="utf-8") as cli_err:
        try:
            simulator = subprocess.Popen(
                [sys.executable, "-u", str(ROOT / "tools" / "sim_node.py"),
                 "--interface", interface, "--scenario", scenario],
                stdout=subprocess.PIPE, stderr=sim_err, universal_newlines=True)
            wait_ready(simulator, sim_out)
            client = subprocess.Popen(
                [str(cli), "--interface", interface, "--case", case,
                 "--duration-ms", "3000", "--output", str(directory)],
                stdout=cli_out, stderr=cli_err)
            entry["exit_code"] = client.wait(timeout=8)
            if simulator.poll() is not None:
                raise RuntimeError("simulator exited before CLI completed")
            result_path = directory / "result.json"
            if not result_path.is_file():
                raise RuntimeError("CLI did not produce result.json")
            result = json.loads(result_path.read_text(encoding="utf-8"))
            assertions = result.get("assertions", [])
            entry["assertions"] = assertions
            entry["pass"] = (entry["exit_code"] == 0 and result.get("pass") is True and
                             result.get("case") == case and result.get("interface") == interface and
                             result.get("simulated") is True and result.get("hardware_verified") is False and
                             bool(assertions) and all(item.get("pass") is True for item in assertions) and
                             (directory / "raw.jsonl").is_file() and
                             (directory / "engine" / "report.json").is_file())
            if not entry["pass"]:
                entry["error"] = result.get("configuration_error", "CLI exit/report/assertions failed")
        except KeyboardInterrupt:
            interrupted = True
            entry["error"] = "acceptance interrupted"
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
            entry["error"] = str(error)
        finally:
            stop_owned(client)
            stop_owned(simulator)
            if simulator is not None and simulator.stdout is not None:
                sim_out.write(simulator.stdout.read())
                simulator.stdout.close()
                entry["simulator_exit_code"] = simulator.returncode
            entry["elapsed_ms"] = round((time.monotonic() - started) * 1000)
    return entry, interrupted


def save_summary(output, interface, cases, error=None):
    passed = not error and len(cases) == len(CASES) and all(case["pass"] for case in cases)
    summary = {"pass": bool(passed), "simulated": True, "hardware_verified": False,
               "interface": interface, "finished_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
               "cases": cases}
    if error:
        summary["error"] = error
    (output / "summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    lines = ["# Simulated CAN acceptance", "", "Result: **{}**".format("PASS" if passed else "FAIL"), "",
             "Interface: `{}` (verified vcan only).".format(interface), "",
             "Simulation evidence only; physical CAN and STM32 hardware are not verified.", "",
             "| Case | Result | Detail |", "|---|---|---|"]
    for case in cases:
        failures = [item["name"] for item in case.get("assertions", []) if not item.get("pass")]
        detail = case.get("error", "") or ", ".join(failures) or "Expected behavior and recovery observed"
        lines.append("| {} | {} | {} |".format(case["case"], "PASS" if case["pass"] else "FAIL", detail.replace("|", "/")))
    if error:
        lines.extend(["", "Error: " + error])
    lines.extend(["", "Fault-injection cases pass only when the expected fault is detected and a new matching GET_STATUS succeeds.", ""])
    (output / "summary.md").write_text("\n".join(lines), encoding="utf-8")
    return bool(passed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True, type=Path)
    parser.add_argument("--interface", default="vcan0")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    cli = args.cli.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error("--output must be a new or empty directory")
    output.mkdir(parents=True, exist_ok=True)
    entries = []
    error = None
    def interrupted(_signum, _frame):
        raise KeyboardInterrupt()
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    try:
        if not cli.is_file() or not os.access(str(cli), os.X_OK):
            raise ValueError("--cli is not an executable file")
        require_vcan(args.interface)
        for case, scenario in CASES:
            print("RUN {}".format(case), flush=True)
            entry, was_interrupted = run_case(cli, args.interface, output / case, case, scenario)
            entries.append(entry)
            print("{} {}".format("PASS" if entry["pass"] else "FAIL", case), flush=True)
            if was_interrupted:
                error = "acceptance interrupted; only owned child processes were stopped"
                break
    except KeyboardInterrupt:
        error = "acceptance interrupted"
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        error = str(exc)
    passed = save_summary(output, args.interface, entries, error)
    print("Summary: {}".format(output / "summary.json"), flush=True)
    return 0 if passed else 1 if entries else 2


if __name__ == "__main__":
    sys.exit(main())
