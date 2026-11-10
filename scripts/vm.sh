#!/bin/sh
# Development helper: a persistent headless test VM in the builder container.
#   scripts/vm.sh start [--iso out/x.iso] [--disk work/d.qcow2] [--firmware uefi] [--boot disk]
#   scripts/vm.sh shot work/s.png | key ret | type text | click X Y | exec 'cmd' | wait-agent | stop
set -eu
. "$(dirname "$0")/common.sh"
NAME=zionos-devvm
if [ "$1" = start ]; then
    shift
    docker rm -f $NAME >/dev/null 2>&1 || true
    docker run -d --rm --name $NAME --device /dev/kvm -v "$ROOT":/src -w /src \
        -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" "$BUILDER_IMAGE" \
        python3 tests/vm/vmctl.py serve "$@" >/dev/null
    echo started
else
    docker exec -e ZVM_TIMEOUT="${ZVM_TIMEOUT:-300}" $NAME python3 tests/vm/vmctl.py "$@"
fi
