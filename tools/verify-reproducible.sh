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

# SOURCE_DATE_EPOCH y SOURCE_COMMIT son entradas del build, no cosas que deban
# variar entre las dos compilaciones: se fijan aquí, iguales para ambas.
#
# Antes se dejaban al ambiente y la compilación B, que usa otro HOME, perdía el
# acceso a git —safe.directory vive en $HOME/.gitconfig— y caía a los valores
# por defecto. Los binarios diferían por eso y no por un problema real.
export SOURCE_DATE_EPOCH="$("$root/tools/build-stamp.sh" epoch)"
export SOURCE_COMMIT="$("$root/tools/build-stamp.sh" commit)"
echo "entradas fijadas: SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH SOURCE_COMMIT=$SOURCE_COMMIT"
echo

echo "compilación A..."
a=$(build_once a 0022 TZ=UTC LC_ALL=C TERM=dumb \
      SOURCE_DATE_EPOCH="$SOURCE_DATE_EPOCH" SOURCE_COMMIT="$SOURCE_COMMIT")

echo "compilación B (otra ruta, otra TZ, otra umask, otro entorno)..."
b=$(build_once b 0077 TZ=Pacific/Kiritimati LC_ALL=C.UTF-8 TERM=xterm-256color \
      HOME="$work/fakehome" \
      SOURCE_DATE_EPOCH="$SOURCE_DATE_EPOCH" SOURCE_COMMIT="$SOURCE_COMMIT")

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

# Un .exe distinto no dice de dónde viene la diferencia. Se baja al nivel de
# objeto para señalar el origen: si los que difieren están todos bajo
# subprojects/, el problema son las banderas que Meson no propaga a los
# subproyectos; si están repartidos, es algo global del proyecto.
echo
echo "--- objetos que difieren entre las dos compilaciones ---"
differing=0
while IFS= read -r rel; do
    if ! cmp -s "$work/a/build/$rel" "$work/b/build/$rel"; then
        differing=$((differing + 1))
        [ "$differing" -le 25 ] && echo "  $rel"
    fi
done < <(cd "$work/a/build" && find . -name '*.obj' -o -name '*.a' | sed 's#^\./##' | sort)

if [ "$differing" -eq 0 ]; then
    echo "  ninguno: los objetos coinciden, la diferencia la introduce el enlazado"
else
    echo "  ($differing objetos distintos en total)"
    echo
    echo "--- reparto ---"
    printf '  bajo subprojects/ : %s\n' \
        "$(cd "$work/a/build" && find . -name '*.obj' | sed 's#^\./##' | while IFS= read -r r; do
             cmp -s "$work/a/build/$r" "$work/b/build/$r" || echo "$r"; done | grep -c '^subprojects/' || true)"
    printf '  resto del proyecto: %s\n' \
        "$(cd "$work/a/build" && find . -name '*.obj' | sed 's#^\./##' | while IFS= read -r r; do
             cmp -s "$work/a/build/$r" "$work/b/build/$r" || echo "$r"; done | grep -vc '^subprojects/' || true)"
fi
exit 1
