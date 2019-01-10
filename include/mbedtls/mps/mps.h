/**
 * \file mps.h
 *
 * \brief Message Processing Stack
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

#ifndef MBEDTLS_MPS_H
#define MBEDTLS_MPS_H

#include "common.h"
#include "transport.h"
#include "transform.h"
#include "error.h"
#include "reader.h"
#include "writer.h"
#include "layer3.h"

#include "../timing.h"

/*! (DTLS only) The maximum number of messages in a flight.
 *
 *  This is used to allocate space for retransmission backup handles. */
#define MBEDTLS_MPS_MAX_FLIGHT_LENGTH         5

/*!< The maximum allowed handshake sequence number.
 *   This must not be larger than #MBEDTLS_MPS_HS_SEQ_MAX. */
#define MBEDTLS_MPS_LIMIT_SEQUENCE_NUMBER MBEDTLS_MPS_HS_SEQ_MAX

/*!< (DTLS only) The maximum number of future messages to be buffered. */
#define MBEDTLS_MPS_FUTURE_MESSAGE_BUFFERS 4

#define MBEDTLS_MPS_RETRANSMISSION_CALLBACK_SUCCESS 0
#define MBEDTLS_MPS_RETRANSMISSION_CALLBACK_PAUSE   1

/*! The type of reassembly/buffering states handshake messages.
 *
 *  Possible values are:
 *  - #MPS_REASSEMBLY_NONE
 *    Reassembly hasn't started.
 *  - #MPS_REASSEMBLY_NO_FRAGMENTATION
 *    The message has been received in a single fragment
 *    and no reassembly was necessary; a reader is available
 *    which gives access to the contents.
 *  - #MPS_REASSEMBLY_WINDOW
 *    Some fragments of the message have been received
 *    and reassembly is in progress.
 *
 * The state #MPS_REASSEMBLY_NO_FRAGMENTATION is only
 * possible for the next message, as for future messages
 * we need to make a copy of the Layer 3 data anyway.
 *
 * NOTE: There are more alternatives, for example
 *       one could always wait until a new fragment
 *       comes in which continues the initial part
 *       of the message that has already been received;
 *       this way, no additional buffers would be needed
 *       (if the parsing routines make use of pausing).
 *       However, this seems to be suitable only for very
 *       reliable networks, or in DTLS-1.3 where a more
 *       elaborate acknowledgement scheme is available.
 *
 */
typedef uint8_t mbedtls_mps_msg_reassembly_state;
#define MPS_REASSEMBLY_NONE             ( (mbedtls_mps_msg_reassembly_state) 0 )
#define MPS_REASSEMBLY_NO_FRAGMENTATION ( (mbedtls_mps_msg_reassembly_state) 1 )
#define MPS_REASSEMBLY_WINDOW           ( (mbedtls_mps_msg_reassembly_state) 2 )

/*! Messages of the last incoming flight are tagged with values of this type
 *  to indicate whether a re-receipt should lead to retransmission of our
 *  own last outgoing flight, or not. */
typedef uint8_t mbedtls_mps_retransmission_detection_state;
#define MBEDTLS_MPS_RETRANSMISSION_DETECTION_ENABLED  \
    ( (mbedtls_mps_retransmission_detection_state) 0 )
#define MBEDTLS_MPS_RETRANSMISSION_DETECTION_ON_HOLD  \
    ( (mbedtls_mps_retransmission_detection_state) 1 )

/*! The enumeration of (D)TLS alerts. */
typedef uint8_t mbedtls_mps_alert_t;
/* TODO: Add (D)TLS alert types here, see ssl.h.
 * Either use the same constants as in the standard,
 * or keep them abstract here and provide a translation
 * function. */

/*! The type of reasons for MPS being blocked.
 *
 *  Possible values are:
 *  - #MBEDTLS_MPS_ERROR_NONE
 *    No blocking reason has been recorded.
 *  - #MBEDTLS_MPS_ERROR_ALERT_SENT
 *    A fatal alert has been sent by the user.
 *  - #MBEDTLS_MPS_ERROR_ALERT_RECEIVED
 *    A fatal alert has been received from the peer.
 *  - #MBEDTLS_MPS_ERROR_INTERNAL_ERROR
 *    An internal error lead to blocking MPS.
 */
typedef uint8_t mbedtls_mps_blocking_reason_t;
#define MBEDTLS_MPS_ERROR_UNKNOWN        ( (mbedtls_mps_blocking_reason_t) 0 )
#define MBEDTLS_MPS_ERROR_ALERT_SENT     ( (mbedtls_mps_blocking_reason_t) 1 )
#define MBEDTLS_MPS_ERROR_ALERT_RECEIVED ( (mbedtls_mps_blocking_reason_t) 2 )
#define MBEDTLS_MPS_ERROR_INTERNAL_ERROR ( (mbedtls_mps_blocking_reason_t) 3 )

typedef struct
{
    /*! The reason for blocking MPS. */
    mbedtls_mps_blocking_reason_t reason;

    /*! This is a union indexed by \c reason giving
     *  more information about the reason for blocking MPS.
     *  Specifically:
     * - If \c avail is #MBEDTLS_MPS_ERROR_ALERT_SENT or
     *   #MBEDTLS_MPS_ERROR_ALERT_RECEIVED, \c info.alert is valid.
     *   and contains the type of alert sent or received, respectively.
     * - If \c avail is #MBEDTLS_MPS_ERROR_INTERNAL_ERROR,
     *   \c avail.err is valid and contains the internal error
     *   code that lead to blocking MPS.
     * - Otherwise, \c info is invalid.
     */
    union
    {
        mbedtls_mps_alert_t alert;
        int err;
    } info;
} mbedtls_mps_blocking_info_t;

/*! The type of MPS connection states.
 *
 *  Possible values are:
 *  - #MBEDTLS_MPS_STATE_OPEN
 *    The connection is open.
 *  - #MBEDTLS_MPS_STATE_WRITE_ONLY
 *    The peer has closed its writing side, but we may still send data.
 *  - #MBEDTLS_MPS_STATE_READ_ONLY
 *    We have closed the writing side, but the peer may still send data.
 *  - #MBEDTLS_MPS_STATE_CLOSED
 *    The connection is fully closed.
 *  - #MBEDTLS_MPS_STATE_BLOCKED
 *    The MPS is blocked after an error.
 */
typedef uint8_t mbedtls_mps_connection_state_t;
#define MBEDTLS_MPS_STATE_OPEN       ( (mbedtls_mps_connection_state_t) 0 )
#define MBEDTLS_MPS_STATE_WRITE_ONLY ( (mbedtls_mps_connection_state_t) 1 )
#define MBEDTLS_MPS_STATE_READ_ONLY  ( (mbedtls_mps_connection_state_t) 2 )
#define MBEDTLS_MPS_STATE_CLOSED     ( (mbedtls_mps_connection_state_t) 3 )
#define MBEDTLS_MPS_STATE_BLOCKED    ( (mbedtls_mps_connection_state_t) 4 )

