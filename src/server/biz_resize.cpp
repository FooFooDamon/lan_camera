// SPDX-License-Identifier: Apache-2.0

/*
 * Biz executor of resizing images.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include "signal_handling.h"

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"

__attribute__((weak))
void biz_resize(biz_context_t *ctx, int index)
{
    bool is_test = ("test" == ctx->cmd_args->biz);
    const auto &conf = *ctx->conf;
    const auto &size = conf.camera.image_sizes[conf.camera.which_size - 1];
    const auto &model = conf.inference.model;
    bool can_copy_directly = (size.first == model.width && size.second <= model.height);
    double width_height_ratio = size.first / (double)size.second;
    bool src_fatter_than_model = (width_height_ratio >= (model.width / (double)model.height));
    int resized_width = src_fatter_than_model ? model.width : (model.height * width_height_ratio);
    int resized_height = src_fatter_than_model ? (model.width / width_height_ratio) : model.height;
    std::vector<cv::Mat> matrixes;
    struct timespec start_time;
    struct timespec end_time;
    std::vector<struct timespec> cost_times;
    uint8_t buf_idx;
    int8_t idx_to_infer = -1;
    int i = 0;

    SET_THREAD_NAME("lanc/resize");

    if (is_test)
        cost_times.resize(conf.test.capture_duration_secs, {});

    matrixes.reserve(conf.camera.buffer_count);
    for (buf_idx = 0; buf_idx < conf.camera.buffer_count; ++buf_idx)
    {
        matrixes.push_back(cv::Mat(resized_height, resized_width, CV_8UC3,
            ctx->resized_buffers[buf_idx].data(), model.width * 3));
    }

    while (true)
    {
        if (sig_check_critical_flag())
            break;

        if (true)
        {
            std::unique_lock<std::mutex> lock(*ctx->capture_lock);

            ctx->capture_notifier->wait_for(lock, std::chrono::milliseconds(BIZ_POLL_TIMEOUT_MSECS));
        }

        if (ctx->inference_paused || (ctx->frame_seq % (conf.inference.spacing_frame_count + 1)) > 0)
        {
            std::unique_lock<std::mutex> lock(*ctx->infer_lock);

            ctx->buf_index_to_infer = -1; // No frame for inference.
            ++ctx->uninferred_count; // Still increase the counter.
            ctx->infer_notifier->notify_all(); // Still notify backend.

            continue;
        }

        if (is_test)
            clock_gettime(CLOCK_MONOTONIC, &start_time);

        buf_idx = ctx->buf_index_of_latest_frame;
        idx_to_infer = (idx_to_infer + 1) % conf.camera.buffer_count;

        if (conf.inference.enabled)
        {
            if (can_copy_directly)
            {
                memcpy(ctx->resized_buffers[idx_to_infer].data(), ctx->rgb_buffers[buf_idx].data(),
                    size.first * size.second * 3);
            }
            else
            {
                cv::resize(ctx->rgb_matrixes[buf_idx], matrixes[idx_to_infer], matrixes[idx_to_infer].size(),
                    0, 0, cv::INTER_AREA);
            }
        }

        if (is_test)
        {
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            cost_times[i++ % conf.test.capture_duration_secs] = subtract_timespec(end_time, start_time);
        }

        if (true)
        {
            std::unique_lock<std::mutex> lock(*ctx->infer_lock);

            ctx->buf_index_to_infer = idx_to_infer;
            ++ctx->uninferred_count;
            ctx->infer_notifier->notify_all();
        }
    } // while (true)

    if (is_test)
    {
        for (i = 0; i < conf.test.capture_duration_secs; ++i)
        {
            LOG_INFO("[%d] Cost time of resizing image: %ld.%09ld s",
                i, cost_times[i].tv_sec, cost_times[i].tv_nsec);
        }
    }
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

