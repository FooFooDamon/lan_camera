/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Biz protocol definitions.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#ifndef __BIZ_PROTOCOLS_HPP__
#define __BIZ_PROTOCOLS_HPP__

#include "communication_protocol.h"

#define TIMESPEC_TO_SESSION_ID(t)           (((uint64_t)(t).tv_sec) * 1000000000 + (t).tv_nsec)

#pragma pack(1)

// Comment out this macro if the type of session_id is string.
#define INT64_SESSION_ID

#if !defined(INT64_SESSION_ID) && !defined(SESSION_ID_LENGTH)
#define SESSION_ID_LENGTH                   16
#define SESSION_ID_ARRAY_SIZE               (SESSION_ID_LENGTH + (sizeof(uint32_t) - (SESSION_ID_LENGTH % sizeof(uint32_t))))
#endif

#ifdef INT64_SESSION_ID
#define SESSION_ID_META_VAR                 COMMPROTO_INT64
#else
#define SESSION_ID_META_VAR                 COMMPROTO_INT8_FIXED_ARRAY, COMMPROTO_ARRAY_LEN_IS(SESSION_ID_ARRAY_SIZE)
#endif

// Comment out this macro if session_id is in packet body.
#define SESSION_ID_IN_HEADER

typedef struct packet_head
{
    /*
     * NOTE:
     *      DO NOT modify these fields directly!
     *      Use set_once_per_round() and set_for_current_packet() instead!
     */
    uint16_t length; // total length of remaining fields of this struct and the following body[n]
    uint16_t command_code;
    uint16_t packet_seq; // value range: [1, total_packets]
    uint16_t total_packets;
    uint32_t sizeof_all_bodies; // includes length of packet_body_prefix
    // packet[1]:                   0 + length(body[1]) (including length of packet_body_prefix)
    // packet[2]: next_body_offset[1] + length(body[2])        (no length of packet_body_prefix)
    // packet[3]: next_body_offset[2] + length(body[3])        (no length of packet_body_prefix)
    // ...
    uint32_t next_body_offset;
#ifdef SESSION_ID_IN_HEADER
#ifdef INT64_SESSION_ID
    uint64_t session_id;
#else
    char session_id[SESSION_ID_ARRAY_SIZE];
#endif
#endif

    COMMPROTO_META_VAR_IN_STRUCT = {
        COMMPROTO_INT16, // length
        COMMPROTO_INT16, // command_code
        COMMPROTO_INT16, // packet_seq
        COMMPROTO_INT16, // total_packets
        COMMPROTO_INT32, // sizeof_all_bodies
        COMMPROTO_INT32, // next_body_offset
#ifdef SESSION_ID_IN_HEADER
        SESSION_ID_META_VAR // session_id
#endif
    };

    COMMPROTO_DEFINE_META_FUNCTIONS_IN_STRUCT();

    inline void set_once_per_round(uint16_t command_code, uint16_t total_packets, uint32_t sizeof_all_bodies,
#ifdef SESSION_ID_IN_HEADER
#ifdef INT64_SESSION_ID
        uint64_t session_id
#else
        const char session_id[]
#endif
#endif
    )
    {
        this->command_code = command_code;
        this->packet_seq = 0;
        this->total_packets = total_packets;
        this->sizeof_all_bodies = sizeof_all_bodies;
        this->next_body_offset = 0;
#ifdef SESSION_ID_IN_HEADER
#ifdef INT64_SESSION_ID
        this->session_id = session_id;
#else
        memcpy(this->session_id, session_id, SESSION_ID_LENGTH);
        session_id[SESSION_ID_LENGTH] = '\0';
#endif
#endif
    }

    inline void set_for_current_packet(uint16_t body_size)
    {
        ++this->packet_seq;
        this->length = sizeof(packet_head) - sizeof(packet_head::length) + body_size;
        this->next_body_offset += body_size;
    }

    inline bool is_valid(void)
    {
        return ((packet_seq > 0) && (total_packets >= packet_seq)
            && (next_body_offset > 0) && (sizeof_all_bodies >= next_body_offset)
            && (length > sizeof(packet_head) - sizeof(packet_head::length)));
    }

    /*
     * This function CAN be used ONLY AFTER:
     *      set_once_per_round() and set_for_current_packet() are called,
     * OR:
     *      COMMPROTO_CPP_PARSE() or commproto_parse() is called, and is_valid() returns true.
     */
    inline uint16_t body_size(void)
    {
        return this->length + sizeof(packet_head::length) - sizeof(packet_head);
    }

    /*
     * This function CAN be used ONLY AFTER:
     *      set_once_per_round() and set_for_current_packet() are called,
     * OR:
     *      COMMPROTO_CPP_PARSE() or commproto_parse() is called, and is_valid() returns true.
     */
    inline uint32_t body_offset(void)
    {
        return this->next_body_offset - this->body_size();
    }
} packet_head_t;

