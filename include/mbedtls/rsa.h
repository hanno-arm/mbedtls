/**
 * \file rsa.h
 *
 * \brief The RSA public-key cryptosystem
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
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
#ifndef MBEDTLS_RSA_H
#define MBEDTLS_RSA_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include "bignum.h"
#include "md.h"

#if defined(MBEDTLS_THREADING_C)
#include "threading.h"
#endif

/*
 * RSA Error codes
 */
#define MBEDTLS_ERR_RSA_BAD_INPUT_DATA                    -0x4080  /**< Bad input parameters to function. */
#define MBEDTLS_ERR_RSA_INVALID_PADDING                   -0x4100  /**< Input data contains invalid padding and is rejected. */
#define MBEDTLS_ERR_RSA_KEY_GEN_FAILED                    -0x4180  /**< Something failed during generation of a key. */
#define MBEDTLS_ERR_RSA_KEY_CHECK_FAILED                  -0x4200  /**< Key failed to pass the library's validity check. */
#define MBEDTLS_ERR_RSA_PUBLIC_FAILED                     -0x4280  /**< The public key operation failed. */
#define MBEDTLS_ERR_RSA_PRIVATE_FAILED                    -0x4300  /**< The private key operation failed. */
#define MBEDTLS_ERR_RSA_VERIFY_FAILED                     -0x4380  /**< The PKCS#1 verification failed. */
#define MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE                  -0x4400  /**< The output buffer for decryption is not large enough. */
#define MBEDTLS_ERR_RSA_RNG_FAILED                        -0x4480  /**< The random generator failed to generate non-zeros. */

/*
 * RSA constants
 */
#define MBEDTLS_RSA_PUBLIC      0
#define MBEDTLS_RSA_PRIVATE     1

#define MBEDTLS_RSA_PKCS_V15    0
#define MBEDTLS_RSA_PKCS_V21    1

#define MBEDTLS_RSA_SIGN        1
#define MBEDTLS_RSA_CRYPT       2

#define MBEDTLS_RSA_SALT_LEN_ANY    -1

/*
 * The above constants may be used even if the RSA module is compile out,
 * eg for alternative (PKCS#11) RSA implemenations in the PK layers.
 */
#if defined(MBEDTLS_RSA_C)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Helper functions for RSA-related operations on MPI's.
 */

/**
 * \brief          Compute RSA prime moduli P, Q from public modulus N=PQ
&                  and a pair of private and public key.
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param N        RSA modulus N = PQ, with P, Q to be found
 * \param D        RSA private exponent
 * \param E        RSA public exponent
 * \param f_rng    PRNG to be used for randomization, or NULL
 * \param p_rng    PRNG context for f_rng, or NULL
 * \param P        Pointer to MPI holding first prime factor of N on success
 * \param Q        Pointer to MPI holding second prime factor of N on success
 *
 * \return         - 0 if successful. In this case, P and Q constitute a
 *                   factorization of N, and it is guaranteed that D and E
 *                   are indeed modular inverses modulo P-1 and modulo Q-1.
 *                   The values of N, D and E are unchanged. It is checked
 *                   that P, Q are prime if a PRNG is provided.
 *                 - A non-zero error code otherwise. In this case, the values
 *                   of N, D, E are undefined.
 *
 * \note           The input MPI's are deliberately not declared as constant
 *                 and may therefore be used for in-place calculations by
 *                 the implementation. In particular, their values can be
 *                 corrupted when the function fails. If the user cannot
 *                 tolerate this, he has to make copies of the MPI's prior
 *                 to calling this function. See \c mbedtls_mpi_copy for this.
 */
int mbedtls_rsa_deduce_moduli( mbedtls_mpi *N, mbedtls_mpi *D, mbedtls_mpi *E,
                     int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                     mbedtls_mpi *P, mbedtls_mpi *Q );

