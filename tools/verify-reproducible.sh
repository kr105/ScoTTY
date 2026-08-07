#!/usr/bin/env bash
#
# Compila el proyecto dos veces bajo condiciones deliberadamente distintas y
# comprueba que los binarios salen idénticos bit a bit.
#
# Las dos compilaciones difieren en ruta de build, directorio de trabajo, zona
# horaria, umask, TERM y $HOME. Si algo de eso se filtrase al binario —una ruta
# absoluta embebida, una marca de tiempo, un orden dependiente del sistema de
# ficheros— los hashes divergerían.
#
#   tools/verify-reproducible.sh
#
# Devuelve 0 si los seis ejecutables coinciden, 1 si alguno difiere.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cross="$root/cross/x86_64-w64-mingw32.ini"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

build_once() {
    local tag="$1"
    shift
    local mask="$1"
    shift
    local out="$work/$tag"
    mkdir -p "$out"
    (
        cd "$root"
        umask "$mask"
        env "$@" meson setup "$out/build" --cross-file "$cross" >"$out/setup.log" 2>&1
        env "$@" ninja -C "$out/build" >"$out/build.log" 2>&1
    ) || { echo "la compilación '$tag' falló; mira $out/*.log"; exit 1; }
    find "$out/build/0.76b_My_PuTTY/windows" -maxdepth 1 -name '*.exe' -print0 \
        | sort -z | xargs -0 sha256sum | sed "s#$out/build/##"
}

echo "compilación A..."
a=$(build_once a 0022 TZ=UTC LC_ALL=C TERM=dumb)

echo "compilación B (otra ruta, otra TZ, otra umask, otro entorno)..."
b=$(build_once b 0077 TZ=Pacific/Kiritimati LC_ALL=C.UTF-8 TERM=xterm-256color HOME="$work/fakehome")

echo
if [ "$a" = "$b" ]; then
    echo "REPRODUCIBLE: los seis binarios son idénticos bit a bit"
    echo
    echo "$a"
    exit 0
fi

echo "NO REPRODUCIBLE: los hashes difieren"
echo
diff <(echo "$a") <(echo "$b") || true
exit 1