/*! The type of flight exchange states.
 *
 *  Possible values are:
 *  - #MBEDTLS_MPS_FLIGHT_DONE
 *    No flight exchange is in progress.
 *  - #MBEDTLS_MPS_FLIGHT_AWAIT
 *    We're waiting for the first message of the next flight from the peer.
 *    In this state, we're not yet sure whether the peer has fully received
 *    our last outgoing flight, and we retransmit the latter on timeout.
 *  - #MBEDTLS_MPS_FLIGHT_RECEIVE
 *    We're receiving the next flight from the peer. This is different
 *    from #MBEDTLS_MPS_FLIGHT_AWAIT in that we must already have received
 *    some part of the next incoming flight, witnessing that the peer has
 *    received our last outgoing flight. In this mode, we're sending
 *    retransmission requests on a timeout, but not necessarily fully
 *    retransmit our last outgoing flight.
 *  - #MBEDTLS_MPS_FLIGHT_SEND,
 *    We're in the process of sending our next outgoing flight.
 *  - #MBEDTLS_MPS_FLIGHT_FINALIZE
 *    The flight exchange has been completed with an outgoing flight of ours,
 *    but we're holding it back in case the peer didn't receive it.
 */
typedef uint8_t mbedtls_mps_flight_state_t;
#define MBEDTLS_MPS_FLIGHT_DONE     ( (mbedtls_mps_flight_state_t) 0 )
#define MBEDTLS_MPS_FLIGHT_AWAIT    ( (mbedtls_mps_flight_state_t) 1 )
#define MBEDTLS_MPS_FLIGHT_RECEIVE  ( (mbedtls_mps_flight_state_t) 2 )
#define MBEDTLS_MPS_FLIGHT_SEND     ( (mbedtls_mps_flight_state_t) 3 )
#define MBEDTLS_MPS_FLIGHT_FINALIZE ( (mbedtls_mps_flight_state_t) 4 )

/**
 * Retransmission state
 */

/*! The type of retransmission states.
 *
 *  Possible values are:
 *  - #MBEDTLS_MPS_RETRANSMIT_NONE
 *    No retransmission or retransmission request ongoing.
 *  - #MBEDTLS_MPS_RETRANSMIT_RESEND
 *    We are currently resending our last outgoing flight.
 *    This happens in flight-exchange states
 *    #MBEDTLS_MPS_FLIGHT_AWAIT or #MBEDTLS_MPS_FLIGHT_FINALIZE.
 *  - #MBEDTLS_MPS_RETRANSMIT_REQUEST_RESEND
 *    We are in flight-exchange state #MBEDTLS_MPS_FLIGHT_RECEIVE,
 *    observed a disruption during the receipt of the next incoming flight,
 *    and are requesting a retransmission from the peer.
 *    In DTLS 1.0 and 1.2, this is done by retransmitting our last
 *    outgoing flight entirely (so the handling of this state is
 *    the same as the one for #MBEDTLS_MPS_RETRANSMIT_RESEND),
 *    which introduces an unnecessary network load because we already
 *    know that the peer has fully received our flight (otherwise
 *    it wouldn't have started sending). In DTLS 1.3, this
 *    state can be more efficiently handled by sending
 *    ACK messages which indicate to the peer which messages
 *    we have already received.
 */
typedef uint8_t mbedtls_mps_retransmit_state_t;
#define MBEDTLS_MPS_RETRANSMIT_NONE             \
    ( (mbedtls_mps_retransmit_state_t) 0 )
#define MBEDTLS_MPS_RETRANSMIT_RESEND           \
    ( (mbedtls_mps_retransmit_state_t) 1 )
#define MBEDTLS_MPS_RETRANSMIT_REQUEST_RESEND   \
    ( (mbedtls_mps_retransmit_state_t) 2 )

/*! The type of message flags indicating their contribution
 *  to the current flight and flight exchange.
 *
 * Bit(s)   Meaning
 * 0 .. 1   Contribution to flight & handshake:
 *          0: No contribution
 *          1: Contributes to flight
 *          2: Ends flight
 *          3: Ends handshake
 *
 * 2 .. 6   Reserved
 *
 * 7        Validity flag
 *          Used to determine if the flags have been set
 *          This bit realized the `Optional` nature of the
 *          `Options` variable in the read state.
 */
typedef uint8_t mbedtls_mps_msg_flags;
#define MBEDTLS_MPS_FLAGS_MASK       ( (mbedtls_mps_msg_flags) ( 1u << 7 ) )
#define MBEDTLS_MPS_FLIGHT_MASK      ( (mbedtls_mps_msg_flags) ( 3u << 0 ) )
#define MBEDTLS_MPS_FLIGHT_NONE      ( (mbedtls_mps_msg_flags) ( 0u << 0 ) )
#define MBEDTLS_MPS_FLIGHT_ADD       ( (mbedtls_mps_msg_flags) ( 1u << 0 ) )
#define MBEDTLS_MPS_FLIGHT_END       ( (mbedtls_mps_msg_flags) ( 2u << 0 ) )
#define MBEDTLS_MPS_FLIGHT_FINISHED  ( (mbedtls_mps_msg_flags) ( 3u << 0 ) )

/*! Type of bitflags signaling external dependencies.
 *
 * Defined bits are:
 * - #MBEDTLS_MPS_BLOCK_READ
 *   Progress can only be made when the underlying transport
 *   has data ready to be read.
 * - #MBEDTLS_MPS_BLOCK_WRITE
 *   Progress can only be made when the underlying transport
 *   is ready to send data.
 */
typedef uint8_t mbedtls_mps_dependencies;
#define MBEDTLS_MPS_BLOCK_READ  ( (mbedtls_mps_dependencies) ( 1u << 0 ) )
#define MBEDTLS_MPS_BLOCK_WRITE ( (mbedtls_mps_dependencies) ( 1u << 1 ) )

/*
 * Return values from parsing/writing functions
 */
#define MBEDTLS_MPS_HANDSHAKE_DONE   0
#define MBEDTLS_MPS_HANDSHAKE_PAUSE  1

/**
 * The security parameter struct mbedtls_ssl_transform is entirely opaque
 * to the MPS. The MPS only uses its instances through configurable payload
 * encryption and decryption functions of type mbedtls_transform_record_t
 * defined below.
 */

/**
 * \brief       Callback for retransmission of outgoing handshake messages.
 *
 * \param ctx    Opaque context passed to the retransmission function.
 *               Must not be altered because multiple retransmissions
 *               must be guaranteed to produce the same results.
 * \param writer
 *
 * \note        If possible, it is advisable to use the same function
 *              that was used to write the message in the first place.
 */
typedef void mbedtls_mps_write_cb_ctx_t;
typedef int (*mbedtls_mps_write_cb_t)( mbedtls_mps_write_cb_ctx_t const *ctx,
                                       mbedtls_writer_ext *writer );

/**
 * Internal representation of outgoing handshake messages (DTLS only)
 */

typedef uint8_t mbedtls_mps_hs_state;
#define MBEDTLS_MPS_HS_NONE   ( (mbedtls_mps_hs_state) 0 )
#define MBEDTLS_MPS_HS_ACTIVE ( (mbedtls_mps_hs_state) 1 )
#define MBEDTLS_MPS_HS_PAUSED ( (mbedtls_mps_hs_state) 2 )

/*
 * \brief Internal structure representing an outgoing handshake message.
 *
 * This is usually a 'fresh' message requested by the handshake
 * logic layer, but can also be the retransmission of an old message,
 * triggered by the retransmission state machine.
 *
 */
struct mbedtls_mps_handshake_out_internal
{
    mbedtls_mps_hs_state state; /*!< Indicates if the handshake message is
                                 *   currently being paused or not.           */

    /*
     * Static information about the message.
     */

    mbedtls_mps_epoch_id epoch; /*!< The epoch through which the handshake
                                 *   message is to be secured. This may be
                                 *   different from the current outgoing
                                 *   epoch in case of retransmissions.        */

