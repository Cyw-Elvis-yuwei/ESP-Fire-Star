#!/usr/bin/env bash
# One bounded, user-authorized USB replug. Only the known CANable and can0 are touched.
set -euo pipefail
serial="${CANBENCH_CANABLE_SERIAL:?Set CANBENCH_CANABLE_SERIAL to your verified adapter serial}"
mode="${1:---check}"
[[ "$mode" == '--check' || "$mode" == '--watch-replug' ]] || { echo 'Use --check or --watch-replug' >&2; exit 2; }
find_adapter() {
    local dev properties found=''
    for dev in /dev/ttyACM*; do
        [[ -c "$dev" ]] || continue
        properties="$(udevadm info -q property -n "$dev" 2>/dev/null)" || continue
        if grep -Fxq "ID_SERIAL_SHORT=$serial" <<< "$properties" && grep -Fxq 'ID_VENDOR_ID=16d0' <<< "$properties" && grep -Fxq 'ID_MODEL_ID=117e' <<< "$properties"; then
            [[ -z "$found" ]] || { echo 'Multiple matching adapters; refusing' >&2; return 2; }
            found="$dev"
        fi
    done
    [[ -n "$found" ]] || return 1
    printf '%s\n' "$found"
}
device="$(find_adapter)" || { echo 'Known CANable not present in Ubuntu' >&2; exit 2; }
[[ -r /sys/class/net/can0/type && "$(cat /sys/class/net/can0/type)" == 280 ]] || { echo 'Expected can0 absent or not CAN' >&2; exit 2; }
declare -a owned_pids=() owned_args=()
while read -r pid; do
    [[ -r "/proc/$pid/cmdline" ]] || continue
    args="$(tr '\0' ' ' < "/proc/$pid/cmdline")"
    if [[ " $args " == *" $device "* && " $args " == *' can0 '* ]]; then
        owned_pids+=("$pid"); owned_args+=("$args")
    fi
done < <(pgrep -x slcand || true)
[[ ${#owned_pids[@]} == 1 ]] || { echo 'Expected exactly one slcand for this adapter and can0' >&2; exit 2; }
echo "Checked CANable $serial at $device, can0, slcand PID ${owned_pids[0]}"
[[ "$mode" == '--watch-replug' ]] || exit 0
[[ $EUID == 0 ]] || { echo 'Recreating can0 requires sudo; no changes made' >&2; exit 2; }
echo 'READY: wait for the assistant to request unplugging CANable USB. DAP and board stay connected.'
deadline=$((SECONDS + 300))
while find_adapter >/dev/null; do
    (( SECONDS < deadline )) || { echo 'No unplug observed within 300s; no interface changes' >&2; exit 3; }
    sleep 0.2
done
echo 'USB removal observed. Waiting for the same CANable to return.'
pid="${owned_pids[0]}"
if [[ -r "/proc/$pid/cmdline" ]]; then
    args="$(tr '\0' ' ' < "/proc/$pid/cmdline")"
    [[ "$args" == "${owned_args[0]}" ]] || { echo 'Daemon changed; refusing to terminate it' >&2; exit 3; }
    kill -TERM "$pid" || { [[ ! -e "/proc/$pid" ]] || exit 3; }
fi
deadline=$((SECONDS + 300))
until device="$(find_adapter)"; do
    (( SECONDS < deadline )) || { echo 'Adapter has not returned to Ubuntu within 300s' >&2; exit 3; }
    sleep 0.2
done
# Old serial line discipline must have released its interface; never overwrite another can0.
for ((i=0;i<25;i++)); do [[ ! -e /sys/class/net/can0 ]] && break; sleep 0.2; done
[[ ! -e /sys/class/net/can0 ]] || { echo 'can0 still exists; refusing to replace an unknown interface' >&2; exit 3; }
echo "Restoring the same adapter at $device using existing 500kbps SLCAN configuration."
slcand -o -c -s6 -S 115200 "$device" can0
for ((i=0;i<25;i++)); do [[ -e /sys/class/net/can0 ]] && break; sleep 0.2; done
[[ -r /sys/class/net/can0/type && "$(cat /sys/class/net/can0/type)" == 280 ]] || { echo 'SLCAN did not create can0' >&2; exit 3; }
ip link set dev can0 up
ip -br link show can0
echo 'CAN0_READY: interface restored; application reconnect and node verification are separate.'