/**
 * \brief          Compute RSA private exponent from
 *                 prime moduli and public key.
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param P        First prime factor of RSA modulus
 * \param Q        Second prime factor of RSA modulus
 * \param E        RSA public exponent
 * \param D        Pointer to MPI holding the private exponent on success.
 *
 * \note           This function does not check whether P and Q are primes.
 *
 * \return         - 0 if successful. In this case, D is set to a simultaneous
 *                   modular inverse of E modulo both P-1 and Q-1.
 *                 - A non-zero error code otherwise. In this case, the values
 *                   of P, Q, E are undefined.
 *
 * \note           The input MPI's are deliberately not declared as constant
 *                 and may therefore be used for in-place calculations by
 *                 the implementation. In particular, their values can be
 *                 corrupted when the function fails. If the user cannot
 *                 tolerate this, he has to make copies of the MPI's prior
 *                 to calling this function. See \c mbedtls_mpi_copy for this.
 */
int mbedtls_rsa_deduce_private( mbedtls_mpi *P, mbedtls_mpi *Q, mbedtls_mpi *E,
                                mbedtls_mpi *D );


/**
 * \brief          Generate RSA-CRT parameters
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param P        First prime factor of N
 * \param Q        Second prime factor of N
 * \param D        RSA private exponent
 * \param DP       Output variable for D modulo P-1
 * \param DQ       Output variable for D modulo Q-1
 * \param QP       Output variable for the modular inverse of Q modulo P.
 *
 * \return         0 on success, non-zero error code otherwise.
 *
 */
int mbedtls_rsa_deduce_crt( const mbedtls_mpi *P, const mbedtls_mpi *Q,
                            const mbedtls_mpi *D, mbedtls_mpi *DP,
                            mbedtls_mpi *DQ, mbedtls_mpi *QP );


/**
 * \brief          Check validity of core RSA parameters
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param N        RSA modulus N = PQ
 * \param P        First prime factor of N
 * \param Q        Second prime factor of N
 * \param D        RSA private exponent
 * \param E        RSA public exponent
 * \param f_rng    PRNG to be used for randomization, or NULL
 * \param p_rng    PRNG context for f_rng, or NULL
 *
 * \return         - 0 if the following conditions are satisfied:
 *                   - N = PQ if N,P,Q != NULL
 *                   - P>1, Q>1 and D and E are modular inverses
 *                     modulo P-1 and Q-1 if D,E,P,Q != NULL
 *                   - P prime if f_rng, P != NULL
 *                   - Q prime if f_rng, Q != NULL
 *                 - MBEDTLS_ERR_RSA_KEY_CHECK_FAILED if check failed,
 *                   potentially including MBEDTLS_ERR_MPI_XXX if some
 *                   MPI calculations failed.
 *
 * \note           The function can be used with a restricted set of arguments
 *                 to perform specific checks only. E.g., calling it with
 *                 (-,P,-,-,-) and a PRNG amounts to a primality check for P.
 *
 */
int mbedtls_rsa_validate( const mbedtls_mpi *N, const mbedtls_mpi *P,
                          const mbedtls_mpi *Q, const mbedtls_mpi *D,
                          const mbedtls_mpi *E,
                          int (*f_rng)(void *, unsigned char *, size_t),
                          void *p_rng );

/**
 * \brief          Check validity of RSA-CRT parameters
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param P        First prime factor of RSA modulus
 * \param Q        Second prime factor of RSA modulus
 * \param D        RSA private exponent
 * \param DP       MPI to check for D modulo P-1
 * \param DQ       MPI to check for D modulo P-1
 * \param QP       MPI to check for the modular inverse of Q modulo P.
 *
 * \return         - 0 if the following conditions are satisfied:
 *                   - D = DP mod P-1 if P, D, DP != NULL
 *                   - Q = DQ mod P-1 if P, D, DQ != NULL
 *                   - QP = Q^-1 mod P if P, Q, QP != NULL
 *                 - MBEDTLS_ERR_RSA_KEY_CHECK_FAILED if check failed,
 *                   potentially including MBEDTLS_ERR_MPI_XXX if some
 *                   MPI calculations failed.
 *                 - MBEDTLS_ERR_RSA_BAD_INPUT_DATA if insufficient
 *                   data was provided to check DP, DQ or QP.
 *
 * \note           The function can be used with a restricted set of arguments
 *                 to perform specific checks only. E.g., calling it with the
 *                 parameters (P, -, D, DP, -, -) will check DP = D mod P-1.
 */
