/*
 * bcrypt.c — cifrado de cadenas y ficheros para KiTTY/ScoTTY.
 *
 * Reemplaza al bcrypt.o que se enlazaba desde bcrypt/bcrypt.a: 17 KB de código
 * objeto compilado en 2020 cuyo fuente no estaba en el repositorio ni está
 * publicado en esa forma. Mientras existiera, ninguna afirmación sobre
 * reproducibilidad del binario final podía verificarse de verdad.
 *
 *
 * SOBRE QUÉ SEGURIDAD APORTA ESTO
 *
 * Ninguna frente a un atacante con el binario. La clave que reciben estas
 * funciones se deriva de datos públicos —el hostname y el termtype de la
 * sesión, o la constante de compilación MASTER_PASSWORD— así que cualquiera
 * puede reproducirla. Esto es ofuscación de contraseñas guardadas, igual que
 * lo era la implementación anterior. Se usa AES-GCM en vez de un cifrado
 * casero porque no cuesta más y porque delega en la implementación del
 * sistema, no porque convierta el esquema en confidencial.
 *
 * Confidencialidad real exigiría una contraseña maestra introducida por el
 * usuario y no almacenada, que es un cambio de producto, no de build.
 *
 *
 * FORMATO
 *
 *   "SCT1" | salt[16] | nonce[12] | tag[16] | ciphertext[n]
 *
 * y el conjunto codificado en base64. AES-256-GCM con clave derivada por
 * PBKDF2-HMAC-SHA256, todo vía CNG (bcrypt.dll) de Windows.
 *
 * El formato NO es compatible con el anterior: es un cambio deliberado, ya que
 * el formato viejo no era descriptible sin su fuente. La cabecera mágica hace
 * que el contenido antiguo se detecte y se rechace con error en vez de
 * descifrarse como basura; KiTTY pide entonces reintroducir la contraseña.
 *
 *
 * CONTRATO CON LOS LLAMANTES  (importante: la API es in-place)
 *
 * kitty_crypt.c invoca estas funciones con st_in == st_out, así que la salida
 * pisa la entrada y al cifrar es más larga. La API no recibe el tamaño del
 * buffer de destino, de modo que no hay forma de comprobarlo aquí. Los
 * llamantes reales usan buffers de 4096 bytes, así que se rechaza cualquier
 * entrada cuya salida superaría BCRYPT_MAX_OUTPUT. La implementación anterior
 * no comprobaba nada y desbordaba en silencio.
 *
 * Valores de retorno, tomados de cómo los usan los llamantes y no del
 * comentario de nbcrypt.h, que dice "1 si tout va bien" pero es incorrecto:
 *   - bcrypt_*   -> longitud del texto cifrado escrito, 0 si error
 *   - buncrypt_* -> longitud del texto plano recuperado, 0 si error
 * kitty_savedump.c:583 y kitty.c:4418 recorren el buffer descifrado usando ese
 * retorno como longitud, porque el texto plano puede llevar NUL embebidos.
 */

/* CNG requiere Vista o superior; el resto del proyecto fija XP. */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#undef WINVER
#define WINVER 0x0600

#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbcrypt.h"

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#define MAGIC        "SCT1"
#define MAGIC_LEN    4
#define SALT_LEN     16
#define NONCE_LEN    12
#define TAG_LEN      16
#define HEADER_LEN   (MAGIC_LEN + SALT_LEN + NONCE_LEN + TAG_LEN)   /* 48 */

/*
 * Iteraciones de PBKDF2. Deliberadamente bajas: la clave se deriva de datos
 * públicos, así que endurecer la derivación no frena a nadie que pueda leer el
 * binario, y esto se ejecuta una vez por sesión al cargar la lista.
 */
#define KDF_ITERATIONS 1000

/* Techo de la salida en base64, ajustado a los buffers de 4096 de los llamantes. */
#define BCRYPT_MAX_OUTPUT 4000

/* ---------------------------------------------------------------- base64 -- */

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t b64_encoded_len(size_t n, unsigned int maxlinesize)
{
    size_t len = ((n + 2) / 3) * 4;
    if (maxlinesize > 0 && len > 0)
        len += (len - 1) / maxlinesize;      /* saltos de línea insertados */
    return len;
}

