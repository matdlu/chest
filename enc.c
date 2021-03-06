#define ENC_IMPLEMENTATION
#include "enc.h"

#define ENC_KEY_L  8 * crypto_secretbox_KEYBYTES
#define ENC_NONCE_L  8 * crypto_secretbox_NONCEBYTES
#define ENC_SALT_L  8 * crypto_pwhash_SALTBYTES

int encInit() {
    return sodium_init();
}

static ul encStrLen(const char* str) {
    if ( str ) {
        size_t str_l = 0;
        while (str[str_l]) str_l++;
        return str_l--;
    } else {
        return 0;
    }
}

/* encryption */

static void encPbkdf(const char* pw, const uc* salt, uc* key) {
    size_t pw_l = encStrLen(pw);

    if (crypto_pwhash
                (key, ENC_KEY_L, pw, pw_l, salt,
                 crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
                 crypto_pwhash_ALG_ARGON2ID13) != 0) {
        /* out of memory, propably ignore it, since it would propably be impossible on desktop */
    }
}

typedef struct {
    uc salt[ENC_SALT_L];
    uc nonce[ENC_NONCE_L];
    ul m_l;
    ul m_enc_l;
    uc* m_enc;
} EncEncryptOut;

static void encEncryptOutClear(EncEncryptOut* out) {
    ENC_ZERO(out->salt, ENC_SALT_L);
    ENC_ZERO(out->nonce, ENC_NONCE_L);
    out->m_l = 0;
    out->m_enc_l = 0;
    ENC_ZERO_AND_FREE(out->m_enc);
}

static EncEncryptOut encEncrypt(uc* m, const ul m_l, const char* pw) {
    size_t m_enc_l = m_l + crypto_secretbox_MACBYTES; // + space for authentication header
    uc* m_enc = ENC_ALLOC(m_enc_l);

    /* generate nonce */
    uc nonce[ENC_NONCE_L];
    randombytes_buf(&nonce, ENC_NONCE_L);

    /* derive key from password */
    uc salt[ENC_SALT_L];
    randombytes_buf(salt, ENC_SALT_L);
    uc key[ENC_KEY_L];
    encPbkdf(pw, salt, key);

    /* actual encryption */
    crypto_secretbox_easy(m_enc, m, m_l, nonce, key);

    /* output handling */
    EncEncryptOut out;
    ENC_ZERO(&out, sizeof(out));

    out.m_l = m_l;
    out.m_enc = m_enc;
    out.m_enc_l = m_enc_l;
    memcpy(out.nonce, nonce, ENC_NONCE_L); //for(int i = 0; i < ENC_NONCE_L; i++) out.nonce[i] = nonce[i];
    memcpy(out.salt, salt, ENC_SALT_L); //for(int i = 0; i < ENC_SALT_L; i++) out.salt[i] = salt[i];

    return out;
}

static uc* encDecrypt(uc* m_enc, const ul m_enc_l, const uc* nonce, const uc* salt, const char* pw) {
    uc *m_dec = ENC_ALLOC(m_enc_l - crypto_secretbox_MACBYTES); // todo: check if this screwed up something

    uc key[ENC_KEY_L];
    encPbkdf(pw, salt, key);

    if ( crypto_secretbox_open_easy(m_dec, m_enc, m_enc_l, nonce, key) >= 0 ) {
        return m_dec;
    } else {
        return 0; // verification failed
    }
}

/* file IO */

/* encrypted header
 * uc enc_metadata
 * ul m_l
 * */

typedef struct { // todo: the first information could be the length of the header, in case the header would change, it would be possible to add backwards compatibility
    ul salt_l;
    uc salt[ENC_SALT_L];
    ul nonce_l;
    uc nonce[ENC_NONCE_L];
    ul m_enc_l;
} EncNotEncryptedHeader;

static void encNotEncryptedHeaderClear(EncNotEncryptedHeader* neh) {
    ENC_ZERO(neh->salt, neh->salt_l);
    neh->salt_l = 0;
    ENC_ZERO(neh->nonce, neh->nonce_l);
    neh->nonce_l = 0;
    neh->m_enc_l = 0;
}

/* encrypts uc array then saves it to file on path
 * returns 0 if writing failed */