int mbedtls_rsa_validate_crt( const mbedtls_mpi *P,  const mbedtls_mpi *Q,
                              const mbedtls_mpi *D,  const mbedtls_mpi *DP,
                              const mbedtls_mpi *DQ, const mbedtls_mpi *QP );

/**
 * Implementation of RSA interface
 */

/**
 * \brief          RSA context structure
 */
typedef struct
{
    int ver;                    /*!<  always 0          */
    size_t len;                 /*!<  size(N) in chars  */

    mbedtls_mpi N;                      /*!<  public modulus    */
    mbedtls_mpi E;                      /*!<  public exponent   */

    mbedtls_mpi D;                      /*!<  private exponent  */
    mbedtls_mpi P;                      /*!<  1st prime factor  */
    mbedtls_mpi Q;                      /*!<  2nd prime factor  */
    mbedtls_mpi DP;                     /*!<  D % (P - 1)       */
    mbedtls_mpi DQ;                     /*!<  D % (Q - 1)       */
    mbedtls_mpi QP;                     /*!<  1 / (Q % P)       */

    mbedtls_mpi RN;                     /*!<  cached R^2 mod N  */
    mbedtls_mpi RP;                     /*!<  cached R^2 mod P  */
    mbedtls_mpi RQ;                     /*!<  cached R^2 mod Q  */

    mbedtls_mpi Vi;                     /*!<  cached blinding value     */
    mbedtls_mpi Vf;                     /*!<  cached un-blinding value  */

    int padding;                /*!<  MBEDTLS_RSA_PKCS_V15 for 1.5 padding and
                                      MBEDTLS_RSA_PKCS_v21 for OAEP/PSS         */
    int hash_id;                /*!<  Hash identifier of mbedtls_md_type_t as
                                      specified in the mbedtls_md.h header file
                                      for the EME-OAEP and EMSA-PSS
                                      encoding                          */
#if defined(MBEDTLS_THREADING_C)
    mbedtls_threading_mutex_t mutex;    /*!<  Thread-safety mutex       */
#endif
}
mbedtls_rsa_context;

/**
 * \brief          Initialize an RSA context
 *
 *                 Note: Set padding to MBEDTLS_RSA_PKCS_V21 for the RSAES-OAEP
 *                 encryption scheme and the RSASSA-PSS signature scheme.
 *
 * \param ctx      RSA context to be initialized
 * \param padding  MBEDTLS_RSA_PKCS_V15 or MBEDTLS_RSA_PKCS_V21
 * \param hash_id  MBEDTLS_RSA_PKCS_V21 hash identifier
 *
 * \note           The hash_id parameter is actually ignored
 *                 when using MBEDTLS_RSA_PKCS_V15 padding.
 *
 * \note           Choice of padding mode is strictly enforced for private key
 *                 operations, since there might be security concerns in
 *                 mixing padding modes. For public key operations it's merely
 *                 a default value, which can be overriden by calling specific
 *                 rsa_rsaes_xxx or rsa_rsassa_xxx functions.
 *
 * \note           The chosen hash is always used for OEAP encryption.
 *                 For PSS signatures, it's always used for making signatures,
 *                 but can be overriden (and always is, if set to
 *                 MBEDTLS_MD_NONE) for verifying them.
 */
void mbedtls_rsa_init( mbedtls_rsa_context *ctx,
               int padding,
               int hash_id);

/**
 * \brief          Set padding for an already initialized RSA context
 *                 See \c mbedtls_rsa_init() for details.
 *
 * \param ctx      RSA context to be set
 * \param padding  MBEDTLS_RSA_PKCS_V15 or MBEDTLS_RSA_PKCS_V21
 * \param hash_id  MBEDTLS_RSA_PKCS_V21 hash identifier
 */