static size_t b64_encode(const unsigned char *in, size_t n,
                         char *out, unsigned int maxlinesize)
{
    size_t i, o = 0;
    unsigned int col = 0;

    for (i = 0; i < n; i += 3) {
        unsigned long v = (unsigned long)in[i] << 16;
        size_t rem = n - i;
        if (rem > 1) v |= (unsigned long)in[i + 1] << 8;
        if (rem > 2) v |= (unsigned long)in[i + 2];

        char quad[4];
        quad[0] = B64[(v >> 18) & 0x3F];
        quad[1] = B64[(v >> 12) & 0x3F];
        quad[2] = rem > 1 ? B64[(v >> 6) & 0x3F] : '=';
        quad[3] = rem > 2 ? B64[v & 0x3F] : '=';

        int k;
        for (k = 0; k < 4; k++) {
            if (maxlinesize > 0 && col == maxlinesize) {
                out[o++] = '\n';
                col = 0;
            }
            out[o++] = quad[k];
            col++;
        }
    }
    out[o] = '\0';
    return o;
}

static int b64_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;                                   /* '=' y basura incluidos */
}

/* Decodifica ignorando espacios y saltos de línea. Devuelve bytes escritos. */
static size_t b64_decode(const char *in, size_t n, unsigned char *out)
{
    size_t i, o = 0;
    unsigned long acc = 0;
    int bits = 0;

    for (i = 0; i < n; i++) {
        int v = b64_value(in[i]);
        if (v < 0) continue;
        acc = (acc << 6) | (unsigned long)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (unsigned char)((acc >> bits) & 0xFF);
        }
    }
    return o;
}

/* ------------------------------------------------------------------ CNG -- */

