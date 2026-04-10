// SPDX-License-Identifier: Apache-2.0

/*
 * Biz executors of sending image streams, audio streams
 * and inference results to client.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include <signal.h> // For raise() and SIG*.
#include <unistd.h> // For usleep() and close().
#include <sys/ioctl.h>
#include <sys/socket.h>
//#include <netinet/in.h> // For struct sockaddr_in and htons().
#include <arpa/inet.h> // For inet_addr() and IP*.
#include <net/if.h> // For struct ifreq.
#if 0
#include <lz4.h>
#include <zlib.h>
#endif

#include "signal_handling.h"

#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"
#include "biz_protocols.hpp"

enum
{
    TTL_FOR_SAME_HOST = 0,
    TTL_FOR_SAME_SUBNET = 1,
    TTL_FOR_SAME_SITE = 32,
    TTL_FOR_SAME_REGION = 64,
    TTL_FOR_SAME_STATE = 128,
    TTL_UNLIMITED = 255
};

static int get_address_by_name(int fd, const char *name, struct in_addr &addr)
{
    if (name[0] == '*' || 0 == strcasecmp(name, "ALL"))
    {
        addr.s_addr = INADDR_ANY;

        return 0;
    }

    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, name, IFNAMSIZ);

    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0)
        return -1;

    addr.s_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;

    return 0;
}

static int to_jpeg(const cv::Mat &mat, const unsigned char *buf, uint32_t length,
    int compression_level, std::vector<unsigned char> &result)
{
    static thread_local std::vector<int> s_params = {
        cv::IMWRITE_JPEG_QUALITY, compression_level,
    };

    return cv::imencode(".jpg", mat, result, s_params) ? result.size() : -1;
}

static int to_png(const cv::Mat &mat, const unsigned char *buf, uint32_t length,
    int compression_level, std::vector<unsigned char> &result)
{
    static thread_local std::vector<int> s_params = {
        cv::IMWRITE_PNG_COMPRESSION, compression_level,
        cv::IMWRITE_PNG_STRATEGY, cv::IMWRITE_PNG_STRATEGY_FIXED,
    };

    return cv::imencode(".png", mat, result, s_params) ? result.size() : -1;
}

static int to_webp(const cv::Mat &mat, const unsigned char *buf, uint32_t length,
    int compression_level, std::vector<unsigned char> &result)
{
    static thread_local std::vector<int> s_params = {
        cv::IMWRITE_WEBP_QUALITY, compression_level,
    };

    return cv::imencode(".webp", mat, result, s_params) ? result.size() : -1;
}

#if 0

static int to_lz4(const cv::Mat &mat, const unsigned char *buf, uint32_t length,
    int compression_level, std::vector<unsigned char> &result)
{
    int ret = LZ4_compress_fast((const char *)buf, (char *)result.data(), length, length, compression_level);

    return (ret > 0) ? ret : -1;
}

static int to_zlib(const cv::Mat &mat, const unsigned char *buf, uint32_t length,
    int compression_level, std::vector<unsigned char> &result)
{
    uLongf result_size = length;
    int ret = compress2(result.data(), &result_size, buf, length, compression_level);

    return (Z_OK == ret) ? result_size : -1;
}

#endif

typedef int (*compression_func_t)(const cv::Mat &, const unsigned char *, uint32_t, int, std::vector<unsigned char> &);

static const struct
{
    const char *algo;
    enum compression_algo_e code;
    compression_func_t func;
} S_COMPRESSION_MAPPINGS[] = {
    { "jpeg", COMPRESSION_JPEG, to_jpeg },
    { "png", COMPRESSION_PNG, to_png },
    { "webp", COMPRESSION_WEBP, to_webp },
#if 0
    { "lz4", COMPRESSION_LZ4, to_lz4 },
    { "zlib", COMPRESSION_ZLIB, to_zlib },
#endif
};

static inline compression_func_t get_compression_function(const char *algo)
{
    for (size_t i = 0; i < sizeof(S_COMPRESSION_MAPPINGS) / sizeof(S_COMPRESSION_MAPPINGS[0]); ++i)
    {
        if (0 == strcasecmp(algo, S_COMPRESSION_MAPPINGS[i].algo))
            return S_COMPRESSION_MAPPINGS[i].func;
    }

    LOG_ERROR("*** No handling function for compression algorithm: %s", algo);

    return nullptr;
}

static inline enum compression_algo_e get_compression_code(const char *algo)
{
    for (size_t i = 0; i < sizeof(S_COMPRESSION_MAPPINGS) / sizeof(S_COMPRESSION_MAPPINGS[0]); ++i)
    {
        if (0 == strcasecmp(algo, S_COMPRESSION_MAPPINGS[i].algo))
            return S_COMPRESSION_MAPPINGS[i].code;
    }

    LOG_ERROR("*** Invalid compression algorithm: %s", algo);

    return COMPRESSION_NONE;
}

static inline enum image_format_e get_image_format(enum compression_algo_e compression)
{
    switch (compression)
    {
    case COMPRESSION_JPEG:
        return IMG_FORMAT_JPEG;

    case COMPRESSION_PNG:
        return IMG_FORMAT_PNG;

    case COMPRESSION_WEBP:
        return IMG_FORMAT_WEBP;

    default:
        return IMG_FORMAT_NOT_CARED;
    }
}

static inline bool is_supported_by_opencv(enum compression_algo_e compression)
{
    switch (compression)
    {
    case COMPRESSION_JPEG:
    case COMPRESSION_PNG:
    case COMPRESSION_WEBP:
        return true;

    default:
        return false;
    }
}

static inline ssize_t retriable_sendmsg(int fd, const struct msghdr &msg, uint8_t max_retries, int timeout_usecs_per_retry)
{
#if 0
    ssize_t ret = 0;

    for (uint8_t i = 0; i < max_retries + 1; ++i)
    {
        if ((ret = sendmsg(fd, &msg, 0)) > 0)
            return ret;

        int err = (ret < 0) ? errno : EINVAL;

        if (EINTR != err && EAGAIN != err && EWOULDBLOCK != err)
            return -err;

        if (i < max_retries)
            usleep(timeout_usecs_per_retry);
    }

    return (ret < 0) ? -errno : -EINVAL;
#else
    int total_timeout_usecs = timeout_usecs_per_retry * max_retries;
    fd_set fds;
    struct timeval timesout;
    int ret;
    int err;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    timesout.tv_sec = total_timeout_usecs / 1000000;
    timesout.tv_usec = total_timeout_usecs % 1000000;

lbl_sendmsg_poll:
    if (0 == (ret = select(fd + 1, nullptr, &fds, nullptr, &timesout)))
    {
        //LOG_ERROR("*** Timed out after %d us", total_delay_usecs);

        return -EAGAIN;
    }

    if (ret < 0)
    {
        if (EINTR == (err = errno))
            goto lbl_sendmsg_poll;

        return -err;
    }

    if ((ret = sendmsg(fd, &msg, 0)) > 0)
        return ret;

    return (ret < 0) ? -errno : -EINVAL;
#endif
}

__attribute__((weak))
void biz_send_image(biz_context_t *ctx, int index)
{
    SET_THREAD_NAME("lanc/send:v");

    do
    {
        usleep(BIZ_POLL_TIMEOUT_MSECS * 1000);
        if (sig_check_critical_flag())
            return;
    }
    while (0 == ctx->raw_buf_size);

    const auto &conf = *ctx->conf;
    const char *compression_algo = conf.video.compression.first.c_str();
    int compression_level = conf.video.compression.second;
    compression_func_t compress_image;

    if (nullptr == (compress_image = get_compression_function(compression_algo)))
    {
        raise(SIGABRT);

        return;
    }

    const auto &multicast = conf.network.multicast;
    const auto &send_policy = multicast.send_policy;
    const char *iface_name = send_policy.interface.c_str();
    struct in_addr iface_addr;
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    unsigned char ttl = TTL_FOR_SAME_SITE;
    int needs_local_copy = send_policy.needs_local_copy;
    int allows_broadcast = 1;

    if (fd < 0
        || setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0
        || get_address_by_name(fd, iface_name, iface_addr) < 0
        || setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &iface_addr, sizeof(iface_addr)) < 0
        || setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &needs_local_copy, sizeof(needs_local_copy)) < 0
        || setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_policy.sendbuf_size, sizeof(send_policy.sendbuf_size)) < 0
        || setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &allows_broadcast, sizeof(allows_broadcast)) < 0)
    {
        LOG_ERROR("*** %s() failed: %s", ((fd < 0) ? "socket" : "ioctl/setsockopt"), strerror(errno));
        raise(SIGABRT);
        if (fd >= 0)
            close(fd);

        return;
    }

    bool is_test = ("test" == ctx->cmd_args->biz);
    const auto &img_size = conf.camera.image_sizes[conf.camera.which_size - 1];
    const uint16_t MAX_PAYLOAD_SIZE = multicast.max_payload_size;
    packet_head_t header = {};
    reply_0005_live_stream_t body = {};
    std::vector<unsigned char> header_buf(sizeof(header));
    std::vector<unsigned char> desc_buf(body.sizeof_info_fields());
    std::vector<unsigned char> encoded_buf;
    std::vector<uint32_t> encoded_sizes;
    struct sockaddr_in addr = {};
    struct msghdr msg = {};
    struct iovec iov[] = { {}, {} };
    struct timespec start_time;
    struct timespec end_time;
    std::vector<struct timespec> cost_times;
    uint64_t frame_seq;
    uint8_t buf_idx;
    uint8_t batch_size = send_policy.packets_per_batch;
    uint16_t batch_spacing_usecs = send_policy.batch_gap_usecs;
    int timeout_usecs_per_retry = batch_spacing_usecs / batch_size;
    int ret;
    int i = 0;

    body.prefix.set(PROTO_VERSION, PROTO_ERR_OK);
    body.compression_algo = get_compression_code(compression_algo);
    body.compression_level = conf.video.compression.second;
    body.image_format = get_image_format((enum compression_algo_e)body.compression_algo);
    body.image_width = img_size.first;
    body.image_height = img_size.second;
    body.image_channels = 3;
    body.fps = atof(ctx->cmd_args->fps.c_str());

    if (!is_supported_by_opencv((enum compression_algo_e)body.compression_algo))
        encoded_buf.resize(ctx->raw_buf_size);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(multicast.ip.c_str());
    addr.sin_port = htons(multicast.port);

    msg.msg_name = &addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_iov = iov;
    msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

    iov[0].iov_base = header_buf.data();
    iov[0].iov_len = header_buf.size();

    if (is_test)
    {
        cost_times.resize(conf.test.capture_duration_secs, {});
        encoded_sizes.resize(conf.test.capture_duration_secs, 1);
    }

    while (!sig_check_critical_flag())
    {
        if (ctx->unsent_count < 1)
        {
            std::unique_lock<std::mutex> lock(*ctx->capture_lock);

            ctx->capture_notifier->wait_for(lock, std::chrono::milliseconds(BIZ_POLL_TIMEOUT_MSECS));
        }

        if (ctx->unsent_count < 1)
            continue;

        if (--ctx->unsent_count > 0)
        {
            if (ctx->needs_live_stream)
                ++ctx->skipped_sending_count;

            continue;
        }

        if (!ctx->needs_live_stream)
            continue;

        buf_idx = ctx->buf_index_of_latest_frame;
        frame_seq = ctx->frame_seq;

        if (is_test)
            clock_gettime(CLOCK_MONOTONIC, &start_time);

        int encoded_size = compress_image(ctx->rgb_matrixes[buf_idx], ctx->raw_buffers[buf_idx],
            ctx->raw_buf_size, compression_level, encoded_buf);

        if (is_test)
        {
            encoded_sizes[i % conf.test.capture_duration_secs] = encoded_size;
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            cost_times[i++ % conf.test.capture_duration_secs] = subtract_timespec(end_time, start_time);

            continue;
        }

        if (encoded_size < 0)
        {
            ++ctx->skipped_sending_count;

            continue;
        }

        header.set_once_per_round(REPLY_LIVE_STREAM,
            1 + (encoded_size / MAX_PAYLOAD_SIZE) + ((encoded_size % MAX_PAYLOAD_SIZE) ? 1 : 0),
            body.sizeof_info_fields() + encoded_size,
            frame_seq);

        iov[1].iov_base = desc_buf.data();
        iov[1].iov_len = desc_buf.size();
        body.inference_positive = ctx->inference_positive;
        body.time_secs = ctx->timestamps[buf_idx].tv_sec;
        body.time_nsecs = ctx->timestamps[buf_idx].tv_nsec;
        body.data_size = encoded_size;
        COMMPROTO_CPP_SERIALIZE(&body, (uint8_t *)iov[1].iov_base, iov[1].iov_len);

        header.set_for_current_packet(iov[1].iov_len);
        COMMPROTO_CPP_SERIALIZE(&header, header_buf.data(), sizeof(header));

        if ((ret = retriable_sendmsg(fd, msg, batch_size * 2, timeout_usecs_per_retry)) < 0)
        {
            ++ctx->skipped_sending_count;
            LOG_ERROR("*** Failed to send info fragment (%lu bytes) of frame[%lu]: %d(%s)",
                iov[1].iov_len, frame_seq, -ret, strerror(-ret));

            continue;
        }

        for (uint16_t j = 1; j < header.total_packets; ++j)
        {
            int offset = MAX_PAYLOAD_SIZE * (j - 1);

            if (0 == (j % batch_size))
                usleep(batch_spacing_usecs);

            iov[1].iov_base = encoded_buf.data() + offset;
            iov[1].iov_len = (j < header.total_packets - 1) ? MAX_PAYLOAD_SIZE : (encoded_size - offset);

            header.set_for_current_packet(iov[1].iov_len);
            COMMPROTO_CPP_SERIALIZE(&header, header_buf.data(), sizeof(header));

            if ((ret = retriable_sendmsg(fd, msg, batch_size, timeout_usecs_per_retry)) < 0)
            {
                LOG_ERROR("*** Failed to send fragment[%d/%d] (%d/%d bytes) of frame[%lu]: %d(%s)",
                    header.packet_seq - 1, header.total_packets - 1, (int)iov[1].iov_len, encoded_size,
                    frame_seq, -ret, strerror(-ret));

                break;
            }
        }
    } // while (!sig_check_critical_flag())

    close(fd);

    if (is_test)
    {
        for (i = 0; i < conf.test.capture_duration_secs; ++i)
        {
            LOG_INFO("[%d] %d-level %s compressed %d bytes to %d bytes (-> ratio:%.02f) within %ld.%09ld s",
                i, compression_level, compression_algo, ctx->raw_buf_size, encoded_sizes[i],
                (float)ctx->raw_buf_size / encoded_sizes[i], cost_times[i].tv_sec, cost_times[i].tv_nsec);
        }
    }
}

// TODO:
//__attribute__((weak))
//void biz_send_audio(biz_context_t *ctx, int index)
//{
//}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

