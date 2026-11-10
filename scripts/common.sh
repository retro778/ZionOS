# Shared helpers for ZionOS build scripts (POSIX sh; source, do not execute).
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ZIONOS_VERSION=$(cat "$ROOT/VERSION")
ZIONOS_CODENAME="Aleph"
DEBIAN_SUITE="trixie"
BUILDER_IMAGE="${ZIONOS_BUILDER_IMAGE:-zionos-builder:trixie}"
export ROOT ZIONOS_VERSION ZIONOS_CODENAME DEBIAN_SUITE BUILDER_IMAGE

die() { echo "error: $*" >&2; exit 1; }

# in_container [docker-run-options --] command...
# Runs a command inside the builder image with the repository at /src.
# Nothing is installed on, or changed in, the host outside $ROOT.
in_container() {
    # Everything before a literal "--" is passed to `docker run` as options
    # (option values must not contain whitespace).
    _opts=""
    case " $* " in
        *" -- "*)
            while [ "$1" != "--" ]; do _opts="$_opts $1"; shift; done
            shift ;;
    esac
    _tty=""; [ -t 0 ] && [ -t 1 ] && _tty="-t"
    # shellcheck disable=SC2086
    docker run --rm -i $_tty $_opts \
        -v "$ROOT":/src -w /src \
        -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
        -e ZIONOS_VERSION="$ZIONOS_VERSION" -e ZIONOS_CODENAME="$ZIONOS_CODENAME" \
        ${SOURCE_DATE_EPOCH:+-e SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH} \
        "$BUILDER_IMAGE" "$@"
}

inside_container() { [ -f /.dockerenv ]; }