    /* OPTIMIZATION:
     * This has significant overlap with Layer 3, which also
     * stores the metadata for handshake messages. Consider
     * optimizing this.
     *
     * Initial thoughts:
     * - For DTLS, handshake metadata isn't used after
     *   the initial call to mps_l3_write_handshake()
     *   anymore because the handshake header is written
     *   immediately. It should therefore not be stored
     *   in the Layer 3 structure in this case.
     * - For TLS, Layer 3 currently uses the stored metadata
     *   to check that the handshake message metadata doesn't
     *   change during paused-and-continued handshake writes.
     *   This is a legitimate use and makes the API harder
     *   to abuse.
     * - For DTLS, Layer 4 needs to store the handshake
     *   metadata: If a large handshake message is written
     *   by the user which fits in the writer's queue but
     *   cannot be dispatched to Layer 3 in one go, the
     *   calls to Layer 3 dispatching the remaining fragments
     *   need the metadata.
     * - Again for DTLS, Layer 4 uses the stored handshake
     *   metadata to check consistency across paused-and-continued
     *   handshake writes, making the API harder to abuse.
     *
     * It seems worth considering to remove the metadata from
     * Layer 3 and storing it solely at Layer 4.
     */

    /*! The handshake sequence number. */
    mbedtls_mps_stored_hs_seq_nr_t seq_nr;

    /*! The type of the handshake message. */
    mbedtls_mps_stored_hs_type type;

    /*! The length of the handshake message to be written, or
     *  #MBEDTLS_MPS_LENGTH_UNKNOWN if the length is determined at write-time.
     *  In this case, pausing is not possible for the handshake message
     *  (because the headers for handshake fragments include the total
     *  length of the handshake message). */
    mbedtls_mps_stored_opt_size_t length;

    /*
     * Progress of writing
     */

    /*! Indicates the offset of the fragment that's currently being written. */
    mbedtls_mps_stored_size_t offset;

    /*
     * Information about current handshake fragment
     * currently opened at Layer 3 to carry the next
     * part of the handshake message.
     */

    /* OPTIMIZATION:
     * Consider removing this pointer; the reader can be queried
     * from Layer 3 anytime, and there's no need to keep its
     * address here. Moreover, the querying might be done through
     * an inline function so that the compiler is able to optimize
     * this into a direct structure field access.
     * In general, care has to be taken to not have the
     * layered structure of MPS come at the cost of information
     * duplication and too many layers of indirections.
     */

    /* OPTIMIZATION:
     * Consider removing the extended writer from Layer 3.
     * Currently, it is only needed to keep track of handshake
     * message bounds during TLS handshake fragmentation, but
     * these bounds checks could as well be moved to Layer 4.
     * This would weaken the API guarantees of Layer 3 in that
     * it'd allow to write fragmented handshake messages longer
     * than indicated in their handshake header, but given
     * that the logic layer only interfaces with Layer 4
     * and the API guarantees of Layer 4 stay the same, this
     * seems acceptable.
     * This change would lead to conceptual simplification,
     * less code, and saving one extended writer of RAM.
     *
     * Removing the extended writer from Layer 3 would mean that
     * we'd safe a pointer to a raw writer here (which, however,
     * might be removed due to the previous optimization
     * opportunity). */
    mbedtls_writer_ext *wr_ext_l3;  /*!< The writer obtained from Layer 3 to
                                     *   write the next handshake fragment.*/

    /* TODO: Document! When is this set? */
    mbedtls_mps_stored_size_t frag_len;

    /*
     * User-facing writers
     */

    /* OPTIMIZATION:
     * The queue and its length are already stored in the writer,
     * and one should be able to avoid dupliating them here. */
    mbedtls_mps_stored_size_t queue_len;
    unsigned char            *queue;
    mbedtls_writer         wr;
    mbedtls_writer_ext wr_ext; /*!< The write-handle to the handshake message
                                *   content that's passed to the user.        */

};

typedef struct mbedtls_mps_handshake_out_internal mbedtls_mps_handshake_out_internal;

/**
 * Retransmission backup
 */

/*! The type of retransmission handle types.
 *
 *  Supported values are:
 *  - #MPS_RETRANSMISSION_HANDLE_NONE
 *    to characterize uninitialized handles.
 *  - #MPS_RETRANSMISSION_HANDLE_HS_RAW
 *    for a handshake message retransmission
 *    based on a raw backup of the message.
 *  - #MPS_RETRANSMISSION_HANDLE_HS_CALLBACK
 *    for a handshake message retransmission
 *    based on a callback.
 *  - #MPS_RETRANSMISSION_HANDLE_CCS
 *    for a CCS message retransmission.
 */
typedef uint8_t mps_retransmission_handle_type;
#define MPS_RETRANSMISSION_HANDLE_NONE         ( (mps_retransmission_handle_type) 0 )
#define MPS_RETRANSMISSION_HANDLE_HS_RAW       ( (mps_retransmission_handle_type) 1 )
#define MPS_RETRANSMISSION_HANDLE_HS_CALLBACK  ( (mps_retransmission_handle_type) 2 )
#define MPS_RETRANSMISSION_HANDLE_CCS          ( (mps_retransmission_handle_type) 3 )

/*! A complete incoming flight has been received. */
#define MPS_INCOMING_FLIGHT_FINISHED 0
/*! We're currently receiving an incoming flight. */
#define MPS_INCOMING_FLIGHT_ONGOING  1

struct mps_retransmission_handle
{
    /*! The type of the retransmission handle. See the documentation
     *  of ::mps_retransmission_handle_type for more information. */
    mps_retransmission_handle_type handle_type;

    /*! The handshake type; unused for CCS retransmissions. */
    mbedtls_mps_stored_hs_type type;
    /*! The handshake sequence number; unused for CCS retransmissions. */
    mbedtls_mps_stored_hs_seq_nr_t seq_nr;

    /*! The epoch used to send the message. */
    mbedtls_mps_epoch_id epoch;

    /*! The total handshake message length. */
    mbedtls_mps_stored_size_t len;

    /*! Union indexed by \c handle_type containing the actual
     *  retransmission handle providing the message content. */
    union
    {
        /*! The raw buffer holding backup of outgoing message. This is valid if
         *  and only if \c type has value #MPS_RETRANSMISSION_HANDLE_HS_RAW. */
        struct
        {
            unsigned char *buf; /*!< The buffer holding the message backup. */

            mbedtls_mps_stored_size_t  len; /*!< Total size of \c buf.      */
        } raw;

        /*! The callback for retransmission. This is valid if and only if
         *  \c type has value #MPS_RETRANSMISSION_HANDLE_CALLBACK. */
        struct
        {
            mbedtls_mps_write_cb_t        cb; /*!< The retransmission
                                               *   callback.                */
            mbedtls_mps_write_cb_ctx_t  *ctx; /*!< The context to be passed
                                               *   to the callback \c cb.   */
        } callback;

        /* No data for CCS messages, i.e. if \c type
         * has value #MPS_RETRANSMISSION_CALLBACK_CCS */
        struct
        {
            int unused[1];
        } ccs;

    } handle;

};
typedef struct mps_retransmission_handle mps_retransmission_handle;


typedef void mbedtls_mps_set_timer_t( void * ctx,
                                      uint32_t int_ms,
                                      uint32_t fin_ms );
typedef int mbedtls_mps_get_timer_t( void * ctx );

/**
 * \brief This structure represents partial backups of messages belonging
 *        to incoming flights that we keep to recognize retransmissions.
 *
 * Currently, we recognize retransmissions by looking at the epoch
 * and the sequence number only, ignoring the handshake type,
 * handshake length, and contents.
 *
 * See the documentation of ::mbedtls_mps::dtls::retransmission_detection
 * for more information on this.
 */
