// SPDX-License-Identifier: Apache-2.0

/*
 * Biz executors of capturing frames from camera
 * and recording audio from microphone.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include <signal.h>
#include <linux/videodev2.h>

#include "signal_handling.h"

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"
#include "camera_v4l2.h"

static int set_raw_buffers_in_biz_context(const camera_v4l2_t &cam, biz_context_t &ctx)
{
    const auto &conf = *ctx.conf;
    const char *compression_algo = conf.video.compression.first.c_str();

    ctx.raw_buf_size = 0;
    for (uint8_t i = 0; i < cam.plane_count; ++i)
    {
        ctx.raw_buf_size += cam.buf_sizes[0][i];
    }

    ctx.raw_buffers.reserve(cam.buf_count);
    for (uint8_t i = 0; i < cam.buf_count; ++i)
    {
        ctx.raw_buffers.push_back(cam.buf_pointers[i][0]);
    }

    if (0 != strcasecmp(compression_algo, "lz4") && 0 != strcasecmp(compression_algo, "zlib"))
        return 0;

    if (V4L2_PIX_FMT_NV12 != cam.fmt_fourcc && V4L2_PIX_FMT_NV21 != cam.fmt_fourcc)
    {
        char fourcc_str[5] = { 0 };

        memcpy(fourcc_str, &cam.fmt_fourcc, sizeof(uint32_t));
        LOG_ERROR("*** %s compression can not used for %s format", compression_algo, fourcc_str);

        return -1;
    }

    if (V4L2_MEMORY_MMAP != cam.io_mode && V4L2_MEMORY_USERPTR != cam.io_mode)
    {
        LOG_ERROR("*** %s compression can not used for I/O mode %d", compression_algo, cam.io_mode);

        return -1;
    }

    if (1 != cam.plane_count)
    {
        LOG_ERROR("*** %s compression can not used for %d-planar data", compression_algo, cam.plane_count);

        return -1;
    }

    return cam.buf_count;
}

static std::vector<std::vector<cv::Mat>> make_input_matrixes(const camera_v4l2_t &cam)
{
    const unsigned char *fourcc = reinterpret_cast<const unsigned char *>(&cam.fmt_fourcc);
    std::vector<std::vector<cv::Mat>> result(cam.buf_count, std::vector<cv::Mat>());

    for (uint8_t i = 0; i < cam.buf_count; ++i)
    {
        result[i].reserve(cam.plane_count);
        if (1 == cam.plane_count)
        {
            switch (cam.fmt_fourcc)
            {
            case V4L2_PIX_FMT_BGR24:
                result[i].push_back(cv::Mat(cam.height, cam.width, CV_8UC3, cam.buf_pointers[i][0]));
                break;

            case V4L2_PIX_FMT_RGB565:
                result[i].push_back(cv::Mat(cam.height, cam.width, CV_8UC2, cam.buf_pointers[i][0]));
                break;

            case V4L2_PIX_FMT_GREY:
                result[i].push_back(cv::Mat(cam.height, cam.width, CV_8UC1, cam.buf_pointers[i][0]));
                break;

            case V4L2_PIX_FMT_NV12:
            case V4L2_PIX_FMT_NV21:
                result[i].push_back(cv::Mat(cam.height * 3 / 2, cam.width, CV_8UC1, cam.buf_pointers[i][0]));
                break;

            case V4L2_PIX_FMT_NV16:
            case V4L2_PIX_FMT_NV61:
                result[i].push_back(cv::Mat(cam.height * 2, cam.width, CV_8UC1, cam.buf_pointers[i][0]));
                break;

            case V4L2_PIX_FMT_YUYV:
            case V4L2_PIX_FMT_YYUV:
            case V4L2_PIX_FMT_YVYU:
            case V4L2_PIX_FMT_UYVY:
            case V4L2_PIX_FMT_VYUY:
                result[i].push_back(cv::Mat(cam.height, cam.width, CV_8UC2, cam.buf_pointers[i][0]));
                break;

            case V4L2_PIX_FMT_MJPEG:
                result[i].push_back(cv::Mat(1, cam.height * cam.width, CV_8UC1, cam.buf_pointers[i][0]));
                break;

            default:
                LOG_ERROR("*** Format not supported yet: %c%c%c%c", fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
                break;
            }
        }
        else if (2 == cam.plane_count)
        {
            switch (cam.fmt_fourcc)
            {
            case V4L2_PIX_FMT_NV12:
            case V4L2_PIX_FMT_NV21:
                result[i].push_back(cv::Mat(cam.height, cam.width, CV_8UC1, cam.buf_pointers[i][0]));
                result[i].push_back(cv::Mat(cam.height / 2, cam.width, CV_8UC1, cam.buf_pointers[i][1]));
                break;

            case V4L2_PIX_FMT_NV16:
            case V4L2_PIX_FMT_NV61:
                result[i].push_back(cv::Mat(cam.height, cam.width, CV_8UC1, cam.buf_pointers[i][0]));
                result[i].push_back(cv::Mat(cam.height, cam.width, CV_8UC1, cam.buf_pointers[i][1]));
                break;

            default:
                LOG_ERROR("*** Format not supported yet: %c%c%c%c", fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
                break;
            }
        }
        else
        {
            ; // TODO
        }
    } // for (i : cam.buf_count)

    return result;
}

static void convert_image_format(const std::vector<cv::Mat> &inputs, cv::Mat &output, uint32_t input_fourcc)
{
    int count = inputs.size();

    if (1 == count)
    {
        switch (input_fourcc)
        {
        case V4L2_PIX_FMT_RGB24:
            cv::cvtColor(inputs[0], output, cv::COLOR_RGB2BGR);
            break;

        case V4L2_PIX_FMT_RGB565:
            cv::cvtColor(inputs[0], output, cv::COLOR_BGR5652BGR);
            break;

        case V4L2_PIX_FMT_GREY:
            cv::cvtColor(inputs[0], output, cv::COLOR_GRAY2BGR);
            break;

        case V4L2_PIX_FMT_NV12:
            cv::cvtColor(inputs[0], output, cv::COLOR_YUV2BGR_NV12);
            break;
        case V4L2_PIX_FMT_NV21:
            cv::cvtColor(inputs[0], output, cv::COLOR_YUV2BGR_NV21);
            break;

        case V4L2_PIX_FMT_NV16:
            //cv::cvtColor(inputs[0], output, cv::COLOR_YUV2BGR_NV16); // FIXME: Missing this type?
            break;
        case V4L2_PIX_FMT_NV61:
            //cv::cvtColor(inputs[0], output, cv::COLOR_YUV2BGR_NV61); // FIXME: Missing this type?
            break;

        case V4L2_PIX_FMT_YUYV:
            cv::cvtColor(inputs[0], output, cv::COLOR_YUV2BGR_YUYV);
            break;
        case V4L2_PIX_FMT_YYUV:
            //cv::cvtColor(inputs[0], output, cv::COLOR_YUV2BGR_YYUV); // FIXME: Missing this type?
            break;
        case V4L2_PIX_FMT_YVYU:
            cv::cvtColor(inputs[0], output, cv::COLOR_YUV2BGR_YVYU);
            break;
        case V4L2_PIX_FMT_UYVY:
            cv::cvtColor(inputs[0], output, cv::COLOR_YUV2BGR_UYVY);
            break;
        case V4L2_PIX_FMT_VYUY:
            //cv::cvtColor(inputs[0], output, cv::COLOR_YUV2BGR_VYUY); // FIXME: Missing this type?
            break;

        case V4L2_PIX_FMT_MJPEG:
            cv::imdecode(inputs[0], cv::IMREAD_COLOR, &output);
            break;

        default:
            break;
        }
    }
    else if (2 == count)
    {
        switch (input_fourcc)
        {
        case V4L2_PIX_FMT_NV12:
            cv::cvtColorTwoPlane(inputs[0], inputs[1], output, cv::COLOR_YUV2BGR_NV12);
            break;
        case V4L2_PIX_FMT_NV21:
            cv::cvtColorTwoPlane(inputs[0], inputs[1], output, cv::COLOR_YUV2BGR_NV21);
            break;

        case V4L2_PIX_FMT_NV16:
            //cv::cvtColorTwoPlane(inputs[0], inputs[1], output, cv::COLOR_YUV2BGR_NV16); // FIXME: Missing this type?
            break;
        case V4L2_PIX_FMT_NV61:
            //cv::cvtColorTwoPlane(inputs[0], inputs[1], output, cv::COLOR_YUV2BGR_NV61); // FIXME: Missing this type?
            break;

        default:
            break;
        }
    }
    else if (3 == count)
    {
        // TODO
    }
    else
        LOG_ERROR("**** Found %d planes! Are you sure?", count);
}

__attribute__((weak))
void biz_capture_image(biz_context_t *ctx, int index)
{
    const auto &conf = *ctx->conf;
    const auto &size = conf.camera.image_sizes[conf.camera.which_size - 1];
    const uint32_t *io_modes = conf.camera.io_modes.empty() ? nullptr
        : conf.camera.io_modes.data();
    camera_v4l2_t cam = camera_v4l2(conf.logger.level.c_str());

    SET_THREAD_NAME("lanc/capture:v");

    if (cam.open(&cam, conf.camera.device.c_str(), /* is_nonblocking = */false) < 0
        || cam.query_capabilities(&cam, /* with_validation = */true) < 0
        || cam.match_format(&cam, conf.camera.capture_format.c_str()) < 0
        || cam.set_size_and_format(&cam, size.first, size.second) < 0
        || cam.set_frame_rate(&cam, atof(ctx->cmd_args->fps.c_str()), conf.camera.fallback_fps) < 0
        || cam.alloc_buffers(&cam, conf.camera.buffer_count, io_modes) < 0
        || cam.start_capture(&cam, /* needs_dma_sync = */true) < 0
        || set_raw_buffers_in_biz_context(cam, *ctx) < 0)
    {
        LOG_ERROR("*** %s() failed: %s", cam.last_func, strerror(-cam.err));
        cam.free_buffers_if_any(&cam);
        cam.close(&cam);
        raise(SIGABRT); // Notify other threads to abort.

        return;
    }

    const std::vector<std::vector<cv::Mat>> &frame_matrixes = make_input_matrixes(cam);
    struct v4l2_buffer frame;
    struct timespec start_time;
    struct timespec end_time;
    std::vector<struct timespec> cost_times;
    bool is_test = ("test" == ctx->cmd_args->biz);
    const int MAX_TEST_FRAME_COUNT = cam.fps * conf.test.capture_duration_secs;
    char fps_str[8] = { 0 };
    int i = 0;

    snprintf(fps_str, sizeof(fps_str) - 1, "%.1f", cam.fps);
    ctx->cmd_args->fps = fps_str;
    if (is_test)
        cost_times.resize(conf.test.capture_duration_secs, {});

    while (true)
    {
        if (cam.wait_and_fetch(&cam, /* timeout_msecs = */0, &frame) < 0)
            break;

        int frm_idx = frame.index;

        clock_gettime(CLOCK_REALTIME, &ctx->timestamps[frm_idx]);
        if (is_test)
            clock_gettime(CLOCK_MONOTONIC, &start_time);

        if (V4L2_PIX_FMT_BGR24 == cam.fmt_fourcc)
            memcpy(ctx->rgb_buffers[frm_idx].data(), cam.buf_pointers[frm_idx][0], cam.buf_sizes[frm_idx][0]);
        else
            convert_image_format(frame_matrixes[frm_idx], ctx->rgb_matrixes[frm_idx], cam.fmt_fourcc);

        if (is_test)
        {
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            cost_times[i++ % conf.test.capture_duration_secs] = subtract_timespec(end_time, start_time);
            if (i > MAX_TEST_FRAME_COUNT)
                break;
        }

        if (true)
        {
            std::unique_lock<std::mutex> lock(*ctx->capture_lock);

            ctx->buf_index_of_latest_frame = frm_idx;
            ++ctx->frame_seq;
            ++ctx->unsaved_count;
            ++ctx->unsent_count;
            ctx->capture_notifier->notify_all();
        }

        if (sig_check_critical_flag() || cam.enqueue_buffer(&cam, frm_idx) < 0)
            break;
    } // while (true)

    cam.stop_capture(&cam);

    if (cam.err < 0 && !sig_check_critical_flag())
        raise(SIGABRT); // Notify other threads to abort.

    if (!sig_check_critical_flag())
        raise(SIGTERM); // Notify other threads to terminate.

    if (is_test)
    {
        for (int i = 0; i < conf.test.capture_duration_secs; ++i)
        {
            LOG_INFO("[%d] Cost time of format conversion: %ld.%09ld s",
                i, cost_times[i].tv_sec, cost_times[i].tv_nsec);
        }
    }

    usleep(std::min(std::max(BIZ_POLL_TIMEOUT_MSECS, 100), 500) * 1000 / 10);
    cam.free_buffers_if_any(&cam);
    cam.close(&cam);
}

// TODO:
//__attribute__((weak))
//void biz_capture_audio(biz_context_t *ctx, int index)
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

