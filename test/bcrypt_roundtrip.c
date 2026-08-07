/*
 * Comprueba el contrato de bcrypt.c tal y como lo usan kitty_crypt.c,
 * kitty.c y kitty_savedump.c.
 *
 * Lo que se verifica:
 *   1. Ida y vuelta con buffers separados.
 *   2. Ida y vuelta in-place (st_in == st_out), que es como lo llama
 *      kitty_crypt.c y la razón por la que la salida pisa la entrada.
 *   3. El retorno de buncrypt_* es la LONGITUD del texto plano, no 1.
 *      kitty_savedump.c:583 y kitty.c:4418 lo usan para recorrer el buffer.
 *   4. Texto plano con NUL embebidos, que es el caso del contenido de scripts.
 *   5. Una clave incorrecta falla en vez de devolver basura.
 *   6. El contenido en formato antiguo se rechaza con 0 en vez de descifrarse
 *      como basura. Esto es lo que evita que una contraseña guardada por una
 *      versión anterior se convierta en ruido silenciosamente.
 *   7. Se respeta el techo de tamaño de salida en vez de desbordar.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nbcrypt.h"

static int failures = 0;

static void check(int cond, const char *what)
{
    if (cond) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FALLO %s\n", what);
        failures++;
    }
}

int main(void)
{
    static const char *KEY = "host.examplextermKiTTY";
    char buf[4096], out[4096];
    int n, m;

    bcrypt_init(0);

    /* 1. buffers separados */
    {
        const char *plain = "correo caballo bateria grapa";
        n = bcrypt_string_base64(plain, out, (unsigned)strlen(plain), KEY, 0);
        check(n > 0, "cifra a buffer separado");
        m = buncrypt_string_base64(out, buf, (unsigned)n, KEY);
        check(m == (int)strlen(plain), "descifra y devuelve la longitud");
        check(memcmp(buf, plain, strlen(plain)) == 0, "el texto plano coincide");
    }

    /* 2. in-place, como lo llama kitty_crypt.c */
    {
        const char *plain = "s3cr3t";
        strcpy(buf, plain);
        n = bcrypt_string_base64(buf, buf, (unsigned)strlen(plain), KEY, 0);
        check(n > 0, "cifra in-place");
        m = buncrypt_string_base64(buf, buf, (unsigned)n, KEY);
        check(m == (int)strlen(plain), "descifra in-place");
        check(strcmp(buf, plain) == 0, "el texto plano sobrevive al in-place");
    }

    /* 3 y 4. NUL embebidos: el formato de scriptfilecontent */
    {
        const char script[] = "linea uno\0linea dos\0linea tres\0";
        unsigned len = (unsigned)sizeof script - 1;
        n = bcrypt_string_base64(script, out, len, KEY, 0);
        check(n > 0, "cifra contenido con NUL embebidos");
        m = buncrypt_string_base64(out, buf, (unsigned)n, KEY);
        check(m == (int)len, "la longitud devuelta cubre los NUL embebidos");
        check(memcmp(buf, script, len) == 0, "los NUL embebidos se preservan");
    }

    /* 5. clave incorrecta */
    {
        const char *plain = "no deberia salir";
        n = bcrypt_string_base64(plain, out, (unsigned)strlen(plain), KEY, 0);
        m = buncrypt_string_base64(out, buf, (unsigned)n, "clave-equivocada");
        check(m == 0, "una clave incorrecta falla en vez de devolver basura");
    }

    /* 6. formato antiguo */
    {
        /* base64 arbitrario sin la cabecera SCT1: lo que produciría KiTTY viejo */
        const char *legacy = "Zm9vYmFyYmF6cXV1eGNvcmdlZ3JhdWx0Z2FycGx5";
        m = buncrypt_string_base64(legacy, buf, (unsigned)strlen(legacy), KEY);
        check(m == 0, "el formato antiguo se rechaza, no se descifra como basura");
    }

    /* 7. techo de tamaño */
    {
        static char big[8192];
        memset(big, 'A', sizeof big - 1);
        big[sizeof big - 1] = '\0';
        n = bcrypt_string_base64(big, out, (unsigned)(sizeof big - 1), KEY, 0);
        check(n == 0, "una entrada demasiado grande se rechaza en vez de desbordar");
    }

    /* salto de línea cada maxlinesize caracteres */
    {
        const char *plain = "algo lo bastante largo como para envolver varias veces";
        n = bcrypt_string_base64(plain, out, (unsigned)strlen(plain), KEY, 80);
        check(n > 0 && strchr(out, '\n') != NULL, "maxlinesize inserta saltos");
        m = buncrypt_string_base64(out, buf, (unsigned)n, KEY);
        check(m == (int)strlen(plain), "el descifrado ignora los saltos de línea");
    }

    printf(failures == 0 ? "OK: contrato de bcrypt verificado\n"
                         : "%d comprobaciones fallidas\n", failures);
    return failures == 0 ? 0 : 1;
}