void mbedtls_rsa_set_padding( mbedtls_rsa_context *ctx, int padding, int hash_id);

/**
 * \brief          Generate an RSA keypair
 *
 * \param ctx      RSA context that will hold the key
 * \param f_rng    RNG function
 * \param p_rng    RNG parameter
 * \param nbits    size of the public key in bits
 * \param exponent public exponent (e.g., 65537)
 *
 * \note           mbedtls_rsa_init() must be called beforehand to setup
 *                 the RSA context.
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 */
int mbedtls_rsa_gen_key( mbedtls_rsa_context *ctx,
                 int (*f_rng)(void *, unsigned char *, size_t),
                 void *p_rng,
                 unsigned int nbits, int exponent );

/**
 * \brief          Check a public RSA key
 *
 * \param ctx      RSA context to be checked
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 */
int mbedtls_rsa_check_pubkey( const mbedtls_rsa_context *ctx );

/**
 * \brief          Check a private RSA key
 *
 * \param ctx      RSA context to be checked
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 */
int mbedtls_rsa_check_privkey( const mbedtls_rsa_context *ctx );

/**
 * \brief          Check a public-private RSA key pair.
 *                 Check each of the contexts, and make sure they match.
 *
 * \param pub      RSA context holding the public key
 * \param prv      RSA context holding the private key
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 */
int mbedtls_rsa_check_pub_priv( const mbedtls_rsa_context *pub, const mbedtls_rsa_context *prv );

