#ifndef CHEST_ENC_H
#define CHEST_ENC_H

#include <sodium.h>
#include <string.h> // memcpy
#include <stdio.h> // files

typedef unsigned char      uc;
typedef unsigned short int us;
typedef unsigned int       ui;
typedef unsigned long int  ul;

/* should be called before using any function in this header
 * returns < 0 if not properly initialized */
int encInit();

enum ENC_METADATA {
    ENC_METADATA_FILE = 1,
    ENC_METADATA_MESSAGE = 2,
};

typedef struct {
    int r; // is -1 if verification failed, 0 if reading failed
    uc enc_metadata;
    ul m_l;
    uc* m;
    uc* m_with_header;
} EncDecryptFileOut;
void encDecryptFileOutClear(EncDecryptFileOut* out);

#ifdef ENC_IMPLEMENTATION
#define ENC_ALLOC(m) sodium_allocarray(m, 1)
#define ENC_ZERO(m, m_l) sodium_memzero(m, m_l)
#define ENC_ZERO_AND_FREE(m) sodium_free(m)

void encDecryptFileOutClear(EncDecryptFileOut* out) {
    // not clearing the return value intentionally
    out->enc_metadata = 0;
    out->m_l = 0;
    ENC_ZERO_AND_FREE(out->m_with_header);
}
#endif // ENC_IMPLEMENTATION

/* loads a file to heap buffer, decrypts it and returns it's contents in uchar array */
/* returns 0 on read error, -1 if verification failed, -10 if one of the arguments was null, 1 on success */
EncDecryptFileOut encDecryptFile(const char *pw, const char *path);

/* encrypts file as message, see enc.h metadata
 * returns 0 on write error */
int encEncryptMessage(const char *pw, const char *message, const char *path);

/* encrypts file as file, see enc.h metadata
 * returns 0 on write error, -10 if one of the parameters is null */
int encEncryptFile(const char *pw, const char *in_path, const char *out_path);

#endif //CHEST_CRYPTO_H
