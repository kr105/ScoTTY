/*
 * regerror.c incluye "locale_impl.h" para traducir sus mensajes de error vía
 * LCTRANS_CUR. Sin catálogos de locale la traducción es la identidad, que es
 * exactamente lo que hacía la libregex.a anterior.
 */
#ifndef SCOTTY_LOCALE_IMPL_H
#define SCOTTY_LOCALE_IMPL_H

#define LCTRANS_CUR(msg) (msg)

#endif /* SCOTTY_LOCALE_IMPL_H */
