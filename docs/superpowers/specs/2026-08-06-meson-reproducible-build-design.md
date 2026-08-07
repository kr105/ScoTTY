# Build system Meson con cross-compilación reproducible

**Fecha:** 2026-08-06
**Estado:** aprobado
**Reemplaza:** `0.76b_My_PuTTY/windows/MAKEFILE.MINGW` (2443 líneas)

## Problema

El build actual solo funciona dentro de una imagen Docker de terceros sin pinnear
(`cyd01/cross-gcc`) y depende de una colección de hacks:

| Hack | Efecto |
|---|---|
| `sed -i` sobre `window.c` en el target `cross` | Muta fuente rastreada; no idempotente |
| `sed -i 's/0/999/' version_major.txt` | Muta fichero rastreado en cada build |
| `cross64` hace `cp mini_64.a mini.a` (y bcrypt, base64) | Pisa las libs de 32 bits con las de 64 |
| `make putty.exe \|\| gcc -o kitty64.exe *.o …` | El `\|\|` enmascara fallos de link |
| `mv … /builds/` | Ruta absoluta; solo existe dentro del contenedor |
| `-DMASTER_PASSWORD=$(cat …)` con backticks | Shell embebido en CFLAGS, en 10 reglas |
| `-DBUILD_TIME=$(date …)` en 7 reglas | Rompe la reproducibilidad por diseño |
| 290 reglas `.o` con dependencias de headers a mano | Ya desincronizadas: referencian `kitty_help.h`, `kitty_ini.h`, `kitty_iv.c` y ~65 fuentes de `unix/` que no existen |
| `nohyperlink:`, `zmodem:` | Apuntan a reglas inexistentes |

Además se enlazan 7 archivos `.a` precompilados y opacos. Dos de ellos
(`regex/libregex.a`, `jpeg/libjpeg.a`) fueron compilados en 2016-2017 por un uid
desconocido y **no tienen fuentes en el repo**.

## Objetivo

Un build que cualquiera pueda ejecutar desde WSL/Linux y que produzca binarios
**idénticos bit a bit** a los que publica el CI, para que la ausencia de
manipulación sea verificable y no una promesa.

## Decisiones tomadas

| Decisión | Elección | Motivo |
|---|---|---|
| Build system | **Meson + cross file** | Cross-compilación de primera clase; legible y auditable |
| Arquitecturas | **Solo x86_64** | Elimina la clase entera de bugs de `cross64` pisando los `.a` de 32 |
| Artefactos | 6: `kitty`, `klink`, `kscp`, `ksftp`, `kageant`, `kittygen` | Se caen los targets muertos y la variante portable |
| Compresión | **Ninguna** | UPX dispara falsos positivos de AV y estorba a la auditoría |
| Libs externas | **Todo desde fuente**, pinneado por SHA256 | Sin blobs opacos en el binario final |
| `bcrypt.o` | **Reimplementar**, sin compatibilidad de formato | Su fuente no existe ni públicamente |

## Arquitectura

```
meson.build                          proyecto, versión, flags MOD_*, reproducibilidad
meson.options                        opciones del build
cross/x86_64-w64-mingw32.ini         toolchain de cross-compilación
subprojects/
  pcre2.wrap                         POSIX regcomp/regexec        ┐ pinneados
  libjpeg-turbo.wrap                 imágenes de fondo            ┘ por sha256
base64/meson.build                   ┐
bcrypt/meson.build                   │ las libs auxiliares del repo,
blocnote/meson.build                 │ cada una declarando sus fuentes
md5/meson.build                      │
mini/meson.build                     ┘
0.76b_My_PuTTY/meson.build           núcleo PuTTY compartido
0.76b_My_PuTTY/windows/meson.build   plataforma Windows + los 6 ejecutables
tools/verify-reproducible.sh         build x2 en distinto entorno, compara hashes
.github/workflows/build.yml          CI
```

Cada directorio declara sus propias fuentes. No hay listas centralizadas.

### Grafo de build (extraído, no adivinado)

Ejecutando `make -n` sobre los 6 targets del makefile viejo y parseando la salida:

- **183 fuentes `.c` únicas**, todas presentes en disco
- **188 objetos únicos**; ningún objeto sin regla
- **31 defines base** compartidos por todo
- **6 ficheros `.rc`**, uno por binario

Reparto por directorio: 166 en `0.76b_My_PuTTY/`, 12 en la raíz (`kitty*.c`),
2 en `far2l/`, 1 en cada uno de `url/`, `adb/`, `zmodem/`.

### Objetos con flags propios

De los 188 objetos, solo 13 se compilan con flags distintos al conjunto base.
Siete de ellos lo son únicamente por `-DMASTER_PASSWORD` o `-DBUILD_*`, que pasan
a ser defines globales. Quedan **cinco variantes reales**:

| Objeto | Fuente | Diferencia |
|---|---|---|
| `be_all_s_plink.o` | `be_all_s.c` | Conjunto reducido `CFLAGS_PLINK` + `NO_MOD_RUTTY` |
| `ldisc_plink.o` | `ldisc.c` | Conjunto reducido `CFLAGS_PLINK` + `NO_MOD_RUTTY` |
| `pageant_integrated.o` | `pageant.c` | `+MOD_INTEGRATED_AGENT` |
| `winpageant_integrated.o` | `windows/pageant.c` | `+MOD_INTEGRATED_AGENT` |
| `winputtygen_integrated.o` | `windows/puttygen.c` | `+MOD_INTEGRATED_KEYGEN` |

En Meson se modelan como `static_library()` adicionales; no requieren duplicar
listas de fuentes.

## Reproducibilidad

| Fuente de no-determinismo | Solución |
|---|---|
| `-DBUILD_TIME=$(date)` en 7 reglas | `SOURCE_DATE_EPOCH` del commit, formateado en UTC |
| PE `TimeDateStamp` que escribe el linker | `-Wl,--no-insert-timestamp` |
| Rutas absolutas del build embebidas | `-ffile-prefix-map=$src=.` |
| Timestamps y uid dentro de los `.a` | `ar` en modo determinista |
| Versión del toolchain a la deriva | `snapshot.debian.org` fijado a fecha |

`--no-insert-timestamp` está soportado por binutils 2.45 y de hecho el cero ya es
su default; se pasa explícito para no depender de esa versión.

`version.h` se genera con `configure_file()` desde `project(version:)`. Mueren
`version_major.txt`, `version_minor.txt` y el target `version:` que
auto-incrementaba en cada build.

**Invariante:** el build no escribe nada dentro del árbol de fuentes. El CI lo
comprueba con `git diff --exit-code` después de compilar.

## Libs externas

- **regex** → **PCRE2**, capa `pcre2posix`. Expone `regcomp/regexec/regfree/regerror`,
  las cuatro funciones que usan `kitty_regex.c` y `url/urlhack.c`.
- **jpeg** → **libjpeg-turbo**, API-compatible con la IJG que espera `kitty_image.c`
  (36 símbolos). Con SIMD desactivado para no arrastrar NASM.
- **bcrypt** → `bcrypt/bcrypt.c` nuevo (ver abajo).
- **base64, mini, blocnote, md5** → ya tienen fuentes completas en el repo; se
  compilan desde ellas y se borran sus `.a`.

### Reimplementación de bcrypt

`bcrypt/bcrypt.a` contiene `bcrypt.o` (17 KB) sin fuente. No es el bcrypt de
OpenSSH (ese es `0.76b_My_PuTTY/crypto/bcrypt.c`) sino cifrado de ficheros con
Blowfish más envoltorios propios de KiTTY. `nbcrypt.h` declara 17 funciones; el
código solo usa cuatro: `bcrypt_init`, `bcrypt_string_base64`,
`buncrypt_string_base64` y `bcrypt_file_base64`.

Se reimplementa la API completa sobre ChaCha20-Poly1305, tomado del propio
`0.76b_My_PuTTY/crypto/`, sin añadir dependencias.

**Cambio incompatible, aceptado explícitamente:** el formato de salida no
coincidirá con el anterior, así que las contraseñas guardadas por versiones
previas dejan de leerse. Para que no se corrompan datos en silencio, el formato
nuevo lleva cabecera mágica `SCT1`; al encontrar algo sin esa cabecera, las
funciones `buncrypt_*` devuelven un código de error distinguible en vez de
descifrar basura, y KiTTY pide reintroducir la contraseña.

## CI

Tres jobs en `.github/workflows/build.yml`:

1. **build** — contenedor Debian pinneado por digest, `meson setup && ninja`,
   sube los 6 `.exe`.
2. **reproducible** — recompila en otra ruta, otro usuario y otro `TZ`; compara
   `sha256sum`. Falla el workflow si difieren.
3. **attest** — `actions/attest-build-provenance` sobre los artefactos y publica
   `SHA256SUMS`.

## Verificación

- `meson test`: round-trip de la nueva bcrypt y detección del formato antiguo.
- Comparación del conjunto de objetos por binario contra el grafo extraído del
  makefile viejo: si a un `.exe` le falta o le sobra una fuente, salta ahí.
- `tools/verify-reproducible.sh` en local.
- `git diff --exit-code` post-build.

## Qué se elimina

`MAKEFILE.MINGW` · los 14 `.a` · el soporte de 32 bits · UPX ·
los targets `nohyperlink`, `zmodem`, `puttytel`, `psocks`, `testcrypt`, `notrans`,
`portable` · `version_major.txt` y `version_minor.txt` · las rutas `/builds/`.

## Riesgos

| Riesgo | Mitigación |
|---|---|
| Lista de fuentes mal reconstruida | Extraída de `make -n`, no a ojo; se compara binario por binario |
| PCRE2 POSIX no equivalente a gnulib regex | Solo se usan 4 funciones POSIX; se verifica que `urlhack.c` y `kitty_regex.c` compilan y enlazan |
| Contraseñas guardadas ilegibles | Aceptado; mitigado con cabecera mágica y error explícito |
| Warnings nuevos con GCC 13 vs el GCC viejo del contenedor | No se usa `-Werror`; se documentan |