typedef struct
{
    /*! The epoch through which the handshake message was secured. */
    mbedtls_mps_epoch_id epoch;

    /*! The handshake sequence number. */
    mbedtls_mps_stored_hs_seq_nr_t seq_nr;

} mps_recognition_info;

/**
 * MPS Configuration
 */

typedef struct
{
    uint8_t mode;
    mps_l3 *l3;

    /*! The initial value of the retransmission timeout (ms). */
    uint32_t hs_timeout_min;
    /*! The maximum value of the retransmission timeout (ms). */
    uint32_t hs_timeout_max;
    /*! The retransmission timer context. */
    void* p_timer;
    /*! Callback to obtain state of timer. */
    mbedtls_mps_get_timer_t *f_get_timer;
    /*! Callback to set or reset timer. */
    mbedtls_mps_set_timer_t *f_set_timer;

} mps_config;

/**
 * MPS context
 */

struct mbedtls_mps
{
    mps_config conf;

    /* Security configuration */

    /*! The current incoming epoch specified by the user.
     *  Only messages from this epoch will be handed to the
     *  user. However, messages from different epochs might
     *  still be handlded internally, e.g. to detect
     *  retransmission. */
    mbedtls_mps_epoch_id in_epoch;

    /*! The current outgoing epoch specified by the user.
     *  Write requests by the user will be served by using
     *  this epoch. */
    mbedtls_mps_epoch_id out_epoch;

    /* Connection state */

    /*! This indicates if an alert needs to be sent.
     *  If it is set, the type of alert is determined
     *  by \c state and \c blocking_info. Specifically:
     *  - If \c state indicates an orderly connection closure,
     *    a ClosureAlert will be sent.
     *  - If \c state indicates a blocked MPS, a fatal alert
     *    based in the data in \c blocking_info will be sent.
     */
    uint8_t alert_pending; /* TODO: Are there other binary flags
                            *       that could be subsumed in a bitfield? */

    /*! The state of the connection. See the documentation of
     *  ::mbedtls_mps_connection_state_t for the possible values. */
    mbedtls_mps_connection_state_t state;

    /*! This structure contains information on why an MPS
     *  instance was blocked. */
    mbedtls_mps_blocking_info_t blocking_info;

    /* Read state */
    struct
    {
        mbedtls_mps_msg_type_t state;

        /* DTLS only */
        mbedtls_mps_msg_flags      flags; /*!< Indicates if and how the incoming
                                           *   message contributes to an ongoing
                                           *   handshake. */

        /* Note:
         * This is slightly memory-inefficient because the data
         * is already stored in the underlying Layer 3 context.
         * Comments:
         * - It is unavoidable to use an mps_l3_handshake_in instance
         *   at some point, because that's how Layer 3 reports the
         *   handshake contents. For TLS, it might be stack-allocated in
         *   mbedtls_mps_read_handshake(), setup via mps_l3_read_handshake()
         *   and used to fill the target structure mbedtls_mps_handshake_in
         *   in that function.
         * - For DTLS, it is unavoidable to have a separate instance of
         *   mps_l3_handshake_in than the one reported by Layer 3, because
         *   of handshake message reassembly. So, in this case at least,
         *   we must store it in the MPS context.
         * Currently, we decided to treat TLS and DTLS uniformly by
         * having the mps_l3_handshake_in instance in the MPS context
         * in any case.
         * Given that choice, it comes at no additional cost to also
         * have the alert type and reader pointer here.
         */
        union
        {
            mbedtls_mps_alert_t alert;
            mbedtls_reader*     app;
            mps_l3_handshake_in hs;
        } data;

    } in;

    /* Write state */
    struct
    {
        uint8_t flush; /*!< Indicates if a flush has been requested that
                        *   needs to complete before the next read or write
                        *   can commence. */

        mbedtls_mps_msg_type_t state;

    } out;

    /* DTLS retransmission state machine. */
    struct
    {
        /*! This indicates the state of the retransmission state machine.
         *  See the documentation of ::mbedtls_mps_flight_state_t. */
        mbedtls_mps_flight_state_t state;

        /*! This indicates if we're currently retransmitting our last outgoing
         *  flight, or are requesting retransmission from the peer.
         *  See the documentation of ::mbedtls_mps_retransmit_state_t. */
        mbedtls_mps_retransmit_state_t retransmit_state;

        /*! This structure controls the state of outgoing handshake
         *  messages and their fragmentation. It is used both for the
         *  initial sending of messages as well as for retransmissions. */
        mbedtls_mps_handshake_out_internal hs;

        /*! This structure is used when waiting for the next incoming
         *  flight of the peer. It captures the time to wait until we
         *  resend our last outgoing flight (in case we haven't received
         *  anything so far) or request retransmission from the peer
         *  (in case at least one message from the peer has been received).
         *  Further, if we are in the process of resending our last flight
         *  or requesting retransmission from the peer, it holds the sending
         *  state.
         *  Note: Retransmission requests are handled differently
         *        between DTLS 1.2 and DTLS 1.3; see the documentation of
         *        ::mbedtls_mps_retransmit_state_t for more. */
        struct
        {
            /*! The current retransmission timeout (ms). Increases with
             *  every retransmission until a configurable threshold
             *  is reached. */
            uint32_t retransmit_timeout;

            /*! In case of a retransmission, this is the index of the
             *  message to be retransmitted next in the \c backup array
             *  in the \c outgoing structure. */
            uint8_t resend_offset;

        } wait;

        /*! The state of outgoing flights. */
        struct
        {
            /*! This flag indicates if and how the outgoing
             *  message contributes to an ongoing handshake.
             *  See the documentation of ::mbedtls_mps_msg_flags. */
            mbedtls_mps_msg_flags flags;

            /*! The sequence number of the next outgoing message. */
            mbedtls_mps_stored_hs_seq_nr_t seq_nr;

            /*! The number of messages in the current/last outgoing flight. */
            uint8_t flight_len;

            /*! A list of backup handles to be used in case the flight,
             *  or parts of it, need to be retransmitted. See the documentation
             *  of ::mps_retransmission_handle_backup for more. */
            mps_retransmission_handle backup[ MBEDTLS_MPS_MAX_FLIGHT_LENGTH ];

        } outgoing;


