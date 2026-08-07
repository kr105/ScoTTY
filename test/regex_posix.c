/*
 * Comprueba que el regex POSIX que se enlaza se comporta como el que había
 * antes en regex/libregex.a (gnulib).
 *
 * Importa porque el patrón de detección de URLs es configurable por el usuario
 * vía HyperlinkRegularExpression: un motor con otra sintaxis cambiaría en
 * silencio el comportamiento de patrones ya guardados. El patrón de este test
 * es literalmente urlhack_default_regex, de url/urlhack.c, e incluye la
 * extensión GNU \w, que no forma parte de POSIX ERE.
 *
 * Los resultados esperados se obtuvieron ejecutando el mismo patrón contra la
 * implementación de glibc.
 */
#include <stdio.h>
#include <string.h>
#include <regex.h>

static const char *PATTERN =
    "((ht|f)tp(s?):\\/\\/[0-9a-zA-Z]([-\\.\\w]*[0-9a-zA-Z])*([:][0-9]+)?\\/?"
    "([-a-zA-Z0-9\\.\\?\\,\\'\\/\\\\\\+=&%\\$#_]*))|"
    "(mailto:[a-zA-Z0-9\\-_\\.]+@[a-zA-Z0-9\\-_\\.]+\\.[a-z]{2,})|"
    "(ssh:\\/\\/([-a-zA-Z0-9_]+([:][^@]*)?@)?[-a-zA-Z0-9_\\.]+"
    "((:[0-9]{2,5})?(\\/[-a-zA-Z0-9_]+)?)?)";

struct tcase {
    const char *text;
    const char *expected;      /* NULL = no debe haber coincidencia */
};

static const struct tcase CASES[] = {
    { "visita https://www.example.com/path?a=1&b=2 ahora",
      "https://www.example.com/path?a=1&b=2" },
    { "http://192.168.1.1:8080/x",           "http://192.168.1.1:8080/x" },
    { "mailto:someone@example.com",          "mailto:someone@example.com" },
    { "ssh://user:pw@host.name:22/dir",      "ssh://user:pw@host.name:22/dir" },
    { "ftp://ftp.gnu.org/gnu/",              "ftp://ftp.gnu.org/gnu/" },
    { "https://sub-domain.example.co.uk/a_b-c.d",
      "https://sub-domain.example.co.uk/a_b-c.d" },
    { "nada de urls aqui",                   NULL },
    { "http://a.b",                          "http://a.b" },
};

int main(void)
{
    regex_t rx;
    char errbuf[256];
    int rc, failures = 0;
    size_t i;

    if ((rc = regcomp(&rx, PATTERN, REG_EXTENDED)) != 0) {
        regerror(rc, &rx, errbuf, sizeof errbuf);
        printf("FALLO: regcomp del patrón por defecto: %s\n", errbuf);
        return 1;
    }

    for (i = 0; i < sizeof CASES / sizeof *CASES; i++) {
        regmatch_t m;
        int matched = regexec(&rx, CASES[i].text, 1, &m, 0) == 0;

        if (!matched) {
            if (CASES[i].expected != NULL) {
                printf("FALLO [%u]: se esperaba \"%s\", no hubo coincidencia\n",
                       (unsigned)i, CASES[i].expected);
                failures++;
            }
            continue;
        }

        if (CASES[i].expected == NULL) {
            printf("FALLO [%u]: coincidencia inesperada en \"%s\"\n",
                   (unsigned)i, CASES[i].text);
            failures++;
            continue;
        }

        size_t len = (size_t)(m.rm_eo - m.rm_so);
        if (len != strlen(CASES[i].expected) ||
            memcmp(CASES[i].text + m.rm_so, CASES[i].expected, len) != 0) {
            printf("FALLO [%u]: se esperaba \"%s\", se obtuvo \"%.*s\"\n",
                   (unsigned)i, CASES[i].expected, (int)len,
                   CASES[i].text + m.rm_so);
            failures++;
        }
    }

    regfree(&rx);

    if (failures == 0)
        printf("OK: %u casos, todos coinciden con la referencia glibc\n",
               (unsigned)(sizeof CASES / sizeof *CASES));
    return failures == 0 ? 0 : 1;
}