static int derive_key(const char *key, const unsigned char *salt,
                      unsigned char out[32])
{
    BCRYPT_ALG_HANDLE alg = NULL;
    NTSTATUS st;
    int ok = 0;

    st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL,
                                     BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (st != STATUS_SUCCESS)
        return 0;

    st = BCryptDeriveKeyPBKDF2(alg,
                               (PUCHAR)key, (ULONG)(key ? strlen(key) : 0),
                               (PUCHAR)salt, SALT_LEN,
                               KDF_ITERATIONS,
                               out, 32, 0);
    ok = (st == STATUS_SUCCESS);

    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

static int random_bytes(unsigned char *buf, ULONG n)
{
    return BCryptGenRandom(NULL, buf, n,
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) == STATUS_SUCCESS;
}

/*
 * Cifra o descifra con AES-256-GCM. encrypt != 0 produce tag; encrypt == 0 lo
 * verifica y falla si no cuadra.
 */
static int aes_gcm(int encrypt, const unsigned char key[32],
                   const unsigned char *nonce, unsigned char *tag,
                   const unsigned char *in, size_t inlen, unsigned char *out)
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE hkey = NULL;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    NTSTATUS st;
    ULONG done = 0;
    int ok = 0;

    st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (st != STATUS_SUCCESS)
        return 0;

    st = BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                           (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                           sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (st != STATUS_SUCCESS)
        goto out;

    st = BCryptGenerateSymmetricKey(alg, &hkey, NULL, 0, (PUCHAR)key, 32, 0);
    if (st != STATUS_SUCCESS)
        goto out;

    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = (PUCHAR)nonce;
    info.cbNonce = NONCE_LEN;
    info.pbTag   = tag;
    info.cbTag   = TAG_LEN;

    if (encrypt)
        st = BCryptEncrypt(hkey, (PUCHAR)in, (ULONG)inlen, &info, NULL, 0,
                           out, (ULONG)inlen, &done, 0);
    else
        st = BCryptDecrypt(hkey, (PUCHAR)in, (ULONG)inlen, &info, NULL, 0,
                           out, (ULONG)inlen, &done, 0);

    ok = (st == STATUS_SUCCESS);

out:
    if (hkey) BCryptDestroyKey(hkey);
    if (alg)  BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

/* --------------------------------------------------------------- núcleo -- */

void bcrypt_init(const long t)
{
    /*
     * La implementación anterior sembraba aquí su generador. CNG gestiona su
     * propia entropía, así que no hay nada que inicializar. Se conserva el
     * símbolo porque kitty.c:5584 lo llama.
     */
    (void)t;
}

int bcrypt_string_base64(const char *st_in, char *st_out,
                         const unsigned int length, const char *key,
                         const unsigned int maxlinesize)
{
    unsigned char salt[SALT_LEN], nonce[NONCE_LEN], tag[TAG_LEN], dk[32];
    unsigned char *blob = NULL;
    size_t bloblen, outlen;
    int rc = 0;

    if (st_in == NULL || st_out == NULL)
        return 0;

    bloblen = HEADER_LEN + (size_t)length;
    if (b64_encoded_len(bloblen, maxlinesize) >= BCRYPT_MAX_OUTPUT)
        return 0;                                /* no cabe: mejor fallar */

    if (!random_bytes(salt, SALT_LEN) || !random_bytes(nonce, NONCE_LEN))
        return 0;
    if (!derive_key(key, salt, dk))
        return 0;

    if ((blob = (unsigned char *)malloc(bloblen)) == NULL)
        goto done;

    /* El texto plano se copia antes de tocar st_out: pueden ser el mismo buffer. */
    if (!aes_gcm(1, dk, nonce, tag,
                 (const unsigned char *)st_in, length, blob + HEADER_LEN))
        goto done;

    memcpy(blob, MAGIC, MAGIC_LEN);
    memcpy(blob + MAGIC_LEN, salt, SALT_LEN);
    memcpy(blob + MAGIC_LEN + SALT_LEN, nonce, NONCE_LEN);
    memcpy(blob + MAGIC_LEN + SALT_LEN + NONCE_LEN, tag, TAG_LEN);

    outlen = b64_encode(blob, bloblen, st_out, maxlinesize);
    rc = (int)outlen;

done:
    if (blob) {
        SecureZeroMemory(blob, bloblen);
        free(blob);
    }
    SecureZeroMemory(dk, sizeof dk);
    return rc;
}

int buncrypt_string_base64(const char *st_in, char *st_out,
                           const unsigned int length, const char *key)
{
    unsigned char *blob = NULL, *plain = NULL;
    unsigned char dk[32];
    size_t bloblen, plainlen;
    int rc = 0;

    if (st_in == NULL || st_out == NULL || length == 0)
        return 0;

    if ((blob = (unsigned char *)malloc((size_t)length + 4)) == NULL)
        return 0;

    bloblen = b64_decode(st_in, length, blob);

    /*
     * Contenido cifrado por versiones anteriores de KiTTY. Sin la cabecera no
     * se puede descifrar, y descifrarlo "a la fuerza" produciría basura que el
     * llamante trataría como una contraseña. Se falla de forma explícita.
     */
    if (bloblen < HEADER_LEN || memcmp(blob, MAGIC, MAGIC_LEN) != 0)
        goto done;

    plainlen = bloblen - HEADER_LEN;
    if ((plain = (unsigned char *)malloc(plainlen + 1)) == NULL)
        goto done;

    if (!derive_key(key, blob + MAGIC_LEN, dk))
        goto done;

    if (!aes_gcm(0, dk,
                 blob + MAGIC_LEN + SALT_LEN,                 /* nonce */
                 blob + MAGIC_LEN + SALT_LEN + NONCE_LEN,     /* tag   */
                 blob + HEADER_LEN, plainlen, plain))
        goto done;                                /* tag inválido: se rechaza */

    memcpy(st_out, plain, plainlen);
    st_out[plainlen] = '\0';
    rc = (int)plainlen;

done:
    if (plain) {
        SecureZeroMemory(plain, plainlen + 1);
        free(plain);
    }
    if (blob) free(blob);
    SecureZeroMemory(dk, sizeof dk);
    return rc;
}

/* ------------------------------------------------------------- ficheros -- */

static int slurp(const char *filename, unsigned char **buf, size_t *len)
{
    FILE *fp;
    long size;

    if ((fp = fopen(filename, "rb")) == NULL)
        return 0;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    if ((size = ftell(fp)) < 0)      { fclose(fp); return 0; }
    rewind(fp);

    if ((*buf = (unsigned char *)malloc((size_t)size + 1)) == NULL) {
        fclose(fp);
        return 0;
    }
    *len = fread(*buf, 1, (size_t)size, fp);
    (*buf)[*len] = '\0';
    fclose(fp);
    return 1;
}

static int spew(const char *filename, const void *buf, size_t len)
{
    FILE *fp;
    size_t written;

    if ((fp = fopen(filename, "wb")) == NULL)
        return 0;
    written = fwrite(buf, 1, len, fp);
    return fclose(fp) == 0 && written == len;
}

int bcrypt_file_base64(const char *filename_in, const char *filename_out,
                       const char *key, const unsigned int maxlinesize)
{
    unsigned char *in = NULL;
    char *out = NULL;
    size_t inlen = 0;
    int rc = 0, n;

    if (!slurp(filename_in, &in, &inlen))
        return 0;

    /* Aquí sí conocemos el tamaño de destino, así que no aplica el techo. */
    out = (char *)malloc(b64_encoded_len(HEADER_LEN + inlen, maxlinesize) + 2);
    if (out == NULL)
        goto done;

    {
        unsigned char salt[SALT_LEN], nonce[NONCE_LEN], tag[TAG_LEN], dk[32];
        unsigned char *blob;
        size_t bloblen = HEADER_LEN + inlen;

        if (!random_bytes(salt, SALT_LEN) || !random_bytes(nonce, NONCE_LEN))
            goto done;
        if (!derive_key(key, salt, dk))
            goto done;
        if ((blob = (unsigned char *)malloc(bloblen)) == NULL)
            goto done;

        if (aes_gcm(1, dk, nonce, tag, in, inlen, blob + HEADER_LEN)) {
            memcpy(blob, MAGIC, MAGIC_LEN);
            memcpy(blob + MAGIC_LEN, salt, SALT_LEN);
            memcpy(blob + MAGIC_LEN + SALT_LEN, nonce, NONCE_LEN);
            memcpy(blob + MAGIC_LEN + SALT_LEN + NONCE_LEN, tag, TAG_LEN);
            n = (int)b64_encode(blob, bloblen, out, maxlinesize);
            rc = spew(filename_out, out, (size_t)n) ? n : 0;
        }
        SecureZeroMemory(blob, bloblen);
        free(blob);
        SecureZeroMemory(dk, sizeof dk);
    }

done:
    if (in) { SecureZeroMemory(in, inlen); free(in); }
    if (out) free(out);
    return rc;
}

int buncrypt_file_base64(const char *filename, const char *filename_out,
                         const char *key)
{
    unsigned char *in = NULL;
    char *out = NULL;
    size_t inlen = 0;
    int rc = 0, n;

    if (!slurp(filename, &in, &inlen))
        return 0;

    if ((out = (char *)malloc(inlen + 1)) == NULL)
        goto done;

    n = buncrypt_string_base64((const char *)in, out, (unsigned int)inlen, key);
    if (n > 0)
        rc = spew(filename_out, out, (size_t)n) ? n : 0;

done:
    if (in) free(in);
    if (out) { SecureZeroMemory(out, inlen + 1); free(out); }
    return rc;
}

/* ------------------------------------------------------------ variantes -- */
/*
 * El resto de la API declarada en nbcrypt.h. Ninguna de estas funciones se
 * llama desde el código de KiTTY —se comprobó símbolo a símbolo— pero se
 * mantienen para no cambiar la interfaz de la biblioteca.
 *
 * Las variantes "printable" y "base64url" comparten transporte con base64: el
 * formato interno es el mismo y solo cambiaría el alfabeto, que aquí no se
 * diferencia. Las "auto" operan sobre un único buffer, in-place.
 */

int bcrypt_string(const char *st_in, char *st_out, const unsigned int length,
                  const char *init_pattern, const char *key,
                  const unsigned int maxlinesize)
{
    (void)init_pattern;
    return bcrypt_string_base64(st_in, st_out, length, key, maxlinesize);
}

int buncrypt_string(const char *st_in, char *st_out, const unsigned int length,
                    const char *init_pattern, const char *key)
{
    (void)init_pattern;
    return buncrypt_string_base64(st_in, st_out, length, key);
}

int bcrypt_string_printable(const char *st_in, char *st_out,
                            const unsigned int length, const char *key,
                            const unsigned int maxlinesize)
{
    return bcrypt_string_base64(st_in, st_out, length, key, maxlinesize);
}

int buncrypt_string_printable(const char *st_in, char *st_out,
                              const unsigned int length, const char *key)
{
    return buncrypt_string_base64(st_in, st_out, length, key);
}

int bcrypt_string_auto(char *st, const unsigned int length,
                       const char *init_pattern, const char *key,
                       const unsigned int maxlinesize)
{
    (void)init_pattern;
    return bcrypt_string_base64(st, st, length, key, maxlinesize);
}

int buncrypt_string_auto(char *st, const unsigned int length,
                         const char *init_pattern, const char *key)
{
    (void)init_pattern;
    return buncrypt_string_base64(st, st, length, key);
}

int bcrypt_file(const char *filename_in, const char *filename_out,
                const char *init_pattern, const char *key,
                const unsigned int maxlinesize)
{
    (void)init_pattern;
    return bcrypt_file_base64(filename_in, filename_out, key, maxlinesize);
}

int buncrypt_file(const char *filename_in, const char *filename_out,
                  const char *init_pattern, const char *key)
{
    (void)init_pattern;
    return buncrypt_file_base64(filename_in, filename_out, key);
}

int bcrypt_file_printable(const char *filename_in, const char *filename_out,
                          const char *key, const unsigned int maxlinesize)
{
    return bcrypt_file_base64(filename_in, filename_out, key, maxlinesize);
}

int buncrypt_file_printable(const char *filename, const char *filename_out,
                            const char *key)
{
    return buncrypt_file_base64(filename, filename_out, key);
}

int bcrypt_file_auto(const char *filename, const char *init_pattern,
                     const char *key, const unsigned int maxlinesize)
{
    (void)init_pattern;
    return bcrypt_file_base64(filename, filename, key, maxlinesize);
}

int buncrypt_file_auto(const char *filename, const char *init_pattern,
                       const char *key)
{
    (void)init_pattern;
    return buncrypt_file_base64(filename, filename, key);
}
