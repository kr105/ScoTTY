# ScoTTY — cross-compilación reproducible de Linux/WSL a Windows x86_64.
#
#   meson setup build --cross-file cross/x86_64-w64-mingw32.ini
#   ninja -C build
#
# Reemplaza a src/windows/MAKEFILE.MINGW (2443 líneas, 290 reglas
# .o con listas de headers mantenidas a mano y ya desincronizadas).
#
# Ver docs/superpowers/specs/2026-08-06-meson-reproducible-build-design.md

project(
  'scotty',
  'c',
  version: '0.76.1.13',
  license: 'MIT',
  meson_version: '>=0.60.0',
  default_options: [
    'c_std=gnu99',
    'buildtype=release',
    'warning_level=1',
    'b_ndebug=true',
  ],
)

fs = import('fs')
cc = meson.get_compiler('c')

if host_machine.system() != 'windows'
  error(
    'ScoTTY solo compila para Windows. Usa:\n' +
    '  meson setup build --cross-file cross/x86_64-w64-mingw32.ini',
  )
endif

################################################################################
# Sello del build — determinista por construcción
################################################################################
# El build anterior inyectaba `date` en siete reglas, así que dos compilaciones
# del mismo commit jamás daban el mismo binario. Aquí la marca de tiempo sale
# del commit (o de SOURCE_DATE_EPOCH), nunca del reloj.

stamp = find_program('tools/build-stamp.sh')
epoch_opt = get_option('source_date_epoch')
stamp_args = epoch_opt == '' ? [] : ['--epoch=' + epoch_opt]

source_date_epoch = run_command(stamp, stamp_args, 'epoch', check: true).stdout().strip()
build_time        = run_command(stamp, stamp_args, 'time',  check: true).stdout().strip()
source_commit     = run_command(stamp, stamp_args, 'commit', check: true).stdout().strip()

# 0.76.1.13 -> partes reutilizadas por version.h y por los recursos .rc
version_parts = meson.project_version().split('.')
version_major = version_parts[0] + '.' + version_parts[1]          # 0.76
build_version = version_major + '.' + version_parts[2]             # 0.76.1
build_subversion = version_parts[3]                                # 13
binary_version = ','.join(version_parts)                           # 0,76,1,13

message('versión      : ' + meson.project_version())
message('commit       : ' + source_commit)
message('SOURCE_DATE_EPOCH: ' + source_date_epoch + '  (' + build_time + ')')

################################################################################
# Reproducibilidad
################################################################################

repro_c_args = [
  # Sin esto el binario embebe la ruta absoluta del árbol de compilación, así
  # que /home/tu/repo y /builds producirían binarios distintos.
  '-ffile-prefix-map=' + meson.project_source_root() + '=.',
  '-ffile-prefix-map=' + meson.project_build_root() + '=.',
]

repro_link_args = [
  '-Wl,--no-insert-timestamp',   # cero en el TimeDateStamp de la cabecera PE
  '-s',                          # sin símbolos: era el LDFLAGS del build viejo
]

foreach arg : repro_c_args
  if cc.has_argument(arg)
    add_project_arguments(arg, language: 'c')
  else
    warning('el compilador no acepta ' + arg + '; el build no será reproducible')
  endif
endforeach

add_project_link_arguments(repro_link_args, language: 'c')

################################################################################
# Flags de funcionalidad de KiTTY
################################################################################
# Los 19 MOD_* que el makefile repartía entre CFLAGS y comentarios sueltos.

kitty_mods = [
  'MOD_ADB',                 # integración con adb de Android
  'MOD_DISABLEALTGR',        # desactivar AltGr
  'MOD_FAR2L',               # extensiones de terminal far2l
  'MOD_HYPERLINK',           # URLs clicables (necesita regex POSIX)
  'MOD_KEYMAPPING',          # remapeo de teclas
  'MOD_LAUNCHER',            # lanzador de sesiones
  'MOD_PERSO',               # raíz de todas las modificaciones de KiTTY
  'MOD_PORTKNOCKING',        # port knocking
  'MOD_PRINTCLIP',           # impresión al portapapeles
  'MOD_PROXY',               # opciones extra de proxy
  'MOD_PUTTYX',              # extensiones de PuTTYX
  'MOD_RECONNECT',           # reconexión automática
  'MOD_RUTTY',               # scripting del parche RuTTY
  'MOD_SAVEDUMP',            # volcado de configuración
  'MOD_STARTBUTTON',         # botón de inicio
  'MOD_TUTTY',               # extensiones de TuTTY
  'MOD_WTS',                 # Windows Terminal Services
  'MOD_ZMODEM',              # transferencia ZMODEM
]

# La imagen de fondo del terminal y las capturas de pantalla se guardan en
# JPEG, y son lo único que hace que un cliente SSH acabe enlazando libjpeg
# (~700 KB estáticos en kitty.exe). Se deja activada por defecto porque es una
# funcionalidad de KiTTY de toda la vida, pero ahora se puede quitar:
#
#   meson setup build --cross-file cross/x86_64-w64-mingw32.ini \
#                     -Dbackground_image=false
want_background_image = get_option('background_image')
if want_background_image
  kitty_mods += 'MOD_BACKGROUNDIMAGE'
endif

# Plataforma y compatibilidad. Se conservan tal cual estaban en CFLAGS.
platform_defines = [
  '_WINDOWS',
  'WIN32S_COMPAT',
  '_NO_OLDNAMES',
  '__USE_MINGW_ANSI_STDIO=1',
  'WINVER=0x0501',
  '_WIN32_IE=0x0500',
  '_WIN32_WINDOWS=0x0410',
  '_WIN32_WINNT=0x0501',
  'NO_HTMLHELP',
  'NO_SECUREZEROMEMORY',
  'COVERITY',
  'FLJ1',
]

