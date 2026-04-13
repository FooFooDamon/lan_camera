// SPDX-License-Identifier: Apache-2.0

/*
 * Biz executor of receiving image and audio streams from server.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include <signal.h> // For raise() and SIG*.
#include <unistd.h>
#include <sys/socket.h>
//#include <netinet/in.h> // For struct sockaddr_in and htons().
#include <arpa/inet.h> // For inet_addr() and IP*.

#include "signal_handling.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"
#include "biz_protocols.hpp"

static inline bool is_multicast_address(uint32_t addr)
{
    static const thread_local uint32_t S_MULTICAST_ADDR_START = ntohl(inet_addr("224.0.0.0"));
    static const thread_local uint32_t S_MULTICAST_ADDR_END = ntohl(inet_addr("239.255.255.255"));

    addr = ntohl(addr);

    return (addr >= S_MULTICAST_ADDR_START && addr <= S_MULTICAST_ADDR_END);
}

__attribute__((weak))
void biz_receive(biz_context_t *ctx, int index)
{
    SET_THREAD_NAME("lanc/recv");

    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    if (fd < 0)
    {
        LOG_ERROR("*** socket() failed: %s", strerror(errno));
        raise(SIGABRT);

        return;
    }

    const auto &conf = *ctx->conf;
    const auto &endpoint = conf.network.endpoint;
    struct sockaddr_in addr = {};
    struct ip_mreq mreq = {};
    //bool is_on_same_host = (endpoint.self_ip == endpoint.peer_ip);
    bool is_via_multicast = is_multicast_address(inet_addr(conf.network.multicast.ip.c_str()));
    char self_ip[16] = {};
    char group_ip[16] = {};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.self_port);
    addr.sin_addr.s_addr = /*is_on_same_host ? */htonl(INADDR_ANY)/* : inet_addr(endpoint.self_ip.c_str())*/;
    mreq.imr_interface.s_addr = inet_addr(endpoint.self_ip.c_str());
    mreq.imr_multiaddr.s_addr = inet_addr(conf.network.multicast.ip.c_str());

    if (/* setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, ...) < 0
        || */bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0
        || (is_via_multicast && setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0))
    {
        LOG_ERROR("*** bind() or setsockopt(ADD_MEMBERSHIP) failed: %s", strerror(errno));
        close(fd);
        raise(SIGABRT);

        return;
    }
    inet_ntop(AF_INET, &addr.sin_addr, self_ip, sizeof(self_ip));
    inet_ntop(AF_INET, &mreq.imr_multiaddr, group_ip, sizeof(group_ip));
    LOG_NOTICE("Bound to %s:%d to receive live stream from %s",
        self_ip, endpoint.self_port, (is_via_multicast ? group_ip : endpoint.peer_ip.c_str()));

    packet_head_t header = {};
    reply_0005_live_stream_t body = {};
    fd_set fds;
    struct timeval poll_timesout;
    uint16_t poll_timeout_msecs = std::max(endpoint.poll_timeout_msecs, (uint16_t)BIZ_POLL_TIMEOUT_MSECS);
    std::vector<unsigned char> recv_buf(sizeof(header) + body.sizeof_info_fields() + conf.network.multicast.max_payload_size);
    uint32_t spliced_size = 0;
    const auto &img_size = conf.camera.image_sizes[conf.camera.which_size - 1];
    std::vector<std::vector<uint8_t>> raw_buffers(conf.camera.buffer_count,
        std::vector<uint8_t>(img_size.first * img_size.second * 3, 0));
    uint8_t buf_idx;
    bool should_display = false;
    bool already_displayed = false;
    char fps[16] = { 0 };
    int ret;
    int err;

    ctx->raw_buf_size = raw_buffers[0].size();

    while (!sig_check_critical_flag())
    {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        poll_timesout.tv_sec = poll_timeout_msecs / 1000;
        poll_timesout.tv_usec = (poll_timeout_msecs % 1000) * 1000;

        if (0 == (ret = select(fd + 1, &fds, nullptr, nullptr, &poll_timesout)))
            continue;

        if (ret < 0)
        {
            if (EINTR == (err = errno))
                continue;
            else
            {
                LOG_ERROR("*** select() failed: %s", strerror(err));
                raise(SIGABRT);

                break;
            }
        }

        buf_idx = (ctx->buf_index_of_latest_frame + 1) % conf.camera.buffer_count;

        do // while (EAGAIN != err && EWOULDBLOCK != err)
        {
            ret = recvfrom(fd, recv_buf.data(), recv_buf.size(), 0, nullptr, nullptr);
            err = (ret > 0) ? 0 : ((ret < 0) ? errno : EINVAL);

            if (err || ret < (int)sizeof(header))
            {
                if (EINTR == err && sig_check_critical_flag())
                    break;

                continue;
            }

            commproto_result_t result = COMMPROTO_CPP_PARSE(recv_buf.data(), sizeof(header), &header);

            if (result.error_code < 0 || result.handled_len < sizeof(header))
            {
                LOG_ERROR("*** Failed to parse header: %s", commproto_error(result.error_code));

                continue;
            }

            if (!header.is_valid() || REPLY_LIVE_STREAM != header.command_code)
            {
                LOG_ERROR("*** This is not a %s!", header.is_valid() ? "live stream packet" : "valid header");

                continue;
            }

            if (header.session_id > ctx->frame_seq) // new frame
            {
                ctx->frame_seq = header.session_id;
                spliced_size = 0;
                already_displayed = false;
            }

            if (header.session_id < ctx->frame_seq /* overdue old frame */ || already_displayed)
                continue;

            const uint8_t *body_ptr = recv_buf.data() + sizeof(header);
            uint16_t body_size = ret - sizeof(header);
            uint16_t total_size = header.length + sizeof(packet_head_t::length);

            if (ret < total_size || (header.packet_seq <= 1 && body_size < body.sizeof_info_fields()))
            {
                LOG_ERROR("*** This fragment might be truncated: actual(%u) < expected(%u), or body(%u) < info(%u)",
                    ret, total_size, body_size, body.sizeof_info_fields());

                continue;
            }

            if (header.packet_seq <= 1)
            {
                result = COMMPROTO_CPP_PARSE(body_ptr, body.sizeof_info_fields(), &body);

                if (result.error_code < 0 || (int)result.handled_len < body.sizeof_info_fields())
                {
                    LOG_ERROR("*** Failed to parse body: %s", commproto_error(result.error_code));

                    continue;
                }

                if (body.data_size > ctx->raw_buf_size)
                {
                    LOG_ERROR("*** Stream data size %u is bigger than the maximum buffer size %u",
                        body.data_size, ctx->raw_buf_size);

                    continue;
                }

                ctx->timestamps[buf_idx].tv_sec = body.time_secs;
                ctx->timestamps[buf_idx].tv_nsec = body.time_nsecs;
                snprintf(fps, sizeof(fps) - 1, "%.1f", body.fps);
                ctx->cmd_args->fps = fps;

                continue;
            } // if (header.packet_seq <= 1)

            uint32_t data_offset = header.body_offset() - body.sizeof_info_fields();

            if (data_offset + body_size > body.data_size)
            {
                LOG_ERROR("***[%lu] Fragment[%u] out of bound: offset = %u, this size = %u, total size = %u",
                    header.session_id, header.packet_seq, data_offset, body_size, body.data_size);

                continue;
            }

            memcpy(raw_buffers[buf_idx].data() + data_offset, body_ptr, body_size);
            spliced_size += body_size;

            if (spliced_size >= body.data_size || header.packet_seq == header.total_packets)
                should_display = true;
        }
        while (EAGAIN != err && EWOULDBLOCK != err);

        if (should_display)
        {
            spliced_size = 0;
            should_display = false;
            already_displayed = true;
            if (img_size.first == body.image_width && img_size.second == body.image_height)
                cv::imdecode(raw_buffers[buf_idx], cv::IMREAD_COLOR, &ctx->rgb_matrixes[buf_idx]);

            std::unique_lock<std::mutex> lock(*ctx->capture_lock);

            ctx->buf_index_of_latest_frame = buf_idx;
            ++ctx->unsaved_count;
            ctx->capture_notifier->notify_all();
        }
    } // while (!sig_check_critical_flag())

    if (is_via_multicast && setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
        LOG_ERROR("*** setsockopt(DROP_MEMBERSHIP) failed: %s", strerror(errno));

    close(fd);
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-13, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

