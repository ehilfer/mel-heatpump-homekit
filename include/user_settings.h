#pragma once
#warning "USING PROJECT user_settings.h"
#include <esp_system.h>

static inline int hk_hwrand_generate_block(unsigned char* buf, unsigned int sz)
{
    esp_fill_random(buf, sz);
    return 0;
}

#define WOLFSSL_SMALL_STACK

/* RNG */
#define CUSTOM_RAND_GENERATE_BLOCK hk_hwrand_generate_block
#define WC_NO_HASHDRBG

/* Platform */
#define ARDUINO
#define WOLFSSL_ESP32
#define WOLFSSL_USER_IO

/* wolfSSL hardware SHA/AES hooks (esp32-crypt.h) require WOLFSSL_ESPIDF,
 * which is only defined in full ESP-IDF builds, not Arduino.
 * Disable them for all Arduino targets. */
#define NO_ESP32_CRYPT
#define NO_WOLFSSL_ESP32_CRYPT_HASH

/* HomeKit crypto */
#define WOLFSSL_SHA512
#define HAVE_HKDF
#define HAVE_CHACHA
#define HAVE_POLY1305
#define HAVE_CURVE25519
#define HAVE_ED25519

/* SRP for pairing */
#define WOLFCRYPT_HAVE_SRP
#define WOLFCRYPT_ONLY

/* Silence wolfSSL logging */
#define NO_WOLFSSL_LOGGING

#define WOLFSSL_BASE64_ENCODE
#define FP_MAX_BITS (4096 * 2)

#if defined(ESP32) && !defined(ESP32C3)
/* ESP32 WROOM (Xtensa LX6): software fast math gives better SRP performance
 * than the default. Hardware crypto (WOLFSSL_ESPIDF) is not available in
 * Arduino builds so only pure-software optimisations are enabled here. */
#define USE_FAST_MATH
#define HAVE_ECC
#define TFM_TIMING_RESISTANT
#define ECC_TIMING_RESISTANT
#define WC_RSA_BLINDING
#endif
