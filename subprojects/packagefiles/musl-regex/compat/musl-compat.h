/*
 * Las tres definiciones que src/regex/ de musl espera del resto de musl.
 * Se inyecta con -include, así que no hace falta tocar las fuentes originales.
 */
#ifndef SCOTTY_MUSL_COMPAT_H
#define SCOTTY_MUSL_COMPAT_H

/* Macro de visibilidad de musl (features.h). Irrelevante en una static lib. */
#ifndef hidden
#define hidden
#endif

/* Límites POSIX que musl declara en su <limits.h>. */
#ifndef CHARCLASS_NAME_MAX
#define CHARCLASS_NAME_MAX 14
#endif
#ifndef RE_DUP_MAX
#define RE_DUP_MAX 255
#endif

#endif /* SCOTTY_MUSL_COMPAT_H */