static int encEncryptToFile(const char* path, const uc* m, const ul m_l, const char* pw, const uc enc_metadata) {
    ul m_with_header_l = m_l + sizeof enc_metadata + sizeof m_l;
    uc* m_with_header = ENC_ALLOC(m_with_header_l);

    /* copy EncEncryptedHeader data to m_with_header */
    *m_with_header = enc_metadata;
    memcpy(m_with_header + sizeof enc_metadata, &m_l, sizeof m_l);
    memcpy(m_with_header + sizeof enc_metadata + sizeof m_l, m, m_l); //for(int i = 0; i < m_l; i++) m_with_header[i + sizeof eh] = m[i];

    EncEncryptOut encrypt_out = encEncrypt(m_with_header, m_with_header_l, pw);

    /* clear m_with_header */
    ENC_ZERO_AND_FREE(m_with_header);

    EncNotEncryptedHeader neh;
    neh.salt_l = ENC_SALT_L;
    memcpy(neh.salt, encrypt_out.salt, ENC_SALT_L); //for(int i = 0; i < ENC_SALT_L; i++) neh.salt[i] = encrypt_out.salt[i];
    neh.nonce_l = ENC_NONCE_L;
    memcpy(neh.nonce, encrypt_out.nonce, ENC_NONCE_L); //for(int i = 0; i < ENC_NONCE_L; i++) neh.nonce[i] = encrypt_out.nonce[i];
    neh.m_enc_l = encrypt_out.m_enc_l;

    FILE *f = fopen(path, "wb");
    if ( f == 0
        || ! fwrite(&neh.salt_l, 8, 1, f)
        || ! fwrite(neh.salt, neh.salt_l, 1, f)
        || ! fwrite(&neh.nonce_l, 8, 1, f)
        || ! fwrite(neh.nonce, neh.nonce_l, 1, f)
        || ! fwrite(&neh.m_enc_l, 8, 1, f)
        || ! fwrite(encrypt_out.m_enc, encrypt_out.m_enc_l, 1, f)
        || fclose(f) == EOF ) return 0;

    encEncryptOutClear(&encrypt_out);

    encNotEncryptedHeaderClear(&neh);

    // todo: do something to overwrite the internal fwrite buffer, or use an unbuffered function
    return 1;
}

EncDecryptFileOut encDecryptFile(const char *pw, const char *path) {
    EncDecryptFileOut out;
    ENC_ZERO(&out, sizeof(out));

    if ( pw == 0 || path == 0 ) { out.r = -10; return out; }

    EncNotEncryptedHeader neh;

    FILE *f = fopen(path, "rb");

    if ( f == 0
        || ! fread(&neh.salt_l, 8, 1, f)
        || ! fread(neh.salt,neh.salt_l, 1, f)
        || ! fread(&neh.nonce_l, 8, 1, f)
        || ! fread(neh.nonce, neh.nonce_l, 1, f)
        || ! fread(&neh.m_enc_l, 8, 1, f) ) return out;

    uc* m_enc = ENC_ALLOC(neh.m_enc_l);
    if ( ! fread(m_enc, neh.m_enc_l, 1, f)
        || fclose(f) == EOF ) return out;

    uc* m_with_header = encDecrypt(m_enc, neh.m_enc_l, neh.nonce, neh.salt, pw);
    out.r = m_with_header ? 1 : -1;

    encNotEncryptedHeaderClear(&neh);

    if ( out.r > 0 ) {
        out.enc_metadata = *m_with_header;
        memcpy(&out.m_l, m_with_header + sizeof out.enc_metadata, sizeof out.m_l);

        out.m = m_with_header + 1 + sizeof out.m_l;
        out.m_with_header = m_with_header;
    }

    return out;
}

/* convenience functions */

int encEncryptMessage(const char *pw, const char *message, const char *path) {
    if ( pw == 0 || message == 0 || path == 0 ) return -10;

    return encEncryptToFile(path, message, encStrLen(message), pw, ENC_METADATA_MESSAGE); // todo: check if encEncryptToFile accepts m as const and change it if it does else make message not const
}

/* todo: use something better than fseek to get rid of fseek 2GB limitation */
int encEncryptFile(const char *pw, const char *in_path, const char *out_path) {
    if ( pw == 0 || in_path == 0 || out_path == 0) return -10;

    FILE *f_in = fopen(in_path, "rb");
    if ( f_in == 0 ) return 0;

    fseek(f_in, 0l, SEEK_END);
    int f_l = ftell(f_in); // todo: error handling if -1
    fseek(f_in, 0l, SEEK_SET);

    uc* buf = ENC_ALLOC(f_l);
    fread(buf, f_l, 1, f_in);

    int r = encEncryptToFile(out_path, buf, f_l, pw, ENC_METADATA_FILE);

    ENC_ZERO_AND_FREE(buf);
    f_l = 0;

    return r;
}
