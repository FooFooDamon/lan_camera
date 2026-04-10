// SPDX-License-Identifier: Apache-2.0

/*
 * Biz executor of inference.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include <string.h>
#include <signal.h>
#include <math.h>

#include "signal_handling.h"

#include <set>
#include <fstream>
//#include <opencv2/imgproc.hpp>

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"

struct inference_resources;
typedef std::shared_ptr<struct inference_resources> inference_resources_ptr_t;

__attribute__((weak))
inference_resources_ptr_t prepare_inference_resources(const conf_file_t &conf)
{
    return inference_resources_ptr_t(nullptr);
}

__attribute__((weak))
void release_inference_resources(inference_resources_ptr_t &res)
{
    // empty
}

__attribute__((weak))
int do_inference(biz_context_t *ctx, inference_resources_ptr_t &res, int img_buf_index, bool &hit)
{
    hit = ("test" == ctx->cmd_args->biz); // false

    return 0;
}

__attribute__((weak))
void biz_infer(biz_context_t *ctx, int index)
{
    SET_THREAD_NAME("lanc/infer");

    bool is_test = ("test" == ctx->cmd_args->biz);
    const auto &conf = *ctx->conf;
    struct timespec start_time;
    struct timespec end_time;
    std::vector<struct timespec> cost_times;
    std::shared_ptr<struct inference_resources> infer_res = prepare_inference_resources(conf);
    int resume_countdown = 0;
    bool last_infer_result = ctx->inference_positive;
    bool cur_infer_result;
    int8_t buf_idx_to_infer;
    int i = 0;

    if (!infer_res && conf.inference.enabled)
    {
        raise(SIGABRT);

        return;
    }

    if (is_test)
        cost_times.resize(conf.test.capture_duration_secs, {});

    while (true)
    {
        if (sig_check_critical_flag())
            break;

        if (ctx->uninferred_count < 1)
        {
            std::unique_lock<std::mutex> lock(*ctx->infer_lock);

            ctx->infer_notifier->wait_for(lock, std::chrono::milliseconds(BIZ_POLL_TIMEOUT_MSECS));
        }

        if (ctx->uninferred_count < 1)
            continue;

        if (--ctx->uninferred_count > 0)
        {
            ++ctx->skipped_inference_count;

            continue;
        }

        if (resume_countdown > 0)
        {
            if (--resume_countdown > 0)
                continue;
            else
                ctx->inference_paused = false; // Resize thread becomes busy again.
        }

        if ((buf_idx_to_infer = ctx->buf_index_to_infer) < 0)
            continue;

        if (is_test)
            clock_gettime(CLOCK_MONOTONIC, &start_time);

        if (do_inference(ctx, infer_res, buf_idx_to_infer, cur_infer_result) < 0)
        {
            raise(SIGABRT);

            break;
        }

        if (is_test)
        {
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            cost_times[i++ % conf.test.capture_duration_secs] = subtract_timespec(end_time, start_time);
        }

        if (true == last_infer_result && false == cur_infer_result)
        {
            last_infer_result = false;
            ctx->should_save = ctx->inference_positive = true; // to keep the save thread continuing to save frames
            ctx->inference_paused = true; // to notify the resize thread to take a rest for a while
            resume_countdown = atof(ctx->cmd_args->fps.c_str()) * conf.save.tail_waiting_secs;

            continue;
        }

        ctx->should_save = ctx->inference_positive = cur_infer_result;
        last_infer_result = cur_infer_result;
    } // while (true)

    if (is_test)
    {
        for (i = 0; i < conf.test.capture_duration_secs; ++i)
        {
            LOG_INFO("[%d] Cost time of inference: %ld.%09ld s", i, cost_times[i].tv_sec, cost_times[i].tv_nsec);
        }
    }

    release_inference_resources(infer_res);
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