        /*! Incoming message buffering and reassembly
         *
         *  Contains all state related to message reassembly
         *  and buffering of future messages.
         *
         *  To the user, it has the following states:
         *  - Inactive:
         *    No incoming handshake message is ready to be read.
         *  - Available:
         *    The next incoming handshake message is available
         *    and can be requested by the user.
         *  - Active:
         *    The next incoming handshake message is available
         *    and has been requested by the user.
         *  - Paused:
         *    The reading of the next incoming handshake message
         *    has been paused.
         *
         *  The interface is the following:
         *
         *  - Initialize
         *    This takes the expected sequence number of the first
         *    handshake message in the next incoming flight.
         *
         *  - Reset
         *
         *  - Feed a new handshake fragment
         *
         *    This is valid only in inactive state.
         *    The reassembly submodule reacts by indicating whether the
         *    new handshake fragment allowed to produce (parts of) the
         *    next expected incoming handshake message to be delivered
         *    to the user. If so, it switches to the `available` state,
         *    and takes ownership of the handshake fragment until the
         *    user has requested and passed back the handshake message.
         *
         *  - Request a new handshake message
         *
         *    This is valid in state `Available` only.
         *    In this case, it provides the user with a handle to
         *    the next incoming handshake message, and moves
         *    to state `Active`.
         *
         *  - Finalize the reading of the current handshake message.
         *
         *    This is valid in state `Active` only and moves to
         *    state `Inactive` or `Available`, depending on whether
         *    the next handshake messages are already available
         *    or not.
         *
         *  - Pause the reading of the current handshake message.
         *
         *  Internally, the complexity is in the feeding call.
         *  When facing a new handshake message fragment, the
         *  reassembly submodule may perform the following actions:
         *
         *   (1) If the fragment is an entire handshake message of
         *       the expected epoch and sequence number, directly
         *       pass it through to the user.
         *   (3) If the fragment is proper and belongs to the next
         *       incoming handshake message, extract its content
         *       and update the reassembly process for that message.
         *       If the fragment leads to a fully reassembled message,
         *       or at least a sufficiently large next contiguous
         *       chunk, it is passed to the user.
         *   (4) If the fragment belongs to a future message and
         *       the implementation supports buffering of future
         *       messages, back it up.
         *   (5) Ignore otherwise (message duplication should be
         *       detected by the replay protection of Layer 2, but
         *       we might receive retransmitted messages of the
         *       current incoming flight that we have already seen).
         *
         * Note: It is not the responsibility of the reassembly
         *       submodule to detect retransmissions. This should
         *       be done before, on the basis of the recognition
         *       info structures of the last incoming flight.
         */
        struct mps_reassembly
        {
            /* QUESTION:
             * Consider storing ::mps_reassembly on the heap
             * and only allocate it when necessary.
             */

            /*! The next expected handshake sequence number. */
            mbedtls_mps_stored_hs_seq_nr_t next_seq_nr;

            /*! The reader and extended reader managing the contents
             *  of the current incoming handshake message. */
            mbedtls_reader         rd;
            mbedtls_reader_ext rd_ext;

            /*! The array of structures representing future and/or
             *  partially received handshake messages. */
            struct mps_msg_reassembly
            {
                /*! The reassembly state of the message.
                 *  See ::mbedtls_mps_msg_reassembly_state for more. */
                mbedtls_mps_msg_reassembly_state status;

                /*! The handshake message type. */
                mbedtls_mps_stored_hs_type type;

                /*! The epoch of the incoming handshake message.
                 *  This must be stored to detect a change of epoch
                 *  between buffering and reading of the message,
                 *  or an epoch change across fragments.
                 *  Examples for that:
                 *  - MPS does not have any knowledge about the structure
                 *    of flights and the evolution of epochs within them.
                 *    In particular, it would buffer a future Finished
                 *    message in a DTLS 1.2 handshake even if it is unencrypted.
                 *    However, remembering the epoch here allows to error out
                 *    by the time the user switches the incoming epoch before
                 *    asking for the Finished message.
                 *  - In DTLS 1.3, there can be key changes at flight
                 *    boundaries, in which case we have multiple incoming
                 *    epochs active when waiting for the next incoming flight,
                 *    because we must be able to detect retransmissions.
                 *    In this case, we must not piece together new messages
                 *    with fragments coming from different epochs.
                 */
                mbedtls_mps_epoch_id epoch;

                /*! The total handshake message length. Remembered to
                 *  check that it is consistent across fragments. */
                mbedtls_mps_stored_size_t length;

                /*!< Union indexed by \c status giving rise to the
                 *   message contents fetched so far. */
                union
                {
                    /*! The extended reader owned by Layer 3 giving rise to the
                     *  contents of the handshake message. This is valid if and
                     *  only if \c status is #MPS_REASSEMBLY_NO_FRAGMENTATION */
                    mbedtls_reader_ext *rd_ext_l3;

                    /*! The reassembly buffer holding the partially received
                     *  handshake message. This is valid if and only if
                     *  \c status is #MPS_REASSEMBLY_WINDOW. */
                    struct mps_msg_reassembly_window
                    {
                        unsigned char *buf;     /*!< The reassembly buffer.   */
                        unsigned char *bitmask; /*!< The bitmask indicating
                                                 *   the state of reassembly. */
                        /*! The size of \c buf.                   */
                        mbedtls_mps_stored_size_t buf_len;
                        /*! The length of the reassembly bitmask. */
                        mbedtls_mps_stored_size_t bitmask_len;
                    } window;

                } data;

            } reassembly[ 1 + MBEDTLS_MPS_FUTURE_MESSAGE_BUFFERS ];

        } incoming;

        /*! Memory of last incoming flight
         *
         *  If the flight state is #MBEDTLS_MPS_FLIGHT_AWAIT,
         *  #MBEDTLS_MPS_FLIGHT_SEND or #MBEDTLS_MPS_FLIGHT_RESEND,
         *  this structure contains the last incoming flight,
         *  remembered in order to be able to recognize
         *  retransmissions.
         *
         *  If the flight state is #MEDTLS_MPS_FLIGHT_RECEIVE,
         *  it contains the ongoing incoming flight.
         *
         *  The transition from #MBEDTLS_MPS_FLIGHT_AWAIT
         *  to #MBEDTLS_MPS_FLIGHT_RECEIVE occurs when the
         *  first message of the next incoming flight is
         *  received, implicitly acknowledging that the peer
         *  has received our last outgoing flight, and thereby
         *  justifying removing our memory of it.
         *
         *  QUESTION:
         *  We should not always trigger retransmission when we
         *  receive a retransmitted message from an old flight:
         *  Since DTLS-1.2 does not use per-message acknowledging,
         *  flights are always retransmitted in their entirety, so if
         *  _any_ message from an old flight would trigger retransmission,
         *  we'd retransmit our last outgoing flight as many times as there
         *  are messages in the last incoming flight.
         *  The previous retransmission state machine implementation
         *  triggered retransmission only when observing the _last_ message
         *  of the previous flight. While this does prevent multiple
         *  retransmissions being triggered by a single retransmission
         *  of our peer, it may happen that the last message gets dropped
         *  and we miss a retransmission, thereby delaying the handshake.
         *
         *     Are there better solutions?
         *
         *  One approach is to have any message from the last incoming
         *  flight trigger a retransmission, but to remember that the
         *  other messages should not immediately trigger another
         *  retransmission when seen. This can be realized by maintaining
         *  two states for each message from the last incoming flight,
         *  'active' and 'on hold', with the following semantics:
         *  - If a retransmission is observed for an 'active' message,
         *    it (1) triggers retransmission of the last outgoing flight,
         *    (2) the message stays 'active', and (3) all other messages
         *    are moved to 'on hold' state.
         *  - If a retransmission is observed for an 'on hold' message,
         *    its state is changed to 'active', but no retransmission happens.
         *  This way, there will never be more than one retransmission
         *  triggered per peer-retransmitted incoming flight.
         */
        struct
        {
            /*! The number of handshake messages in the current or last
             *  incoming flight that we use for flight retransmission
             *  detection; at the same time, the number of entries
             *  in \c msgs which are valid.
             *
             *  Note: We don't remember CCS messages of incoming flights.
             */
            uint8_t flight_len;

            /*! This indicates which messages should currently trigger
             *  a retransmission. See the documentation of
             *  ::mbedtls_mps_recognition_state for more.
             *
             *  NOTE: Currently, ::mbedtls_mps_retransmission_detection_state
             *        has only two values, so a bitfield would do here, but that
             *        doesn't save many bytes. */
            mbedtls_mps_retransmission_detection_state
              msg_state[ MBEDTLS_MPS_MAX_FLIGHT_LENGTH ];

            /*! Aspects of the current or last incoming flight that
             *  we remember for the purpose of recognizing retransmissions.
             *  Currently, we base recognition of retransmitted messages
             *  on the handshake sequence number and epoch only. */
            mps_recognition_info msgs[ MBEDTLS_MPS_MAX_FLIGHT_LENGTH ];

        } retransmission_detection;