#define DEFINE_PROTO_VERSION_FUNC(_ver_)    static inline constexpr uint32_t version(void) { return _ver_; }

// NOTE: This enum type can be defined in somewhere else,
//      but its name after the enum keyword should not change
//      since the doc generation of packet_body_prefix depends on it!
enum proto_err_e //! Protocol Error Codes
{
    PROTO_ERR_OK = 0, //!> Success
    PROTO_ERR_UNSUPPORTED = 1, //!> Operation unsupported yet
    PROTO_ERR_UNIMPLEMENTED = 2, //!> Operation unimplemented yet
    PROTO_ERR_INNER = 3, //!> Inner error
    PROTO_ERR_PKT_BIGGER_THAN_SPECIFIED = 4, //!> Packet bigger than specified
    PROTO_ERR_PKT_TOO_BIG = 5, //!> Packet too big
    PROTO_ERR_PKT_TRUNCATED = 6, //!> Packet truncated
    PROTO_ERR_PKT_FRAGMENTS_NOT_ENOUGH = 7, //!> Fragments not enough to form a full packet
    PROTO_ERR_PKT_HEADER_PRECHK_FAILED = 8, //!> Packet header precheck failed
    PROTO_ERR_PKT_TOTAL_FRAGS_INCONSISTENT = 9, //!> Total fragments value inconsistent
    PROTO_ERR_PKT_SIZEOF_ALL_FRAGS_INCONSISTENT = 10, //!> Size value of all fragments inconsistent
    PROTO_ERR_PROTO_VERSION_TOO_LOW = 11, //!> Protocol version too low
    PROTO_ERR_PROTO_VERSION_TOO_MISMATCHED = 12, //!> Protocol version mismatched
    PROTO_ERR_UNKNOWN_PEER = 13, //!> Unknown peer
    PROTO_ERR_PEER_ALREADY_EXISTED = 14, //!> Peer already existed
    PROTO_ERR_REPEATED_REQUEST = 15, //!> Repeated request
    PROTO_ERR_NULL_SESSION_ID = 16, //!> Null session ID
    PROTO_ERR_SESSION_ID_TOO_LONG = 17, //!> Session ID too long
    PROTO_ERR_NULL_NAME = 18, //!> Null name
    PROTO_ERR_NAME_TOO_LONG = 19, //!> Name too long
    PROTO_ERR_NULL_STRING = 20, //!> Null string
    PROTO_ERR_STRING_TOO_LONG = 21, //!> String too long
    PROTO_ERR_VALUE_OUT_OF_RANGE = 22, //!> Value out of range
};

typedef struct packet_body_prefix //! \makecell[l]{Prefix that body of each FULL packet\\ (not sub-packet or fragment) should contain one.}
{
#ifndef SESSION_ID_IN_HEADER
#ifdef INT64_SESSION_ID
    uint64_t session_id; //!> | session id |
#else
    char session_id[SESSION_ID_ARRAY_SIZE]; //!> | session id |
#endif
#endif
    uint32_t version; //!> | version |
    uint16_t return_code; //!> | return code | See \ref{proto_err_e}

    COMMPROTO_META_VAR_IN_STRUCT = {
#ifndef SESSION_ID_IN_HEADER
        SESSION_ID_META_VAR, // session_id
#endif
        COMMPROTO_INT32, // version
        COMMPROTO_INT16, // return_code
    };

    COMMPROTO_DEFINE_META_FUNCTIONS_IN_STRUCT();

    inline void set(
#ifndef SESSION_ID_IN_HEADER
#ifdef INT64_SESSION_ID
        uint64_t session_id,
#else
        const char session_id[],
#endif
#endif
        uint32_t version, uint16_t return_code)
    {
#ifndef SESSION_ID_IN_HEADER
#ifdef INT64_SESSION_ID
        this->session_id = session_id;
#else
        memcpy(this->session_id, session_id, SESSION_ID_LENGTH);
        session_id[SESSION_ID_LENGTH] = '\0';
#endif
#endif
        this->version = version;
        this->return_code = return_code;
    }
} packet_body_prefix_t;

#ifdef SESSION_ID_IN_HEADER
#define PKT_BODY_PREFIX_META_VARS           \
                                            COMMPROTO_INT32, \
                                            COMMPROTO_INT16
#else
#define PKT_BODY_PREFIX_META_VARS           SESSION_ID_META_VAR, \
                                            COMMPROTO_INT32, \
                                            COMMPROTO_INT16