/**
 * \brief          Do an RSA public key operation
 *
 * \param ctx      RSA context
 * \param input    input buffer
 * \param output   output buffer
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           This function does NOT take care of message
 *                 padding. Also, be sure to set input[0] = 0 or ensure that
 *                 input is smaller than N.
 *
 * \note           The input and output buffers must be large
 *                 enough (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_public( mbedtls_rsa_context *ctx,
                const unsigned char *input,
                unsigned char *output );

/**
 * \brief          Do an RSA private key operation
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Needed for blinding)
 * \param p_rng    RNG parameter
 * \param input    input buffer
 * \param output   output buffer
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The input and output buffers must be large
 *                 enough (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_private( mbedtls_rsa_context *ctx,
                 int (*f_rng)(void *, unsigned char *, size_t),
                 void *p_rng,
                 const unsigned char *input,
                 unsigned char *output );

/**
 * \brief          Generic wrapper to perform a PKCS#1 encryption using the
 *                 mode from the context. Add the message padding, then do an
 *                 RSA operation.
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Needed for padding and PKCS#1 v2.1 encoding
 *                               and MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param ilen     contains the plaintext length
 * \param input    buffer holding the data to be encrypted
 * \param output   buffer that will hold the ciphertext
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The output buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_pkcs1_encrypt( mbedtls_rsa_context *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng,
                       int mode, size_t ilen,
                       const unsigned char *input,
                       unsigned char *output );

/**
 * \brief          Perform a PKCS#1 v1.5 encryption (RSAES-PKCS1-v1_5-ENCRYPT)
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Needed for padding and MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param ilen     contains the plaintext length
 * \param input    buffer holding the data to be encrypted
 * \param output   buffer that will hold the ciphertext
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The output buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_rsaes_pkcs1_v15_encrypt( mbedtls_rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng,
                                 int mode, size_t ilen,
                                 const unsigned char *input,
                                 unsigned char *output );

/**
 * \brief          Perform a PKCS#1 v2.1 OAEP encryption (RSAES-OAEP-ENCRYPT)
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Needed for padding and PKCS#1 v2.1 encoding
 *                               and MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param label    buffer holding the custom label to use
 * \param label_len contains the label length
 * \param ilen     contains the plaintext length
 * \param input    buffer holding the data to be encrypted
 * \param output   buffer that will hold the ciphertext
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The output buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_rsaes_oaep_encrypt( mbedtls_rsa_context *ctx,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng,
                            int mode,
                            const unsigned char *label, size_t label_len,
                            size_t ilen,
                            const unsigned char *input,
                            unsigned char *output );

/**
 * \brief          Generic wrapper to perform a PKCS#1 decryption using the
 *                 mode from the context. Do an RSA operation, then remove
 *                 the message padding
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Only needed for MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param olen     will contain the plaintext length
 * \param input    buffer holding the encrypted data
 * \param output   buffer that will hold the plaintext
 * \param output_max_len    maximum length of the output buffer
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The output buffer length \c output_max_len should be
 *                 as large as the size ctx->len of ctx->N (eg. 128 bytes
 *                 if RSA-1024 is used) to be able to hold an arbitrary
 *                 decrypted message. If it is not large enough to hold
 *                 the decryption of the particular ciphertext provided, 
 *                 the function will return MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE.
 *
 * \note           The input buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_pkcs1_decrypt( mbedtls_rsa_context *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng,
                       int mode, size_t *olen,
                       const unsigned char *input,
                       unsigned char *output,
                       size_t output_max_len );

/**
 * \brief          Perform a PKCS#1 v1.5 decryption (RSAES-PKCS1-v1_5-DECRYPT)
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Only needed for MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param olen     will contain the plaintext length
 * \param input    buffer holding the encrypted data
 * \param output   buffer that will hold the plaintext
 * \param output_max_len    maximum length of the output buffer
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The output buffer length \c output_max_len should be
 *                 as large as the size ctx->len of ctx->N (eg. 128 bytes
 *                 if RSA-1024 is used) to be able to hold an arbitrary
 *                 decrypted message. If it is not large enough to hold
 *                 the decryption of the particular ciphertext provided, 
 *                 the function will return MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE.
 *
 * \note           The input buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_rsaes_pkcs1_v15_decrypt( mbedtls_rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng,
                                 int mode, size_t *olen,
                                 const unsigned char *input,
                                 unsigned char *output,
                                 size_t output_max_len );

/**
 * \brief          Perform a PKCS#1 v2.1 OAEP decryption (RSAES-OAEP-DECRYPT)
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Only needed for MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param label    buffer holding the custom label to use
 * \param label_len contains the label length
 * \param olen     will contain the plaintext length
 * \param input    buffer holding the encrypted data
 * \param output   buffer that will hold the plaintext
 * \param output_max_len    maximum length of the output buffer
 *
 * \return         0 if successful, or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The output buffer length \c output_max_len should be
 *                 as large as the size ctx->len of ctx->N (eg. 128 bytes
 *                 if RSA-1024 is used) to be able to hold an arbitrary
 *                 decrypted message. If it is not large enough to hold
 *                 the decryption of the particular ciphertext provided, 
 *                 the function will return MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE.
 *
 * \note           The input buffer must be as large as the size 
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_rsaes_oaep_decrypt( mbedtls_rsa_context *ctx,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng,
                            int mode,
                            const unsigned char *label, size_t label_len,
                            size_t *olen,
                            const unsigned char *input,
                            unsigned char *output,
                            size_t output_max_len );

/**
 * \brief          Generic wrapper to perform a PKCS#1 signature using the
 *                 mode from the context. Do a private RSA operation to sign
 *                 a message digest
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Needed for PKCS#1 v2.1 encoding and for
 *                               MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param md_alg   a MBEDTLS_MD_XXX (use MBEDTLS_MD_NONE for signing raw data)
 * \param hashlen  message digest length (for MBEDTLS_MD_NONE only)
 * \param hash     buffer holding the message digest
 * \param sig      buffer that will hold the ciphertext
 *
 * \return         0 if the signing operation was successful,
 *                 or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The "sig" buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 *
 * \note           In case of PKCS#1 v2.1 encoding, see comments on
 * \note           \c mbedtls_rsa_rsassa_pss_sign() for details on md_alg and hash_id.
 */