        /*
         * DTLS-1.3-NOTE:
         * For DTLS 1.3 we must explicitly acknowledge handshake records through
         * ACK messages and therefore need to add another structure remembering
         * the record sequence number of the handshake records that we received.
         * Note, though, that this lives at a different level than the structure
         * mps_recognition_info used to detect handshake retransmissions.
         */

    } dtls;

};
typedef struct mbedtls_mps mbedtls_mps;

typedef struct mps_reassembly mps_reassembly;
typedef struct mps_msg_reassembly mps_msg_reassembly;
typedef struct mps_msg_reassembly_window mps_msg_reassembly_window;

/**
 * \brief                Set the underlying transport callbacks for the MPS.
 *
 * \param mps            The MPS context to use.
 * \param f_send         Send data to underlying transport
 * \param f_recv         Receive data from underlying transport
 * \param f_recv_timeout Receive data from underlying transport, with timeout.
 *
 * \return               \c 0 on success.
 * \return               A negative error code on failure.
 */
int mbedtls_mps_set_bio( mbedtls_mps *mps, void *p_bio,
                         mbedtls_mps_send_t *f_send,
                         mbedtls_mps_recv_t *f_recv,
                         mbedtls_mps_recv_timeout_t *f_recv_timeout );

/**
 * MPS maintenance
 */

/**
 * \brief                Initialize an MPS context.
 *
 * \param mps            The MPS context to initialize.
 *
 * \return               \c 0 on success.
 * \return               A negative error code on failure.
 */
int mbedtls_mps_init( mbedtls_mps *mps,
                      mps_l3 *l3, uint8_t mode );

/**
 * \brief                Free an MPS context.
 *
 * \param mps            The MPS context to free.
 *
 * \return               \c 0 on success.
 * \return               A negative error code on failure.
 */
int mbedtls_mps_free( mbedtls_mps *mps );

/**
 * Read interface
 */

/* Structure representing an incoming handshake message. */
typedef struct
{
    uint8_t   type;             /*!< Type of handshake message           */
    size_t  length;             /*!< Length of entire handshake message  */
    mbedtls_reader_ext *handle; /*!< Reader to retrieve message contents */

    uint8_t add[8];             /*!< Opaque, additional data to be used for
                                 *   checksum calculations. */
    uint8_t addlen;             /*!< The length of the additional data. */
} mbedtls_mps_handshake_in;

/**
 * \brief       Attempt to read an incoming message.
 *
 * \param mps   The MPS context to use.
 *
 * \return      A negative error code on failure.
 * \return      #MBEDTLS_MPS_APPLICATION, or
 *              #MBEDTLS_MPS_HANDSHAKE, or
 *              #MBEDTLS_MPS_ALERT, or
 *              #MBEDTLS_MPS_CCS
 *              otherwise, indicating which content type was fetched.
 *
 * \note        On success, you can query the type-specific message contents
 *              using one of mbedtls_mps_read_handshake(), mbedtls_mps_read_alert(),
 *              or mbedtls_mps_read_application().
 */
int mbedtls_mps_read( mbedtls_mps *mps );

/**
 * \brief       Check if a message has been read.
 *
 * \param mps   The MPS context to use.
 *
 * \return      #MBEDTLS_ERR_MPS_BLOCKED if MPS is blocked.
 * \return      #MBEDTLS_MPS_PORT_NONE if no message is available.
 * \return      #MBEDTLS_MPS_PORT_APPLICATION, or
 *              #MBEDTLS_MPS_PORT_HANDSHAKE, or
 *              #MBEDTLS_MPS_PORT_ALERT, or
 *              #MBEDTLS_MPS_PORT_CCS,
 *              otherwise, indicating the message's record content type.
 *
 * \note        This function doesn't do any processing and
 *              and only reports if a message is available
 *              through a prior call to mbedtls_mps_read().
 */
int mbedtls_mps_read_check( mbedtls_mps const *mps );

/**
 * \brief       Get a handle to the contents of a pending handshake message.
 *
 * \param mps   The MPS context to use.
 * \param msg   The address to hold the handshake handle.
 *
 * \return      \c 0 on success.
 * \return      A negative error code on failure.
 *
 * \note        This function should only be called after a successful
 *              call to mbedtls_mps_read() or mbedtls_mps_check() returning
 *              #MBEDTLS_MPS_PORT_HANDSHAKE. Otherwise, the function
 *              will silently fail.
 */
int mbedtls_mps_read_handshake( mbedtls_mps *mps,
                                mbedtls_mps_handshake_in *msg );

/**
 * \brief       Get the contents of a pending application data message.
 *
 * \param mps   The MPS context to use.
 * \param rd    The address at which to store the read handle
 *              to be used to access the application data.
 *
 * \return      \c 0 on success.
 * \return      A negative error code on failure.
 *
 * \note        This function should only be called after a successful
 *              call to mbedtls_mps_read() or mbedtls_mps_check() returning
 *              #MBEDTLS_MPS_PORT_APPLICATION. Otherwise, the function
 *              will silently fail.
 */
int mbedtls_mps_read_application( mbedtls_mps *mps,
                                  mbedtls_reader **rd );

/**
 * \brief       Get the type of a pending alert message.
 *
 * \param mps   The MPS context to use.
 * \param type  The address to hold the type of the received alert.
 *
 * \return      \c 0 on success.
 * \return      A negative error code on failure.
 *
 * \note        This function should only be called after a successful
 *              call to mbedtls_mps_read() or mbedtls_mps_check() returning
 *              #MBEDTLS_MPS_PORT_ALERT. Otherwise, the function
 *              will silently fail.
 */
int mbedtls_mps_read_alert( mbedtls_mps const *mps,
                            mbedtls_mps_alert_t *type );

/**
 * \brief          Set the options for the current incoming message.
 *
 * \param mps      The MPS context to use.
 * \param flags    The bitmask indicating if and how the current message
 *                 contributes to the current flight and handshake.
 *                 See the documentation of ::mbedtls_mps_msg_flags for more
 *                 information.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 *
 */
int mbedtls_mps_read_set_flags( mbedtls_mps *mps, mbedtls_mps_msg_flags flags );

/**
 * \brief          Pause the reading of an incoming handshake message.
 *
 * \param mps      The MPS context to use.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 *
 * \note           If this function succeeds, the MPS holds back the reader
 *                 used to fetch the message contents and returns it to the
 *                 MPS-client on the next successful reading of a handshake
 *                 message via mbedtls_mps_read().
 */
int mbedtls_mps_read_pause( mbedtls_mps *mps );

/**
 * \brief          Conclude the reading of an incoming message (of any type).
 *
 * \param mps      The MPS context to use.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 *
 */
int mbedtls_mps_read_consume( mbedtls_mps *mps );

/**
 * \brief          Check which external interfaces (like the underlying
 *                 transport) need to become available in order for the MPS
 *                 to be able to make progress towards fetching a new message.
 *
 * \param mps      The MPS context to use.
 * \param flags    The pointer ready to receive the bitflag indicating
 *                 the external dependencies.
 *
 * \return         \c 0 on success. In that case,
 *                 *flags holds a bitwise OR of some of the following flags:
 *                 - #MBEDTLS_MPS_BLOCK_READ
 *                   The underlying transport must signal incoming data.
 *                 - #MBEDTLS_MPS_BLOCK_WRITE
 *                   The underlying transport must be ready to write data.
 * \return         A negative error code on failure.
 *
 * \note           #MBEDTLS_MPS_BLOCK_READ need not be set here, as there
 *                 might be more internally buffered data waiting to be
 *                 processed, e.g. if there is more than one records within
 *                 a single datagram.
 *
 */