#endif

/*
 * Rules of defining an enum list:
 *
 * 01. Start a definition with:
 *      enum <name>_e //! <mandatory description in one single line>
 *
 * 02. Each item should be defined in a format of:
 *      <name> = <value>, //!> <mandatory description in one single line>
 */

/*
 * Rules of defining a packet body struct:
 *
 * 01. Start a definition with:
 *      typedef struct {req,reply}_<command code hex>_<name> //! <mandatory description in one single line>
 *
 * 02. Name of a sub-struct does not need   {req,reply}_<command code hex>_   prefix.
 *
 * 03. Only single variables and arrays of BASIC TYPEs are allowed within a sub-structure.
 *
 * 04. Each field of a struct should be defined in a format of:
 *      <type> <name>; //!> | <meaning> | [optional remark in one single line]
 *
 * 05. At least COMMPROTO_META_VAR_IN_STRUCT and COMMPROTO_DEFINE_META_FUNCTIONS_IN_STRUCT() are needed.
 *
 * 06. All structs MUST NOT contain virtual functions!
 *
 * 07. LaTeX syntax is allowed when needed, for example: \ref{}, \makecell[l]{}, etc.
 */

#define REQ_CONNECT                         0x0000

typedef struct req_0000_connect //! Connect Request
{
    struct packet_body_prefix prefix; //!> | body prefix | See \ref{packet_body_prefix}
    char name[32]; //!> | client name |

    DEFINE_PROTO_VERSION_FUNC(0x01000000);

    COMMPROTO_META_VAR_IN_STRUCT = {
        PKT_BODY_PREFIX_META_VARS, // prefix
        COMMPROTO_INT8_FIXED_ARRAY, COMMPROTO_ARRAY_LEN_IS(32), // name
    };

    COMMPROTO_DEFINE_META_FUNCTIONS_IN_STRUCT();
} req_0000_connect_t;

#define REPLY_CONNECT                       0x0001

typedef struct multicast_config //! Multicast Configuration
{
    char group_ip[16]; //!> | IP address of multicast group |
    char receiver_ip[16]; //!> | IP address of receiver | IP of current requesting client echoed by server
    uint16_t port; //!> | multicast port |
    uint16_t max_payload_size; //!> | maximum payload size used in multicast |
} multicast_config_t;

typedef struct reply_0001_connect //! Connect Reply
{
    struct packet_body_prefix prefix; //!> | body prefix | See \ref{packet_body_prefix}
    uint16_t image_width; //!> | image width |
    uint16_t image_height; //!> | image height |
    uint16_t camera_buf_count; //!> | camera buffer count |
    struct multicast_config multicast; //!> | multicast config | See \ref{multicast_config}
    uint64_t startup_time_secs; //!> | startup time in seconds |

    DEFINE_PROTO_VERSION_FUNC(0x01000000);

    COMMPROTO_META_VAR_IN_STRUCT = {
        PKT_BODY_PREFIX_META_VARS, // prefix
        COMMPROTO_INT16, // image_width
        COMMPROTO_INT16, // image_height
        COMMPROTO_INT16, // camera_buf_count
        // Fields of multicast_config_t:
            COMMPROTO_INT8_FIXED_ARRAY, COMMPROTO_ARRAY_LEN_IS(16), // group_ip
            COMMPROTO_INT8_FIXED_ARRAY, COMMPROTO_ARRAY_LEN_IS(16), // receiver_ip
            COMMPROTO_INT16, // port
            COMMPROTO_INT16, // max_payload_size
        COMMPROTO_INT64, // startup_time_secs
    };

    COMMPROTO_DEFINE_META_FUNCTIONS_IN_STRUCT();
} reply_0001_connect_t;

#define REQ_QUERY_SERVER_STATUS             0x0002

typedef struct req_0002_query_server_status //! Query Server Status Request
{
    struct packet_body_prefix prefix; //!> | body prefix | See \ref{packet_body_prefix}
    uint8_t needs_live_stream; //!> | whether to need live stream |

    DEFINE_PROTO_VERSION_FUNC(0x01000000);

    COMMPROTO_META_VAR_IN_STRUCT = {
        PKT_BODY_PREFIX_META_VARS, // prefix
        COMMPROTO_INT8, // needs_live_stream
    };

    COMMPROTO_DEFINE_META_FUNCTIONS_IN_STRUCT();
} req_0002_query_server_status_t;

#define REPLY_QUERY_SERVER_STATUS           0x0003

