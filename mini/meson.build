# Lector/escritor de ficheros INI. KiTTY usa readINI, writeINI y delINI
# desde kitty.c y kitty_commun.c, para el modo de configuración en fichero.
# Antes se enlazaba como mini/mini.a, precompilado.

mini_lib = static_library(
  'mini',
  files('mini.c'),
  include_directories: putty_inc,
)

mini_dep = declare_dependency(
  link_with: mini_lib,
  include_directories: include_directories('.'),
)
