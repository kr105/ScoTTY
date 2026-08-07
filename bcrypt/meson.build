# Cifrado de contraseñas guardadas y volcados de configuración.
#
# bcrypt.c es una reimplementación: el bcrypt.o original venía dentro de
# bcrypt/bcrypt.a sin fuente en el repositorio. Ver la cabecera de bcrypt.c
# para el formato y para qué seguridad aporta realmente (poca: la clave se
# deriva de datos públicos).
#
# license.c sí conservaba su fuente y se compila tal cual; kitty.c usa
# license_form, license_make_with_first y license_test.

# CNG (bcrypt.dll) es la implementación de AES-GCM y PBKDF2 del sistema.
# Usarla evita criptografía escrita a mano en este repositorio.
bcrypt_system_dep = cc.find_library('bcrypt')

bcrypt_lib = static_library(
  'kitty_bcrypt',
  files(
    'bcrypt.c',
    'license.c',
  ),
  include_directories: putty_inc,
  dependencies: bcrypt_system_dep,
)

bcrypt_dep = declare_dependency(
  link_with: bcrypt_lib,
  dependencies: bcrypt_system_dep,
  include_directories: include_directories('.'),
)
