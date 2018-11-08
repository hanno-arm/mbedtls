/**
 * \file psa_compat.h
 *
 * \brief Utility functions for the use of the PSA Crypto library.
 *
 * \warning This function is not part of the public API and may
 *          change at any time.
 */
/*
 *  Copyright (C) 2006-2018, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

#if !defined(MBEDTLS_PSA_COMPAT_H)
#define MBEDTLS_PSA_COMPAT_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO)

#include "psa/crypto.h"

#include "ecp.h"
#include "md.h"

static psa_status_t mbedtls_psa_get_free_key_slot( psa_key_slot_t *key )
{
    for( psa_key_slot_t slot = 1; slot <= 32; slot++ )
    {
        if( psa_get_key_information( slot, NULL, NULL )  == PSA_ERROR_EMPTY_SLOT )
        {
            *key = slot;
            return( PSA_SUCCESS );
        }
    }
    return( PSA_ERROR_INSUFFICIENT_MEMORY );
}

static psa_algorithm_t mbedtls_psa_translate_md( mbedtls_md_type_t md_alg )
{
    switch( md_alg )
    {
#if defined(MBEDTLS_MD2_C)
    case MBEDTLS_MD_MD2:
        return( PSA_ALG_MD2 );
#endif
#if defined(MBEDTLS_MD4_C)
    case MBEDTLS_MD_MD4:
        return( PSA_ALG_MD4 );
#endif
#if defined(MBEDTLS_MD5_C)
    case MBEDTLS_MD_MD5:
        return( PSA_ALG_MD5 );
#endif
#if defined(MBEDTLS_SHA1_C)
    case MBEDTLS_MD_SHA1:
        return( PSA_ALG_SHA_1 );
#endif
#if defined(MBEDTLS_SHA256_C)
    case MBEDTLS_MD_SHA224:
        return( PSA_ALG_SHA_224 );
    case MBEDTLS_MD_SHA256:
        return( PSA_ALG_SHA_256 );
#endif
#if defined(MBEDTLS_SHA512_C)
    case MBEDTLS_MD_SHA384:
        return( PSA_ALG_SHA_384 );
    case MBEDTLS_MD_SHA512:
        return( PSA_ALG_SHA_512 );
#endif
#if defined(MBEDTLS_RIPEMD160_C)
    case MBEDTLS_MD_RIPEMD160:
        return( PSA_ALG_RIPEMD160 );
#endif
    case MBEDTLS_MD_NONE:  /* Intentional fallthrough */
    default:
        return( 0 );
    }
}

static psa_ecc_curve_t mbedtls_psa_translate_ecc_group( mbedtls_ecp_group_id grpid )
{
    switch( grpid )
    {
#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED)
        case MBEDTLS_ECP_DP_SECP192R1:
            return( PSA_ECC_CURVE_SECP192R1 );
#endif
#if defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED)
        case MBEDTLS_ECP_DP_SECP224R1:
            return( PSA_ECC_CURVE_SECP224R1 );
#endif
#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
        case MBEDTLS_ECP_DP_SECP256R1:
            return( PSA_ECC_CURVE_SECP256R1 );
#endif
#if defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED)
        case MBEDTLS_ECP_DP_SECP384R1:
            return( PSA_ECC_CURVE_SECP384R1 );
#endif
#if defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED)
        case MBEDTLS_ECP_DP_SECP521R1:
            return( PSA_ECC_CURVE_SECP521R1 );
#endif
#if defined(MBEDTLS_ECP_DP_BP256R1_ENABLED)
        case MBEDTLS_ECP_DP_BP256R1:
            return( PSA_ECC_CURVE_BRAINPOOL_P256R1 );
#endif
#if defined(MBEDTLS_ECP_DP_BP384R1_ENABLED)
        case MBEDTLS_ECP_DP_BP384R1:
            return( PSA_ECC_CURVE_BRAINPOOL_P384R1 );
#endif
#if defined(MBEDTLS_ECP_DP_BP512R1_ENABLED)
        case MBEDTLS_ECP_DP_BP512R1:
            return( PSA_ECC_CURVE_BRAINPOOL_P512R1 );
#endif
#if defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED)
        case MBEDTLS_ECP_DP_CURVE25519:
            return( PSA_ECC_CURVE_CURVE25519 );
#endif
#if defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED)
        case MBEDTLS_ECP_DP_SECP192K1:
            return( PSA_ECC_CURVE_SECP192K1 );
#endif
#if defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED)
        case MBEDTLS_ECP_DP_SECP224K1:
            return( PSA_ECC_CURVE_SECP224K1 );
#endif
#if defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED)
        case MBEDTLS_ECP_DP_SECP256K1:
            return( PSA_ECC_CURVE_SECP256K1 );
#endif
#if defined(MBEDTLS_ECP_DP_CURVE448_ENABLED)
        case MBEDTLS_ECP_DP_CURVE448:
            return( PSA_ECC_CURVE_CURVE448 );
#endif
        default:
            return( 0 );
    }
}

#endif /* MBEDTLS_USE_PSA_CRYPTO */

#endif /* MBEDTLS_PSA_COMPAT_H */
