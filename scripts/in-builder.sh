#!/bin/sh
# Run an arbitrary command inside the builder container, e.g.
#   scripts/in-builder.sh qemu-img create -f qcow2 work/disk.qcow2 32G
set -eu
. "$(dirname "$0")/common.sh"
in_container --device /dev/kvm -- "$@"
