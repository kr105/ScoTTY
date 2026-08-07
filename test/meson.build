# Los tests se compilan como ejecutables de Windows, igual que el resto del
# proyecto. Solo se registran para ejecución si Meson dispone de una forma de
# correr binarios del host —wine, típicamente—; si no, se compilan y ahí queda
# la comprobación de que enlazan.

test_regex = executable(
  'test_regex_posix',
  files('regex_posix.c'),
  dependencies: [posix_regex_dep],
  win_subsystem: 'console',
  name_suffix: 'exe',
  install: false,
)

test_bcrypt = executable(
  'test_bcrypt_roundtrip',
  files('bcrypt_roundtrip.c'),
  dependencies: [bcrypt_dep],
  include_directories: putty_inc,
  win_subsystem: 'console',
  name_suffix: 'exe',
  install: false,
)

if meson.can_run_host_binaries()
  test('regex POSIX equivale al motor anterior', test_regex)
  test('bcrypt cifra y descifra, y rechaza el formato antiguo', test_bcrypt)
else
  message(
    'Tests compilados pero no ejecutados: no hay forma de correr binarios de ' +
    'Windows en este host. Instala wine o añade exe_wrapper al cross file.',
  )
endif