build_defines = [
  'BUILD_VERSION="' + build_version + '"',
  'BUILD_SUBVERSION=' + build_subversion,
  'BUILD_TIME="' + build_time + '"',
  'MASTER_PASSWORD="' + get_option('master_password') + '"',
  'EXTERN_REGEXLIB',           # urlhack usa el regex externo, no su re_lib interno
]

project_defines = []
foreach d : kitty_mods + platform_defines + build_defines
  project_defines += '-D' + d
endforeach
add_project_arguments(project_defines, language: 'c')

# GCC 10 cambió el default a -fno-common. Este código es anterior y declara
# variables en cabeceras sin extern (trayIcone, urlhack_default_regex y una
# treintena más), de modo que cada unidad de compilación genera una definición
# fuerte y el enlazador las rechaza como duplicadas. El GCC del contenedor
# antiguo aún usaba -fcommon, así que nunca se vio.
#
# Arreglarlo de verdad significa poner extern en las cabeceras y una
# definición en un único .c por cada símbolo, a lo largo de una base de código
# de 25 años. Eso es un cambio de código, no de build, y no se hace aquí.
add_project_arguments('-fcommon', language: 'c')

# Ruido heredado de una base de código de 25 años. No se usa -Werror.
add_project_arguments(
  cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-unused-but-set-variable',
    '-Wno-missing-field-initializers',
    '-Wno-deprecated-declarations',
    '-Wno-format-truncation',
    '-Wno-stringop-truncation',
  ]),
  language: 'c',
)

################################################################################
# Rutas de include
################################################################################

putty_inc = include_directories(
  '.',
  # El árbol de PuTTY se incluye a sí mismo con rutas tipo "ssh/signal-list.h"
  # o "windows/platform.h", relativas a su propia raíz. Antes esa raíz era
  # 0.76b_My_PuTTY/; ahora es src/.
  'src',
  'src/core',
  'src/charset',
  'src/crypto',
  'src/proxy',
  'src/ssh',
  'src/terminal',
  'src/unix',
  'src/utils',
  'src/windows',
  'src/scotty',
  'src/features/blocnote',
  'src/features/far2l',
  'src/features/rutty',
  'src/features/url',
  'third_party/base64',
  'third_party/bcrypt',
)

################################################################################
# Dependencias externas, pinneadas por hash en subprojects/*.wrap
################################################################################

posix_regex_dep = dependency('posix_regex', fallback: ['musl-regex', 'posix_regex_dep'])

# Solo se descarga y compila si la imagen de fondo está activada.
#
# default_library=static o el subproyecto genera libjpeg-8.dll y los .exe salen
# dependiendo de ella en runtime. Los binarios de ScoTTY se distribuyen sueltos,
# sin instalador, así que todo va enlazado estáticamente.
if want_background_image
  libjpeg_dep = dependency(
    'libjpeg',
    default_options: ['default_library=static'],
  )
else
  libjpeg_dep = declare_dependency()
endif

# Bibliotecas del sistema Win32. Unión de las que enlazaba cada .exe.
win_libs = []
foreach l : [
  'advapi32', 'comctl32', 'comdlg32', 'gdi32', 'imm32', 'ole32',
  'psapi', 'shell32', 'user32', 'winmm', 'winspool', 'wsock32', 'wtsapi32',
]
  win_libs += cc.find_library(l)
endforeach

################################################################################
# Bibliotecas auxiliares del propio repo
################################################################################
# Antes se enlazaban como .a precompilados sin reglas para regenerarlos.
# Ahora se compilan desde sus fuentes en cada build.

subdir('third_party/base64')
subdir('third_party/bcrypt')
subdir('src/features/blocnote')
subdir('third_party/mini')

# md5/ no se compila. El makefile lo enlazaba, pero sus dos únicos símbolos,
# CheckMD5Integrity y PrintConsole, no se llaman desde ningún sitio. Al ser una
# biblioteca estática, el enlazador ya extraía cero objetos de ella, así que
# omitirla no cambia los binarios. kitty.c sigue incluyendo md5/MD5check.h, y
# por eso md5/ permanece en las rutas de include.

################################################################################
# Cabeceras generadas desde texto
################################################################################
# El makefile las generaba con tuberías de sed y las escribía dentro del árbol
# de fuentes. Aquí van al directorio de build, como cualquier otro artefacto.

gen_text_header = find_program('tools/gen-text-header.py')

kitty_ini_h = custom_target(
  'kitty_ini.h',
  input: 'src/scotty/kitty_ini.txt',
  output: 'kitty_ini.h',
  command: [
    gen_text_header, '@INPUT@', '@OUTPUT@',
    '--decl', 'char default_init_file_content[]',
    '--eol', '\\n',
  ],
)

# El texto de ayuda de la línea de órdenes sale de la documentación, así que
# ambas no pueden desincronizarse.
kitty_help_h = custom_target(
  'kitty_help.h',
  input: 'docs/pages/CommandLine.md',
  output: 'kitty_help.h',
  command: [
    gen_text_header, '@INPUT@', '@OUTPUT@',
    '--decl', 'static char *default_help_file_content',
    '--eol', '\\r\\n',
    '--between', 'CmdLineOptions_begin', 'CmdLineOptions_end',
    '--strip-html-comments',
    '--strip-markdown-bold',
  ],
)

################################################################################
# PuTTY y los ejecutables
################################################################################

subdir('src/core')     # genera version.h
subdir('src/windows')  # los seis ejecutables

if get_option('tests')
  subdir('test')
endif
