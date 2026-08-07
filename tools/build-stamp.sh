#!/bin/sh
# Emite un dato de sello del build, de forma determinista.
#
#   build-stamp.sh epoch    -> 1690000000
#   build-stamp.sh time     -> 22/07/2023-10:26:40(UTC)
#   build-stamp.sh commit   -> a1b2c3d
#
# La marca de tiempo sale, por orden de preferencia, de:
#   1. el argumento --epoch=N            (opción source_date_epoch de Meson)
#   2. la variable de entorno SOURCE_DATE_EPOCH
#   3. la fecha del último commit de git
#   4. 0, el epoch de UNIX
#
# Nunca se usa la hora actual: eso es precisamente lo que hacía irreproducible
# al build anterior, que llamaba a date(1) en siete reglas del makefile.
set -eu

field=''
forced_epoch=''
for arg in "$@"; do
    case "$arg" in
        --epoch=*) forced_epoch="${arg#--epoch=}" ;;
        *)         field="$arg" ;;
    esac
done

epoch=''
if [ -n "$forced_epoch" ]; then
    epoch="$forced_epoch"
elif [ -n "${SOURCE_DATE_EPOCH:-}" ]; then
    epoch="$SOURCE_DATE_EPOCH"
elif epoch=$(git -C "$(dirname "$0")/.." log -1 --format=%ct 2>/dev/null) && [ -n "$epoch" ]; then
    :
else
    epoch=0
fi

case "$epoch" in
    ''|*[!0-9]*) epoch=0 ;;
esac

case "$field" in
    epoch)
        printf '%s' "$epoch"
        ;;
    time)
        TZ=UTC date -u -d "@$epoch" +'%d/%m/%Y-%H:%M:%S(UTC)' 2>/dev/null \
            || TZ=UTC date -u -r "$epoch" +'%d/%m/%Y-%H:%M:%S(UTC)'
        ;;
    commit)
        git -C "$(dirname "$0")/.." rev-parse --short=10 HEAD 2>/dev/null || printf 'unavailable'
        ;;
    *)
        echo "uso: $0 [--epoch=N] {epoch|time|commit}" >&2
        exit 2
        ;;
esac
