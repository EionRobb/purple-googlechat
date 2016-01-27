#ifndef _CIPHERS_SHA1HASH_H_
#define _CIPHERS_SHA1HASH_H_

#include "cipher.h"

#define PurpleHash		PurpleCipherContext
#define purple_sha1_hash_new()	purple_cipher_context_new(purple_ciphers_find_cipher("sha1"), NULL)
#define purple_hash_append	purple_cipher_context_append
#define purple_hash_digest_to_str(ctx, data, size) \
				purple_cipher_context_digest_to_str(ctx, size, data, NULL)
#define purple_hash_destroy	purple_cipher_context_destroy

#endif /*_CIPHERS_SHA1HASH_H_*/