int mbedtls_rsa_pkcs1_sign( mbedtls_rsa_context *ctx,
                    int (*f_rng)(void *, unsigned char *, size_t),
                    void *p_rng,
                    int mode,
                    mbedtls_md_type_t md_alg,
                    unsigned int hashlen,
                    const unsigned char *hash,
                    unsigned char *sig );

/**
 * \brief          Perform a PKCS#1 v1.5 signature (RSASSA-PKCS1-v1_5-SIGN)
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Only needed for MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param md_alg   a MBEDTLS_MD_XXX (use MBEDTLS_MD_NONE for signing raw data)
 * \param hashlen  message digest length (for MBEDTLS_MD_NONE only)
 * \param hash     buffer holding the message digest
 * \param sig      buffer that will hold the ciphertext
 *
 * \return         0 if the signing operation was successful,
 *                 or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The "sig" buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_rsassa_pkcs1_v15_sign( mbedtls_rsa_context *ctx,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng,
                               int mode,
                               mbedtls_md_type_t md_alg,
                               unsigned int hashlen,
                               const unsigned char *hash,
                               unsigned char *sig );

/**
 * \brief          Perform a PKCS#1 v2.1 PSS signature (RSASSA-PSS-SIGN)
 *
 * \param ctx      RSA context
 * \param f_rng    RNG function (Needed for PKCS#1 v2.1 encoding and for
 *                               MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param md_alg   a MBEDTLS_MD_XXX (use MBEDTLS_MD_NONE for signing raw data)
 * \param hashlen  message digest length (for MBEDTLS_MD_NONE only)
 * \param hash     buffer holding the message digest
 * \param sig      buffer that will hold the ciphertext
 *
 * \return         0 if the signing operation was successful,
 *                 or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The "sig" buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 *
 * \note           The hash_id in the RSA context is the one used for the
 *                 encoding. md_alg in the function call is the type of hash
 *                 that is encoded. According to RFC 3447 it is advised to
 *                 keep both hashes the same.
 */
int mbedtls_rsa_rsassa_pss_sign( mbedtls_rsa_context *ctx,
                         int (*f_rng)(void *, unsigned char *, size_t),
                         void *p_rng,
                         int mode,
                         mbedtls_md_type_t md_alg,
                         unsigned int hashlen,
                         const unsigned char *hash,
                         unsigned char *sig );

/**
 * \brief          Generic wrapper to perform a PKCS#1 verification using the
 *                 mode from the context. Do a public RSA operation and check
 *                 the message digest
 *
 * \param ctx      points to an RSA public key
 * \param f_rng    RNG function (Only needed for MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param md_alg   a MBEDTLS_MD_XXX (use MBEDTLS_MD_NONE for signing raw data)
 * \param hashlen  message digest length (for MBEDTLS_MD_NONE only)
 * \param hash     buffer holding the message digest
 * \param sig      buffer holding the ciphertext
 *
 * \return         0 if the verify operation was successful,
 *                 or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The "sig" buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 *
 * \note           In case of PKCS#1 v2.1 encoding, see comments on
 *                 \c mbedtls_rsa_rsassa_pss_verify() about md_alg and hash_id.
 */
int mbedtls_rsa_pkcs1_verify( mbedtls_rsa_context *ctx,
                      int (*f_rng)(void *, unsigned char *, size_t),
                      void *p_rng,
                      int mode,
                      mbedtls_md_type_t md_alg,
                      unsigned int hashlen,
                      const unsigned char *hash,
                      const unsigned char *sig );

/**
 * \brief          Perform a PKCS#1 v1.5 verification (RSASSA-PKCS1-v1_5-VERIFY)
 *
 * \param ctx      points to an RSA public key
 * \param f_rng    RNG function (Only needed for MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param md_alg   a MBEDTLS_MD_XXX (use MBEDTLS_MD_NONE for signing raw data)
 * \param hashlen  message digest length (for MBEDTLS_MD_NONE only)
 * \param hash     buffer holding the message digest
 * \param sig      buffer holding the ciphertext
 *
 * \return         0 if the verify operation was successful,
 *                 or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The "sig" buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 */
