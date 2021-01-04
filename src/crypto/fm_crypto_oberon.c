#include "fm_crypto.h"

int fm_crypto_sha256(word32 msg_nbytes, const byte *msg, byte out[32])
{
	return 0;
}

int fm_crypto_ckg_init(fm_crypto_ckg_context_t ctx)
{
	return 0;
}

int fm_crypto_ckg_gen_c1(fm_crypto_ckg_context_t ctx, byte out[32])
{
	return 0;
}

int fm_crypto_ckg_gen_c3(fm_crypto_ckg_context_t ctx,
			 const byte c2[89],
			 byte out[60])
{
	return 0;
}

int fm_crypto_ckg_finish(fm_crypto_ckg_context_t ctx,
			 byte p[57],
			 byte skn[32],
			 byte sks[32])
{
	return 0;
}


void fm_crypto_ckg_free(fm_crypto_ckg_context_t ctx)
{
}

int fm_crypto_generate_seedk1(byte out[32])
{
	return 0;
}

int fm_crypto_derive_server_shared_secret(const byte seeds[32],
					  const byte seedk1[32],
					  byte out[32])
{
	return 0;
}

int fm_crypto_authenticate_with_ksn(const byte serverss[32],
				    word32 msg_nbytes,
				    const byte *msg,
				    byte out[32])
{
	return 0;
}

int fm_crypto_encrypt_to_server(const byte pub[65],
				word32 msg_nbytes,
				const byte *msg,
				word32 *out_nbytes,
				byte *out)
{
	return 0;
}

int fm_crypto_verify_s2(const byte pub[65],
			word32 sig_nbytes,
			const byte *sig,
			word32 msg_nbytes,
			const byte *msg)
{
	return 0;
}

int fm_crypto_decrypt_e3(const byte serverss[32],
			 word32 e3_nbytes,
			 const byte *e3,
			 word32 *out_nbytes,
			 byte *out)
{
	return 0;
}

int fm_crypto_roll_sk(const byte sk[32], byte out[32])
{
	return 0;
}

int fm_crypto_derive_ltk(const byte skn[32], byte out[16])
{
	return 0;
}

int fm_crypto_derive_primary_or_secondary_x(const byte sk[32],
					    const byte p[57],
					    byte out[28])
{
	return 0;
}