int mbedtls_mps_read_dependencies( mbedtls_mps *mps,
                                   mbedtls_mps_dependencies *flags );

/*
 * The following function constitutes an abstraction break
 * unavoidable by the DTLS standard, so it seems:
 * The standard mandates that a HelloVerifyRequest in DTLS
 * MUST be sent with the same record sequence number as the
 * ClientHello it is replying to.
 */
/**
 * \brief       Get the sequence number of the record to which the
 *              currently opened message belongs.
 *
 * \param mps   The MPS context to use.
 * \param seq   Pointer to write the record sequence number to.
 *
 * \warning     This function constitutes an abstraction break
 *              and should ONLY be used if it is unavoidable by
 *              the standard.
 *
 * \note        This function must be called between a pair of
 *              mbedtls_mps_read() and mbedtls_mps_read_consume() calls.
 *
 * \return      \c 0 on success.
 * \return      A negative error code on failure.
 *
 */
int mbedtls_mps_get_sequence_number( mbedtls_mps *mps, uint8_t seq[8] );

/**
 * Write interface
 */

/* Structure representing an outgoing handshake message. */
typedef struct
{
    /*! The type of the handshake message to be written.
     *
     *  This field must be set by the user before
     *  calling mbedtls_mps_write_handshake(). */
    mbedtls_mps_stored_hs_type type;

   /*! The length of the handshake message to be written, or
    *  #MBEDTLS_MPS_LENGTH_UNKNOWN if the length is determined at write-time.
    *  In this case, pausing is not possible for the handshake message
    *  (because the headers for handshake fragments include the total
    *  length of the handshake message).
    *
    *  This field must be set by the user before
    *  calling mbedtls_mps_write_handshake(). */
    mbedtls_mps_stored_opt_size_t length;

    mbedtls_writer_ext *handle; /*!< Write-handle to handshake message content.
                                 *
                                 *   This field is set by the MPS implementation
                                 *   of mbedtls_mps_write_handshake(). Any
                                 *   previous value will be ignored and
                                 *   overwritten.       */

    uint8_t add[8];        /*!< Read only additional data attached to the
                            *   handshake message. Concretely, this is empty for
                            *   TLS and contains the handshake sequence number
                            *   for DTLS.
                            *
                            *   This is exposed to allow it to enter
                            *   checksum computations.
                            *
                            *   This field is set by the MPS implementation
                            *   of mbedtls_mps_write_handshake().             */

    uint8_t addlen;         /*!< The length of the additional data.
                             *
                             *   This field is set by the MPS implementation
                             *   of mbedtls_mps_write_handshake().            */
} mbedtls_mps_handshake_out;

/* Structure representing an outgoing application data message. */
typedef struct
{
    uint8_t* app;   /*!< Application data buffer. Its content
                     *   may be modified by the application. */
    size_t app_len; /*!< Size of application data buffer.    */

    size_t *written; /*!< Set by the user, indicating the amount
                      *   of the application data buffer that has
                      *   been filled with outgoing data.     */
} mbedtls_mps_app_out;

/**
 * \brief          Indicate the contribution of the current outgoing
 *                 message to the flight.
 *
 * \param mps      The MPS context to use.
 * \param flags    The bitmask indicating if and how the current message
 *                 contributes to the current flight and handshake.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 *
 */
int mbedtls_mps_write_set_flags( mbedtls_mps *mps, mbedtls_mps_msg_flags flags );

/**
 * \brief        Attempt to start writing a handshake message.
 *
 * \param mps    The MPS context ot use.
 * \param msg    The pointer to a structure which defines the type
 *               and optionally the length of the handshake message
 *               to be written (provided by the user), and to which
 *               the write handle and additional data provided by MPS
 *               should be written on success.
 *               See the documentation of mbedtls_mps_handshake_out
 *               for more information.
 * \param cb     The callback to use for retransmitting, or \c NULL.
 *               If \c NULL, MPS internally makes a copy of the message.
 * \param cb_ctx The opaque context to be passed to the retransmission
 *               callback \p cb on retransmission; must be \c NULL if
 *               \p cb is \c NULL.
 *
 * \note         The question of whether to register a callback or not
 *               is a speed / space / convenience tradeoff: A raw copy
 *               of the message is convenient and fast, but increases
 *               the RAM usage by the size of the message. A callback,
 *               on the contrary, avoids this RAM overhead, but comes
 *               at the cost of manually sorting out which information
 *               is necessary to write the message and to generate and
 *               remember it prior to message writing in order to have
 *               a callback which is constant on the callback context.
 *
 * \return       \c 0 on success.
 * \return       A negative error code on failure.
 *
 */
int mbedtls_mps_write_handshake( mbedtls_mps *mps,
                                 mbedtls_mps_handshake_out *msg,
                                 mbedtls_mps_write_cb_t cb,
                                 mbedtls_mps_write_cb_ctx_t *cb_ctx );

/**
 * \brief       Attempt to start writing application data.
 *
 * \param mps   The MPS context to use.
 * \param app   The address to hold the outgoing application
 *              data buffer structure on success.
 *
 * \return       \c 0 on success.
 * \return       A negative error code on failure.
 *
 */
int mbedtls_mps_write_application( mbedtls_mps *mps,
                                   mbedtls_writer **app );

/**
 * \brief       Attempt to start writing a non-fatal alert.
 *
 * \param mps        The MPS context to use.
 * \param alert_type The type of the alert to be sent.
 *
 * \return           \c 0 on success.
 * \return           A negative error code on failure.
 *
 */
int mbedtls_mps_write_alert( mbedtls_mps *mps,
                             mbedtls_mps_alert_t alert_type );

/**
 * \brief            Attempt to start writing a ChangeCipherSpec message.
 *
 * \param mps        The MPS context to use.
 *
 * \return           \c 0 on success.
 * \return           A negative error code on failure.
 *
 * \note             Even if there is no content to be specified for
 *                   ChangeCipherSpec messages, the writing must currently
 *                   still be explicitly concluded through a call to
 *                   mbedtls_mps_dispatch() in uniformity with the handling
 *                   of the other content types.
 *
 *                   Originally, this splitting was mandatory because
 *                   mbedtls_mps_dispatch() might attempt to deliver
 *                   the outgoing message to the underlying transport
 *                   immediately. In that case, we must be able to tell
 *                   apart the following situations:
 *                   (a) The call returned WANT_WRITE because there was still
 *                       data to be flushed, but the underlying transport
 *                       wasn't available.
 *                   (b) The call returned WANT_WRITE because the alert/CCS
 *                       message could be prepared but not yet delivered
 *                       to the underlying transport.
 *                   In case (a), the writing of the alert/CCS hasn't
 *                   commenced, hence we need to call this function again
 *                   for a retry. In case (b), in contrast, the record holding
 *                   the alert/CCS has been prepared and only its delivery
 *                   needs to be retried via mbedtls_mps_flush().
 *
 *                   However, the current version of MPS does never attempt
 *                   immediate delivery of messages to the underlying transport,
 *                   and hence one might omit the explicit call to
 *                   mbedtls_mps_dispatch() in this case. For now, however,
 *                   we keep it for uniformity.
 *
 */
int mbedtls_mps_write_ccs( mbedtls_mps *mps );

