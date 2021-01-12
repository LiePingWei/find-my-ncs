#ifndef FM_CRYPTO_PLATFORM_H_
#define FM_CRYPTO_PLATFORM_H_

#include <stdint.h>
#include <stddef.h>

#include <ocrypto_curve_p256.h>
#include <ocrypto_curve_p224.h>
#include <ocrypto_sc_p256.h>
#include <ocrypto_sc_p224.h>

/** @brief Dummy type definition of byte */
typedef uint8_t byte;

/** @brief Dummy type definition of word32 */
typedef uint32_t word32;

#define FMN_ERROR_CRYPTO_OK             (0)
#define FMN_ERROR_CRYPTO_RNG_ERROR      (-1)
#define FMN_ERROR_CRYPTO_DEFAULT        (-2)
#define FMN_ERROR_CRYPTO_NO_VALUE_SET   (-3)
#define FMN_ERROR_CRYPTO_INVALID_INPUT  (-4)
#define FMN_ERROR_CRYPTO_INVALID_SIZE   (-5)

/**
 * @brief Type definition for union of supported private key types (scalar) 
 */
typedef struct {
	union {
		ocrypto_sc_p256 scalar_p256;
		ocrypto_sc_p224 scalar_p224;
	};
	uint8_t buffer[32];
} ecc_scalar;

/**
 * @brief Type definition for union of suportad public key types (point)
 */
typedef struct {
	union {
		ocrypto_cp_p256  point_p256;
		ocrypto_cp_p224  point_p224;
	};
	// large enough to hold both p224 and p256 publc keys
	uint8_t buffer[65];
} ecc_point;

/**
 *  @brief Type definition for structure holding key pair (private + public) */
typedef struct {
	ecc_scalar  private_key;
	ecc_point   public_key;
} ecc_key;

/**
 * @brief Type definition of a raw BIGINT structure
 * @note Exactly the same type as ecc_key
 */
typedef ecc_key mp_int;

/**
 * @brief Type definition for enumeration of curve types an operations
 */
typedef enum
{
	ECC_TYPE_NONE =         0,
	ECC_TYPE_P224 =         1,
	ECC_TYPE_P256 =         2,
	ECC_TYPE_P224_BASE =    3,
	ECC_TYPE_P256_BASE =    4,
} ecc_set_type;

typedef struct fm_crypto_ckg_context {
	ecc_key key;
	byte r1[32];
	byte r2[32];
	ecc_point p;
} *fm_crypto_ckg_context_t;

#endif /* FM_CRYPTO_PLATFORM_H_ */
