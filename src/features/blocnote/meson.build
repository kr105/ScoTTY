# El editor de texto integrado en KiTTY. window.c:1256 lo arranca llamando a
# Notepad_WinMain, y putty-common.rc2:232 incluye notepad.rc para traerse sus
# diálogos e iconos.
#
# notepad_putty.c no se lista: notepad.c lo incluye con #include, igual que
# hacía el build anterior.

# NOMAIN selecciona la forma "biblioteca" de notepad.c: sin WinMain propio y
# con notepad_putty.c incluido, que es la que expone Notepad_WinMain para que
# la llame window.c. Sin este flag, notepad.c se compila como el ejecutable
# independiente y ni siquiera declara sus variables de estado.
blocnote_lib = static_library(
  'blocnote',
  files('notepad.c'),
  c_args: ['-DNOMAIN'],
  include_directories: putty_inc,
)

# link_whole y no link_with: Notepad_WinMain es stdcall y el enlazador debe
# incorporar el objeto entero, igual que el makefile pasaba notepad.a completo.
blocnote_dep = declare_dependency(
  link_whole: blocnote_lib,
  include_directories: include_directories('.'),
)
