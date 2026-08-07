# Compila únicamente src/regex/ del tarball de musl. El resto de musl se ignora.
#
# Los cuatro ficheros necesitan tres cosas que musl obtiene de su propia
# maquinaria de headers y que aquí se aportan desde compat/:
#   - <regex.h>          declaraciones POSIX con las mismas structs que musl
#   - "locale_impl.h"    LCTRANS_CUR, que sin locales es la identidad
#   - musl-compat.h      hidden, CHARCLASS_NAME_MAX, RE_DUP_MAX

project(
  'musl-regex',
  'c',
  version: '1.2.5',
  license: 'MIT',
  meson_version: '>=0.60.0',
)

compat_inc = include_directories('compat')

posix_regex_lib = static_library(
  'posix_regex',
  files(
    'src/regex/regcomp.c',
    'src/regex/regerror.c',
    'src/regex/regexec.c',
    'src/regex/tre-mem.c',
  ),
  include_directories: compat_inc,
  c_args: [
    '-include', meson.current_source_dir() / 'compat' / 'musl-compat.h',
    '-Wno-unused-parameter',
    '-Wno-sign-compare',
  ],
  install: false,
)

posix_regex_dep = declare_dependency(
  link_with: posix_regex_lib,
  include_directories: compat_inc,
)