typedef struct reply_0003_query_server_status //! Query Server Status Reply
{
    struct packet_body_prefix prefix; //!> | body prefix | See \ref{packet_body_prefix}
    uint8_t inference_positive; //!> | whether the inference result is positive or negative |
    uint32_t dropped_saving_count; //!> | dropped saving count |
    uint32_t dropped_sending_count; //!> | dropped sending count |
    uint32_t dropped_inference_count; //!> | dropped inference count |
    uint64_t total_saving_count; //!> | total saving count |
    uint64_t total_sending_count; //!> | total sending count |
    uint64_t total_inference_count; //!> | total inference count |

    DEFINE_PROTO_VERSION_FUNC(0x01000000);

    COMMPROTO_META_VAR_IN_STRUCT = {
        PKT_BODY_PREFIX_META_VARS, // prefix
        COMMPROTO_INT8, // inference_positive
        COMMPROTO_INT32, // dropped_saving_count
        COMMPROTO_INT32, // dropped_sending_count
        COMMPROTO_INT32, // dropped_inference_count
        COMMPROTO_INT64, // total_saving_count
        COMMPROTO_INT64, // total_sending_count
        COMMPROTO_INT64, // total_inference_count
    };

    COMMPROTO_DEFINE_META_FUNCTIONS_IN_STRUCT();
} reply_0003_query_server_status_t;

#define REQ_LIVE_STREAM                     0x0004

typedef packet_body_prefix_t                req_0004_live_stream_t; //! Live Stream Request (Unused)

#define REPLY_LIVE_STREAM                   0x0005

enum compression_algo_e //! Compression Algorithm
{
    COMPRESSION_NONE = 0, //!> None
    COMPRESSION_JPEG = 1, //!> JPEG
    COMPRESSION_PNG = 2, //!> PNG
    COMPRESSION_WEBP = 3, //!> WEBP
    COMPRESSION_LZ4 = 4, //!> LZ4
    COMPRESSION_ZLIB = 5, //!> zlib
};

enum image_format_e //! Image Format
{
    IMG_FORMAT_NOT_CARED = 0, //!> Not cared
    IMG_FORMAT_JPEG = 1, //!> JPEG
    IMG_FORMAT_PNG = 2, //!> PNG
    IMG_FORMAT_WEBP = 3, //!> WEBP
    IMG_FORMAT_NV12 = 4, //!> NV12
    IMG_FORMAT_NV21 = 5, //!> NV21
};

typedef struct reply_0005_live_stream //! Live Stream Reply
{
    struct packet_body_prefix prefix; //!> | body prefix | See \ref{packet_body_prefix}
    uint8_t inference_positive; //!> | whether the inference result is positive or negative |
    uint8_t compression_algo; //!> | compression algorithm | See \ref{compression_algo_e}
    uint8_t compression_level; //!> | compression level |
    uint8_t image_format; //!> | image format | See \ref{image_format_e}
    uint16_t image_width; //!> | image width |
    uint16_t image_height; //!> | image height |
    uint16_t image_channels; //!> | image channel count | uint8_t is enough.
    uint64_t time_secs; //!> | timestamp seconds |
    uint64_t time_nsecs; //!> | timestamp nanoseconds |
    float32_t fps; //!> | frames-per-second |
    arraylen32_t data_size; //!> | data size |
    uint8_t *stream_data; //!> | stream data |

    DEFINE_PROTO_VERSION_FUNC(0x01000000);

    COMMPROTO_META_VAR_IN_STRUCT = {
        PKT_BODY_PREFIX_META_VARS, // prefix
        COMMPROTO_INT8, // inference_positive
        COMMPROTO_INT8, // compression_algo
        COMMPROTO_INT8, // compression_level
        COMMPROTO_INT8, // image_format
        COMMPROTO_INT16, // image_width
        COMMPROTO_INT16, // image_height
        COMMPROTO_INT16, // image_channels
        COMMPROTO_INT64, // time_secs
        COMMPROTO_INT64, // time_nsecs
        COMMPROTO_FLOAT32, // fps
        COMMPROTO_ARRAY_LEN32, // data_size
        COMMPROTO_INT8_DYNAMIC_ARRAY, // stream_data
    };

    COMMPROTO_DEFINE_META_FUNCTIONS_IN_STRUCT();

    static inline constexpr uint8_t sizeof_info_fields(void)
    {
        return sizeof(reply_0005_live_stream) - sizeof(reply_0005_live_stream::stream_data);
    }
} reply_0005_live_stream_t;

#pragma pack()

#endif /* #ifndef __BIZ_PROTOCOLS_HPP__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-06, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 *
 * >>> 2026-04-26, Man Hung-Coeng <udc577@126.com>:
 *  01. Replace global PROTO_VERSION with version() in each protocol body class.
 */

