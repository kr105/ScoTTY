# Núcleo de PuTTY. Solo genera version.h; las fuentes de cada binario se
# declaran en src/windows/meson.build, junto a los ejecutables que las usan.

version_conf = configuration_data()
version_conf.set('RELEASE', version_major)
version_conf.set('BINARY_VERSION', binary_version)
version_conf.set('SOURCE_COMMIT', source_commit)

# Se genera en el directorio de build, no en el árbol de fuentes: el build no
# debe escribir nada bajo control de versiones. El CI lo verifica con
# `git diff --exit-code`.
version_h = configure_file(
  input: 'version.h.in',
  output: 'version.h',
  configuration: version_conf,
)
