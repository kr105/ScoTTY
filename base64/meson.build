# Codificación base64. KiTTY usa decode64 desde kitty.h.
# Antes se enlazaba como base64/base64.a, precompilado y sin regla para
# regenerarlo, pese a que su fuente siempre estuvo aquí al lado.

base64_lib = static_library(
  'base64',
  files('base64.c'),
  include_directories: putty_inc,
)

base64_dep = declare_dependency(
  link_with: base64_lib,
  include_directories: include_directories('.'),
)
