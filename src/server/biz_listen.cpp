// SPDX-License-Identifier: Apache-2.0

/*
 * Biz executor of listening and managing clients.
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

#include <set>

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"
#include "biz_protocols.hpp"

static inline bool is_multicast_or_broadcast(uint32_t addr)
{
    static const thread_local uint32_t S_BROADCAST_ADDR = ntohl(inet_addr("255.255.255.255"));
    static const thread_local uint32_t S_MULTICAST_ADDR_START = ntohl(inet_addr("224.0.0.0"));
    static const thread_local uint32_t S_MULTICAST_ADDR_END = ntohl(inet_addr("239.255.255.255"));

    addr = ntohl(addr);

    return (S_BROADCAST_ADDR == addr) || (addr >= S_MULTICAST_ADDR_START && addr <= S_MULTICAST_ADDR_END);
}

static int parse_request(const uint8_t *buf_ptr, int buf_size, packet_head_t &header,
    req_0000_connect_t &conn_body, req_0002_query_server_status_t &status_body)
{
    if (buf_size < (int)sizeof(header))
    {
        LOG_ERROR("*** %s", "No sufficient data for header parsing!");

        return -1;
    }

    commproto_result_t result = COMMPROTO_CPP_PARSE(buf_ptr, sizeof(header), &header);

    if (result.error_code < 0 || result.handled_len < sizeof(header))
    {
        LOG_ERROR("*** Failed to parse header: %s", commproto_error(result.error_code));

        return -1;
    }

    if (!header.is_valid())
    {
        LOG_ERROR("*** %s", "This is not a valid header!");

        return -1;
    }

    if (REQ_CONNECT != header.command_code && REQ_QUERY_SERVER_STATUS != header.command_code)
    {
        LOG_ERROR("*** %s", "This is neither a connect reply nor a server status request!");

        return -1;
    }

    bool is_conn_req = (REQ_CONNECT == header.command_code);
    const char *body_name = is_conn_req ? "connect" : "server status";
    int body_size = is_conn_req ? sizeof(conn_body) : sizeof(status_body);

    buf_ptr += sizeof(header);
    buf_size -= sizeof(header);
    if (buf_size < body_size)
    {
        LOG_ERROR("*** No sufficient data for %s request body parsing!", body_name);

        return -1;
    }

    if (is_conn_req)
        result = COMMPROTO_CPP_PARSE(buf_ptr, body_size, &conn_body);
    else
        result = COMMPROTO_CPP_PARSE(buf_ptr, body_size, &status_body);

    if (result.error_code < 0 || (int)result.handled_len < body_size)
    {
        LOG_ERROR("*** Failed to parse %s request body: %s", body_name, commproto_error(result.error_code));

        return -1;
    }

    return result.handled_len;
}

static thread_local packet_head_t s_header = {};

static void send_server_status_reply(int fd, const packet_head_t &header,
    const req_0002_query_server_status_t &body, uint16_t &return_code,
    const struct sockaddr_in &client_addr, const biz_context_t &ctx)
{
    static thread_local reply_0003_query_server_status_t s_body = {};
    static thread_local std::vector<unsigned char> s_buf(sizeof(s_header) + sizeof(s_body));

    s_header.set_once_per_round(REPLY_QUERY_SERVER_STATUS, 1, sizeof(s_body), header.session_id);
    s_header.set_for_current_packet(sizeof(s_body));
    COMMPROTO_CPP_SERIALIZE(&s_header, s_buf.data(), sizeof(s_header));

    s_body.prefix.set(body.prefix.version, return_code);
    if (PROTO_ERR_OK == return_code)
    {
        s_body.inference_positive = ctx.inference_positive;
        s_body.dropped_saving_count = ctx.skipped_saving_count;
        s_body.dropped_sending_count = ctx.skipped_sending_count;
        s_body.dropped_inference_count = ctx.skipped_inference_count;
    }
    COMMPROTO_CPP_SERIALIZE(&s_body, s_buf.data() + sizeof(s_header), sizeof(s_body));

    if (sendto(fd, s_buf.data(), s_buf.size(), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
        LOG_ERROR("*** Failed to send server status reply for session[%lu]: %s", header.session_id, strerror(errno));
}

static void send_connect_reply(int fd, const packet_head_t &header,
    const req_0000_connect_t &body, uint16_t &return_code,
    const struct sockaddr_in &client_addr, const biz_context_t &ctx)
{
    static thread_local reply_0001_connect_t s_body = {};
    static thread_local std::vector<unsigned char> s_buf(sizeof(s_header) + sizeof(s_body));

    s_header.set_once_per_round(REPLY_CONNECT, 1, sizeof(s_body), header.session_id);
    s_header.set_for_current_packet(sizeof(s_body));
    COMMPROTO_CPP_SERIALIZE(&s_header, s_buf.data(), sizeof(s_header));

    if ('\0' == body.name[0])
    {
        LOG_ERROR("*** %s", "Client name is null!");
        return_code = PROTO_ERR_NULL_NAME;
    }

    if ('\0' != body.name[sizeof(body.name) - 1])
    {
        LOG_ERROR("*** %s", "Client name is too long!");
        return_code = PROTO_ERR_NAME_TOO_LONG;
    }

    if (PROTO_ERR_OK == return_code)
        return_code = PROTO_ERR_PEER_ALREADY_EXISTED;

    s_body.prefix.set(body.prefix.version,
        (PROTO_ERR_UNKNOWN_PEER == return_code) ? (uint16_t)PROTO_ERR_OK : return_code);

    if (PROTO_ERR_UNKNOWN_PEER == return_code || PROTO_ERR_PEER_ALREADY_EXISTED == return_code)
    {
        const auto &conf = *ctx.conf;
        const auto &size = conf.camera.image_sizes[ctx.conf->camera.which_size - 1];

        s_body.image_width = size.first;
        s_body.image_height = size.second;
        s_body.camera_buf_count = ctx.conf->camera.buffer_count;
        strncpy(s_body.multicast.group_ip, conf.network.multicast.ip.c_str(), sizeof(s_body.multicast.group_ip) - 1);
        inet_ntop(AF_INET, &client_addr.sin_addr, s_body.multicast.receiver_ip, sizeof(s_body.multicast.receiver_ip));
        s_body.multicast.port = conf.network.multicast.port;
        s_body.multicast.max_payload_size = conf.network.multicast.max_payload_size;
    }
    COMMPROTO_CPP_SERIALIZE(&s_body, s_buf.data() + sizeof(s_header), sizeof(s_body));

    if (sendto(fd, s_buf.data(), s_buf.size(), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
        LOG_ERROR("*** Failed to send connect reply for session[%lu]: %s", header.session_id, strerror(errno));
}

__attribute__((weak))
void biz_listen(biz_context_t *ctx, int index)
{
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    SET_THREAD_NAME("lanc/listen");

    if (fd < 0)
    {
        LOG_ERROR("*** socket() failed: %s", strerror(errno));
        raise(SIGABRT);

        return;
    }

    const auto &conf = *ctx->conf;
    struct sockaddr_in self_addr = {};

    self_addr.sin_family = AF_INET;
    self_addr.sin_addr.s_addr = inet_addr(conf.network.endpoint.self_ip.c_str());
    self_addr.sin_port = htons(conf.network.endpoint.self_port);

    if (bind(fd, (struct sockaddr *)&self_addr, sizeof(self_addr)) < 0)
    {
        LOG_ERROR("*** bind() failed: %s", strerror(errno));
        close(fd);
        raise(SIGABRT);

        return;
    }

    uint32_t live_stream_addr = inet_addr(conf.network.multicast.ip.c_str());
    bool is_not_unicast = is_multicast_or_broadcast(live_stream_addr);
    bool is_test = ("test" == ctx->cmd_args->biz);
    fd_set fds;
    struct timeval poll_timesout;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    std::map<uint64_t, std::pair<std::string, bool>> clients;
    std::set<std::string> client_names;
    std::vector<unsigned char> buf(1024);
    packet_head_t header;
    req_0000_connect_t conn_req;
    req_0002_query_server_status_t status_req;
    uint16_t poll_timeout_msecs = std::max(conf.network.endpoint.poll_timeout_msecs, (uint16_t)BIZ_POLL_TIMEOUT_MSECS);
    int ret;
    int err;

    while (!sig_check_critical_flag())
    {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        poll_timesout.tv_sec = poll_timeout_msecs / 1000;
        poll_timesout.tv_usec = (poll_timeout_msecs % 1000) * 1000;

        if (0 == (ret = select(fd + 1, &fds, nullptr, nullptr, &poll_timesout)))
        {
            if (!clients.empty() || !client_names.empty())
            {
                clients.clear();
                client_names.clear();
                LOG_WARNING("%s", "Received no request for too long, cleaned all clients!");
            }
            ctx->needs_live_stream = is_test;
            //ctx->frame_seq = 0; // No need to reset this counter.

            continue;
        }

        if (ret < 0 && EINTR != (err = errno))
        {
            LOG_ERROR("*** select() failed: %s", strerror(err));
            //raise(SIGABRT);
        }

        if (ret < 0)
            continue;

        do
        {
            addr_len = sizeof(client_addr);
            ret = recvfrom(fd, buf.data(), buf.size(), 0, (struct sockaddr *)&client_addr, &addr_len);
            err = (ret > 0) ? 0 : ((ret < 0) ? errno : EINVAL);

            if (err)
            {
                if (EINTR == err && sig_check_critical_flag())
                    break;

                continue;
            }

            if (parse_request(buf.data(), ret, header, conn_req, status_req) < 0)
                continue;

            uint32_t cli_ip = client_addr.sin_addr.s_addr;
            uint64_t client_id = (((uint64_t)cli_ip) << 32) | client_addr.sin_port;
            const auto &client_iter = clients.find(client_id);
            uint16_t return_code = (clients.end() == client_iter) ? PROTO_ERR_UNKNOWN_PEER : PROTO_ERR_OK;

            if (REQ_QUERY_SERVER_STATUS == header.command_code)
            {
                int tmp_count = (status_req.needs_live_stream && (cli_ip == live_stream_addr || is_not_unicast)) ? 1 : 0;

                if (!is_test && 0 == tmp_count)
                {
                    for (auto iter = clients.begin(); (tmp_count <= 0) && (clients.end() != iter); ++iter)
                    {
                        cli_ip = iter->first >> 32;
                        tmp_count += (iter->second.second && (cli_ip == live_stream_addr || is_not_unicast));
                    }
                }
                ctx->needs_live_stream = (0 != tmp_count);
                if (PROTO_ERR_UNKNOWN_PEER != return_code)
                    client_iter->second.second = status_req.needs_live_stream;
                send_server_status_reply(fd, header, status_req, return_code, client_addr, *ctx);
            }
            else
            {
                if (PROTO_ERR_UNKNOWN_PEER == return_code && client_names.end() != client_names.find(conn_req.name))
                {
                    return_code = PROTO_ERR_PEER_ALREADY_EXISTED;
                    clients.insert({ client_id, { conn_req.name, false } });
                }

                if (PROTO_ERR_UNKNOWN_PEER != return_code)
                    LOG_WARNING("Client already exists: %s", conn_req.name);

                send_connect_reply(fd, header, conn_req, return_code, client_addr, *ctx);

                if (PROTO_ERR_UNKNOWN_PEER == return_code)
                {
                    clients.insert({ client_id, { conn_req.name, false } });
                    client_names.insert(conn_req.name);
                    LOG_NOTICE("Accepted new client: %s", conn_req.name);
                }
            }
        }
        while (EAGAIN != err && EWOULDBLOCK != err);
    } // while (!sig_check_critical_flag())

    close(fd);
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