int mbedtls_rsa_rsassa_pkcs1_v15_verify( mbedtls_rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng,
                                 int mode,
                                 mbedtls_md_type_t md_alg,
                                 unsigned int hashlen,
                                 const unsigned char *hash,
                                 const unsigned char *sig );

/**
 * \brief          Perform a PKCS#1 v2.1 PSS verification (RSASSA-PSS-VERIFY)
 *                 (This is the "simple" version.)
 *
 * \param ctx      points to an RSA public key
 * \param f_rng    RNG function (Only needed for MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param md_alg   a MBEDTLS_MD_XXX (use MBEDTLS_MD_NONE for signing raw data)
 * \param hashlen  message digest length (for MBEDTLS_MD_NONE only)
 * \param hash     buffer holding the message digest
 * \param sig      buffer holding the ciphertext
 *
 * \return         0 if the verify operation was successful,
 *                 or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The "sig" buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 *
 * \note           The hash_id in the RSA context is the one used for the
 *                 verification. md_alg in the function call is the type of
 *                 hash that is verified. According to RFC 3447 it is advised to
 *                 keep both hashes the same. If hash_id in the RSA context is
 *                 unset, the md_alg from the function call is used.
 */
int mbedtls_rsa_rsassa_pss_verify( mbedtls_rsa_context *ctx,
                           int (*f_rng)(void *, unsigned char *, size_t),
                           void *p_rng,
                           int mode,
                           mbedtls_md_type_t md_alg,
                           unsigned int hashlen,
                           const unsigned char *hash,
                           const unsigned char *sig );

/**
 * \brief          Perform a PKCS#1 v2.1 PSS verification (RSASSA-PSS-VERIFY)
 *                 (This is the version with "full" options.)
 *
 * \param ctx      points to an RSA public key
 * \param f_rng    RNG function (Only needed for MBEDTLS_RSA_PRIVATE)
 * \param p_rng    RNG parameter
 * \param mode     MBEDTLS_RSA_PUBLIC or MBEDTLS_RSA_PRIVATE
 * \param md_alg   a MBEDTLS_MD_XXX (use MBEDTLS_MD_NONE for signing raw data)
 * \param hashlen  message digest length (for MBEDTLS_MD_NONE only)
 * \param hash     buffer holding the message digest
 * \param mgf1_hash_id message digest used for mask generation
 * \param expected_salt_len Length of the salt used in padding, use
 *                 MBEDTLS_RSA_SALT_LEN_ANY to accept any salt length
 * \param sig      buffer holding the ciphertext
 *
 * \return         0 if the verify operation was successful,
 *                 or an MBEDTLS_ERR_RSA_XXX error code
 *
 * \note           The "sig" buffer must be as large as the size
 *                 of ctx->N (eg. 128 bytes if RSA-1024 is used).
 *
 * \note           The hash_id in the RSA context is ignored.
 */
int mbedtls_rsa_rsassa_pss_verify_ext( mbedtls_rsa_context *ctx,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng,
                               int mode,
                               mbedtls_md_type_t md_alg,
                               unsigned int hashlen,
                               const unsigned char *hash,
                               mbedtls_md_type_t mgf1_hash_id,
                               int expected_salt_len,
                               const unsigned char *sig );

/**
 * \brief          Copy the components of an RSA context
 *
 * \param dst      Destination context
 * \param src      Source context
 *
 * \return         0 on success,
 *                 MBEDTLS_ERR_MPI_ALLOC_FAILED on memory allocation failure
 */
int mbedtls_rsa_copy( mbedtls_rsa_context *dst, const mbedtls_rsa_context *src );

/**
 * \brief          Free the components of an RSA key
 *
 * \param ctx      RSA Context to free
 */
void mbedtls_rsa_free( mbedtls_rsa_context *ctx );

/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if the test failed
 */
int mbedtls_rsa_self_test( int verbose );

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_RSA_C */

#endif /* rsa.h */