/**
 * \brief          Pause the writing of an outgoing handshake message.
 *
 * \param mps      The MPS context to use.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 *
 * \note           If this function succeeds, the MPS holds back the writer
 *                 used to write the message contents and returns it to the
 *                 user on the next successful call to mbedtls_mps_write().
 */
int mbedtls_mps_write_pause( mbedtls_mps *mps );

/**
 * \brief          Conclude the writing of the current outgoing message.
 *
 * \param mps      The MPS context to use.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 *
 * \note           This call does not necessarily immediately encrypt and
 *                 deliver the message to the underlying transport. If that
 *                 is desired, additionally mbedtls_mps_flush() must be
 *                 called afterwards.
 *
 * \note           Encryption may be postponed because there's more space
 *                 in the current record. If the current record is full but
 *                 there's more space in the current datagram, the record
 *                 would be decrypted but not yet delivered to the underlying
 *                 transport.
 */
int mbedtls_mps_dispatch( mbedtls_mps *mps );

/**
 * \brief          Enforce that all messages dispatched since the last call
 *                 to this function get encrypted and delivered to the
 *                 underlying transport.
 *
 * \param mps      The MPS context to use.
 *
 * \return          \c 0 on success. In this case, all previously dispatched
 *                  messages have been delivered.
 * \return          #MBEDTLS_ERR_MPS_WANT_WRITE if the underlying transport
 *                  could not yet deliver all messages. In this case, the
 *                  call is remembered and it is guaranteed that no call to
 *                  mbedtls_mps_write() succeeds before all messages have
 *                  been delivered.
 * \return          Another negative error code otherwise.
 *
 */
int mbedtls_mps_flush( mbedtls_mps *mps );

/**
 * \brief          Check which external interfaces need to become
 *                 available in order for the MPS to be able to make
 *                 progress towards starting the writing of a new message.
 *
 * \param mps      The MPS context to use.
 * \param flags    Pointer ready to receive the bitflag indicating
 *                 the external dependencies.
 *
 * \return         \c 0 on success. In this case, \c *flags holds a
 *                 bitwise OR of some of the following flags:
 *                 - #MBEDTLS_MPS_BLOCK_READ
 *                   The underlying transport must signal incoming data.
 *                 - #MBEDTLS_MPS_BLOCK_WRITE
 *                   The underlying transport must be ready to write data.
 * \return         A negative error code otherwise.
 *
 * \note           A typical example for this is #MBEDTLS_MPS_BLOCK_WRITE
 *                 being set after a call to mbedtls_mps_flush().
 *
 */
int mbedtls_mps_write_dependencies( mbedtls_mps *mps,
                                    mbedtls_mps_dependencies *flags );

/*
 * The following function constitutes an abstraction break
 * unavoidable by the DTLS standard, so it seems:
 * The standard mandates that a HelloVerifyRequest in DTLS
 * MUST be sent with the same record sequence number as the
 * ClientHello it is replying to.
 */
/**
 * \brief       Force record sequence number of next record to be written
 *              (DTLS only).
 *
 * \param mps   The MPS context to use.
 * \param seq   Buffer holding record sequence number to use next.
 *
 * \warning     This function constitutes an abstraction break
 *              and should ONLY be used if it is unavoidable by
 *              the standard. It should almost always be fine to
 *              let the MPS choose the record sequence number.
 *
 * \note        This function must be called before starting the
 *              write to which it applies (this is because forcing
 *              the record sequence number most likely mandates
 *              the use of a new record when starting the next write,
 *              while normally the MPS would attempt to merge
 *              messages of the same content type in the same record).
 *
 * \return      \c 0 on success.
 * \return      A negative error code otherwise.
 */
int mbedtls_mps_force_sequence_number( mbedtls_mps *mps, uint8_t seq[8] );


/**
 * Security parameter interface
 */

/**
 * \brief        Register the next epoch of security parameters.
 *
 * \param mps    The MPS context to use.
 * \param params The address of the new security parameter set to register.
 * \param id     The address at which to store the identifier through
 *               which the security parameter set can subsequently be
 *               identified.
 *
 * \note         The registration of the new security parameter set does
 *               not yet put it to use for reading or writing. To that end,
 *               use the functions mbedtls_mps_set_incoming_keys() and
 *               mbedtls_mps_set_outgoing_keys(), passing the identifier
 *               this function has written to \p id.
.
 * \note         The security parameter set \p params must be heap-allocated,
 *               and calling this function transfers ownership entirely to the
 *               MPS. In particular, no read, write or deallocation operation
 *               must be performed on \p params by the user after this function
 *               has been called. This leads to the following usage flow:
 *               - Allocate an ::mbedtls_mps_transform_t instance
 *                 from the heap to hold the new security parameters.
 *               - Initialize and configure the security parameters.
 *               - Register the security parameters through
 *                 a call to this function.
 *               - Enable the security parameters for reading
 *                 and/or writing via mbedtls_mps_set_incoming_kets()
 *                 or mbedtls_mps_set_outgoing_keys().
 *
 * \return       \c 0 on success.
 * \return       A negative error code otherwise.
 */
int mbedtls_mps_add_key_material( mbedtls_mps *mps,
                                  mbedtls_mps_transform_t *params,
                                  mbedtls_mps_epoch_id *id );

/**
 * \brief        Set the security parameters for subsequent incoming messages.
 *
 * \param mps    The MPS context to use.
 * \param id     The identifier of a set of security parameters
 *               previously registered via mbedtls_mps_add_key_material().
 *
 * \return       \c 0 on success.
 * \return       A negative error code otherwise.
 */
int mbedtls_mps_set_incoming_keys( mbedtls_mps *mps,
                                   mbedtls_mps_epoch_id id );

/**
 * \brief        Set the security parameters for subsequent outgoing messages.
 *
 * \param mps    The MPS context to use.
 * \param params The identifier for a set of security parameters
 *               previously registered via mbedtls_mps_add_key_material().
 *
 * \return       \c 0 on success.
 * \return       A negative error code otherwise.
 */
int mbedtls_mps_set_outgoing_keys( mbedtls_mps *mps,
                                   mbedtls_mps_epoch_id id );

/**
 * Error handling and shutdown interface
 */

/**
 * \brief       Send a fatal alert of the given type
 *
 * \param mps        MPS context
 * \param alert_type Type of alert to be sent.
 *
 * \return      \c 0 on success.
 * \return      A negative error code otherwise.
 *
 * \note        This call blocks the MPS except for mbedtls_mps_flush()
 *              which might still be called in case this function returns
 *              #MBEDTLS_ERR_WANT_WRITE, indicating that the alert couldn't
 *              be delivered.
 *              After delivery of the fatal alert, the user must free ths MPS.
 */
int mbedtls_mps_send_fatal( mbedtls_mps *mps, mbedtls_mps_alert_t alert_type );

/**
 * \brief       Initiate or proceed with orderly shutdown.
 *
 * \param mps   MPS context
 *
 * \return      0 on success, nonzero error code otherwise.
 *
 * \note        This call closes the write-side of the connection and
 *              notifies the peer through an appropriate alert. Afterwards,
 *              the MPS' write functions are blocked, except for
 *              mbedtls_mps_flush() which might still be called in
 *              case this function returns #MBEDTLS_ERR_WANT_WRITE,
 *              indicating that the notification couldn't be delivered.
 */
int mbedtls_mps_close( mbedtls_mps *mps );

mbedtls_mps_connection_state_t mbedtls_mps_connection_state( mbedtls_mps const *mps );

int mbedtls_mps_error_state( mbedtls_mps const *mps,
                             mbedtls_mps_blocking_info_t *info );

#endif /* MBEDTLS_MPS_H */
