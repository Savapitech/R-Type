#!/usr/bin/env bash
set -u

if [ -z "${RTYPE_PLAY_NS:-}" ]; then
    exec env RTYPE_PLAY_NS=1 unshare -rn bash "$0" "$@"
fi

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
export TC=$(command -v tc || echo /usr/sbin/tc)
IP=$(command -v ip || echo /usr/sbin/ip)
LOG=$(mktemp /tmp/rtype-play-server.XXXXXX)

netem() {
    "$TC" qdisc replace dev lo root netem "$@" && "$TC" qdisc show dev lo
}
netem_off() {
    "$TC" qdisc del dev lo root 2>/dev/null
    echo "no degradation"
}
export -f netem netem_off

"$IP" link set lo up
[ $# -gt 0 ] && netem "$@"

cd "$ROOT" || exit 1
"$ROOT/bin/r-type_server" -p 4242 >"$LOG" 2>&1 &
SERVER=$!
trap 'kill "$SERVER" 2>/dev/null; wait "$SERVER" 2>/dev/null' EXIT
sleep 1

cat <<HELP
Server running on 127.0.0.1:4242 (log: $LOG).
Start a client with:  bin/r-type_client &
Change the network:   netem delay 100ms | netem loss 10% | netem_off
HELP
PS1='[rtype-netem] \w$ ' bash --norc -i
