// SPDX-License-Identifier: Apache-2.0

/*
 * Biz executor of connecting to server and sending heartbeat packets.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include <signal.h> // For raise() and SIG*.
#include <unistd.h> // For usleep() and close().
#include <sys/socket.h>
#include <netinet/in.h> // For struct sockaddr_in and htons().
#include <arpa/inet.h> // For inet_addr().
#include <sys/select.h>

#include "signal_handling.h"

#include "fmt_log.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"
#include "biz_protocols.hpp"
#include "QLANCamera.hpp"

static thread_local struct timespec s_cur_time;
static thread_local packet_head_t s_header;

static inline struct sockaddr_in make_server_address(const char *ip, uint16_t port)
{
    struct sockaddr_in result = {};

    result.sin_family = AF_INET;
    result.sin_port = htons(port);
    result.sin_addr.s_addr = inet_addr(ip);

    return result;
}

static int send_connect_request(int fd, const biz_context_t &ctx)
{
    static thread_local req_0000_connect_t s_body = {};
    static thread_local std::vector<unsigned char> s_buf(sizeof(s_header) + sizeof(s_body));
    static thread_local struct sockaddr_in s_server_addr = \
        make_server_address(ctx.conf->network.endpoint.peer_ip.c_str(), ctx.conf->network.endpoint.peer_port);

    clock_gettime(CLOCK_MONOTONIC, &s_cur_time);
    s_header.set_once_per_round(REQ_CONNECT, 1, sizeof(s_body), TIMESPEC_TO_SESSION_ID(s_cur_time));
    s_header.set_for_current_packet(sizeof(s_body));
    COMMPROTO_CPP_SERIALIZE(&s_header, s_buf.data(), sizeof(s_header));

    s_body.prefix.set(s_body.version(), 0);
    strncpy(s_body.name, ctx.conf->role.name.c_str(), sizeof(s_body.name) - 1);
    COMMPROTO_CPP_SERIALIZE(&s_body, s_buf.data() + sizeof(s_header), sizeof(s_body));

    return sendto(fd, s_buf.data(), s_buf.size(), 0, (struct sockaddr *)&s_server_addr, sizeof(s_server_addr));
}

static int send_server_status_request(int fd, const biz_context_t &ctx)
{
    static thread_local req_0002_query_server_status_t s_body;
    static thread_local std::vector<unsigned char> s_buf(sizeof(s_header) + sizeof(s_body));
    static thread_local struct sockaddr_in s_server_addr = \
        make_server_address(ctx.conf->network.endpoint.peer_ip.c_str(), ctx.conf->network.endpoint.peer_port);

    clock_gettime(CLOCK_MONOTONIC, &s_cur_time);
    s_header.set_once_per_round(REQ_QUERY_SERVER_STATUS, 1, sizeof(s_body), TIMESPEC_TO_SESSION_ID(s_cur_time));
    s_header.set_for_current_packet(sizeof(s_body));
    COMMPROTO_CPP_SERIALIZE(&s_header, s_buf.data(), sizeof(s_header));

    s_body.prefix.set(s_body.version(), 0);
    s_body.needs_live_stream = ctx.needs_live_stream;
    COMMPROTO_CPP_SERIALIZE(&s_body, s_buf.data() + sizeof(s_header), sizeof(s_body));

    return sendto(fd, s_buf.data(), s_buf.size(), 0, (struct sockaddr *)&s_server_addr, sizeof(s_server_addr));
}

static int receive_reply(int fd, const conf_file_t &conf, int max_size, void *buf)
{
    static thread_local fd_set s_fds;
    static thread_local struct timeval s_timeout;
    static thread_local auto &endpoint = conf.network.endpoint;
    uint16_t poll_timeout_ms = endpoint.poll_timeout_msecs;
    int ret;

    FD_ZERO(&s_fds);
    FD_SET(fd, &s_fds);

    s_timeout.tv_sec = poll_timeout_ms / 1000;
    s_timeout.tv_usec = (poll_timeout_ms % 1000) * 1000;

    if ((ret = select(fd + 1, &s_fds, nullptr, nullptr, &s_timeout)) <= 0)
    {
        if (ret)
            LOG_ERROR("*** select() failed: %s", strerror(errno));
        else
            LOG_ERROR("*** select() timed out after %d.%03ds!", poll_timeout_ms / 1000, poll_timeout_ms % 1000);

        return -1;
    }

    if (!FD_ISSET(fd, &s_fds)) // NOTE: This is not needed since select() above will return 0. But, keep it anyway.
    {
        LOG_WARNING("*** Server[%s:%d] not ready yet", endpoint.peer_ip.c_str(), endpoint.peer_port);

        return -1;
    }

    static thread_local struct sockaddr_in s_server_addr = \
        make_server_address(endpoint.peer_ip.c_str(), endpoint.peer_port);
    static thread_local struct sockaddr_in s_peer_addr;
    socklen_t addr_len = sizeof(s_peer_addr);

    if ((ret = recvfrom(fd, buf, max_size, 0, (struct sockaddr *)&s_peer_addr, &addr_len)) <= 0)
    {
        LOG_ERROR("*** recvfrom() %s %s", (ret ? "failed:" : ""), (ret ? strerror(errno) : "returned nothing"));

        return ret;
    }

    if (s_peer_addr.sin_family != s_server_addr.sin_family
        || s_peer_addr.sin_addr.s_addr != s_server_addr.sin_addr.s_addr
        || s_peer_addr.sin_port != s_server_addr.sin_port)
    {
        LOG_ERROR("*** Packet is not from server, ignored it.");

        return -1;
    }

    return ret;
}

static int parse_reply(const uint8_t *buf_ptr, int buf_size, packet_head_t &header,
    reply_0001_connect_t &conn_body, reply_0003_query_server_status_t &status_body)
{
    if (buf_size < (int)sizeof(header))
    {
        LOG_ERROR("*** No sufficient data for header parsing!");

        return -1;
    }

    commproto_result_t result = COMMPROTO_CPP_PARSE(buf_ptr, buf_size, &header);

    if (result.error_code < 0 || result.handled_len < sizeof(header))
    {
        LOG_ERROR("*** Failed to parse header: %s", commproto_error(result.error_code));

        return -1;
    }

    if (!header.is_valid())
    {
        LOG_ERROR("*** This is not a valid header!");

        return -1;
    }

    if (REPLY_CONNECT != header.command_code && REPLY_QUERY_SERVER_STATUS != header.command_code)
    {
        LOG_ERROR("*** This is neither a connect reply nor a server status reply!");

        return -1;
    }

    bool is_conn_reply = (REPLY_CONNECT == header.command_code);
    const char *body_name = is_conn_reply ? "connect" : "server status";
    int body_size = std::min(header.body_size(), (uint16_t)(is_conn_reply ? sizeof(conn_body) : sizeof(status_body)));

    buf_ptr += sizeof(header);
    buf_size -= sizeof(header);
    if (buf_size < body_size)
    {
        LOG_ERROR("*** No sufficient data for %s reply body parsing!", body_name);

        return -1;
    }

    if (is_conn_reply)
        result = COMMPROTO_CPP_PARSE(buf_ptr, body_size, &conn_body);
    else
        result = COMMPROTO_CPP_PARSE(buf_ptr, body_size, &status_body);

    if (result.error_code < 0 || (int)result.handled_len < body_size)
    {
        LOG_ERROR("*** Failed to parse %s reply body: %s", body_name, commproto_error(result.error_code));

        return -1;
    }

    return result.handled_len;
}

static inline QString make_realtime_info(const biz_context_t *ctx)
{
    return QString::asprintf("%u frames dropped during inference\n"
        "%u frames dropped during saving\n"
        "%u frames dropped during sending",
        (uint32_t)ctx->skipped_inference_count,
        (uint32_t)ctx->skipped_saving_count,
        (uint32_t)ctx->skipped_sending_count);
}

__attribute__((weak))
void biz_connect(biz_context_t *ctx, int index)
{
    SET_THREAD_NAME("lanc/connect");

    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    if (fd < 0 || send_connect_request(fd, *ctx) < 0)
    {
        LOG_ERROR("*** %s() failed: %s", ((fd < 0) ? "socket" : "send_connect_request"), strerror(errno));
        raise(SIGABRT);
        if (fd >= 0)
            close(fd);

        return;
    }

    auto &conf = *ctx->conf;
    auto &endpoint = conf.network.endpoint;
    std::vector<unsigned char> reply_buf(1024);
    packet_head_t header = {};
    reply_0001_connect_t conn_reply = {};
    const multicast_config_t &multicast = conn_reply.multicast;
    const int MIN_CONN_REPLY_SIZE = sizeof(conn_reply) - sizeof(conn_reply.startup_time_secs);
    reply_0003_query_server_status_t status_reply = {};
    packet_body_prefix_t *prefix = &conn_reply.prefix;
    int send_ret;
    int recv_ret = receive_reply(fd, conf, reply_buf.size(), reply_buf.data());
    int parse_ret = (recv_ret <= 0) ? -1
        : parse_reply(reply_buf.data(), recv_ret, header, conn_reply, status_reply);

    if (parse_ret < MIN_CONN_REPLY_SIZE || REPLY_CONNECT != header.command_code
        || (PROTO_ERR_OK != prefix->return_code/* && PROTO_ERR_PEER_ALREADY_EXISTED != prefix->return_code*/))
    {
        LOG_ERROR("*** Failed to receive or parse reply: command_code = 0x%04x, return_code = %d,"
            " parsed length = %d (min:%d | full:%d)", header.command_code, prefix->return_code,
            parse_ret, MIN_CONN_REPLY_SIZE, (int)sizeof(conn_reply));
        close(fd);
        raise(SIGABRT);

        return;
    }

    conf.camera.buffer_count = conn_reply.camera_buf_count;
    conf.inference.model.width = conn_reply.image_width;
    conf.inference.model.height = conn_reply.image_height;
    conf.camera.image_sizes.push_back({ conn_reply.image_width, conn_reply.image_height });
    conf.camera.which_size = 1;
    endpoint.self_ip = multicast.receiver_ip;
    endpoint.self_port = multicast.port;
    conf.network.multicast.ip = multicast.group_ip;
    conf.network.multicast.max_payload_size = multicast.max_payload_size;
    ctx->connected_to_server = true;
    LOG_NOTICE("Connection [%s:* -> %s:%d] established, multicast config for live stream is %s:%d",
        endpoint.self_ip.c_str(), endpoint.peer_ip.c_str(), endpoint.peer_port, multicast.group_ip, multicast.port);
    do
    {
        usleep(BIZ_POLL_TIMEOUT_MSECS * 1000);
        if (sig_check_critical_flag())
            break;
    }
    while (!ctx->widget);

    while (!sig_check_critical_flag())
    {
        if (PROTO_ERR_UNKNOWN_PEER == prefix->return_code)
            send_ret = send_connect_request(fd, *ctx);
        else
            send_ret = send_server_status_request(fd, *ctx);

        if (send_ret < 0)
            LOG_ERROR("*** Failed to send request: %s", strerror(errno));

        recv_ret = receive_reply(fd, conf, reply_buf.size(), reply_buf.data());

        usleep(endpoint.heartbeat_msecs * 1000);

        if (recv_ret <= 0)
            continue;

        if (parse_reply(reply_buf.data(), recv_ret, header, conn_reply, status_reply) < 0)
            continue;

        prefix = (REPLY_CONNECT == header.command_code) ? &conn_reply.prefix : &status_reply.prefix;
        if (PROTO_ERR_OK != prefix->return_code && PROTO_ERR_PEER_ALREADY_EXISTED != prefix->return_code)
            continue;

        if (REPLY_CONNECT != header.command_code)
        {
            if (ctx->should_save != status_reply.inference_positive)
            {
                if (status_reply.inference_positive)
                    ctx->widget->delegatingPlayRecordStartAudio();
                else
                {
                    ctx->widget->delegatingPlayRecordEndAudio();
                    ctx->widget->delegatingSyncLocalVideos();
                    ctx->widget->delegatingReloadVideoList();
                }
            }
            ctx->should_save = ctx->inference_positive = status_reply.inference_positive;

            if (!conf.save.enabled)
                ctx->skipped_saving_count = status_reply.dropped_saving_count;
            ctx->skipped_sending_count = status_reply.dropped_sending_count;
            ctx->skipped_inference_count = status_reply.dropped_inference_count;
            if (!ctx->needs_live_stream)
                ctx->widget->lblRealtimeInfo->setText(make_realtime_info(ctx));

            continue;
        } // if (REPLY_CONNECT != header.command_code)

        LOG_WARNING("[%s:* -> %s:%d] re-connected!!", multicast.receiver_ip, endpoint.peer_ip.c_str(), endpoint.peer_port);
        if (conn_reply.image_width != conf.camera.image_sizes[0].first
            || conn_reply.image_height != conf.camera.image_sizes[0].second)
        {
            LOG_ERROR("*** Server has changed window size to %dx%d, please restart client now!",
                conn_reply.image_width, conn_reply.image_height);
            raise(SIGABRT);
        }
        if (multicast.port != endpoint.self_port || multicast.group_ip != conf.network.multicast.ip
            || multicast.max_payload_size != conf.network.multicast.max_payload_size)
        {
            LOG_ERROR("*** Multicast config has changed to %s:%d (payload <= %d), please restart client now!",
                multicast.group_ip, multicast.port, multicast.max_payload_size);
            raise(SIGABRT);
        }
        endpoint.self_ip = multicast.receiver_ip;
        endpoint.self_port = multicast.port;
        conf.network.multicast.ip = multicast.group_ip;
        conf.network.multicast.max_payload_size = multicast.max_payload_size;
        ctx->frame_seq = 0;
    } // while (!sig_check_critical_flag())

    close(fd);
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-13, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 *
 * >>> 2026-04-26, Man Hung-Coeng <udc577@126.com>:
 *  01. Replace global PROTO_VERSION with version() in each protocol body class.
 */

