// SPDX-License-Identifier: Apache-2.0

/*
 * Biz protocol definitions.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include "biz_protocols.hpp"

COMMPROTO_META_VAR_OUT_OF_STRUCT(packet_head);

COMMPROTO_META_VAR_OUT_OF_STRUCT(packet_body_prefix);

COMMPROTO_META_VAR_OUT_OF_STRUCT(req_0000_connect);
COMMPROTO_META_VAR_OUT_OF_STRUCT(reply_0001_connect);

COMMPROTO_META_VAR_OUT_OF_STRUCT(req_0002_query_server_status);
COMMPROTO_META_VAR_OUT_OF_STRUCT(reply_0003_query_server_status);

COMMPROTO_META_VAR_OUT_OF_STRUCT(reply_0005_live_stream);

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-06, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

