// SPDX-License-Identifier: Apache-2.0

/*
 * Biz executor of rendering real-time images in GUI window.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include <signal.h>

#include "signal_handling.h"

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"
#include "QLANCamera.hpp"

static inline float calculate_fps(const struct timespec &frame_interval)
{
    return 1.0f / (frame_interval.tv_sec + frame_interval.tv_nsec / 1000000000.0);
}

static inline QString make_realtime_info(const biz_context_t *ctx,
    const struct timespec &cur_time, const struct timespec &frame_interval)
{
    static struct tm s_now;
    const int MAX_STAT_STRLEN = 63;
    static char s_infer_stat[MAX_STAT_STRLEN + 1] = {};
    static char s_save_stat[MAX_STAT_STRLEN + 1] = {};
    static char s_send_stat[MAX_STAT_STRLEN + 1] = {};
    static struct
    {
        const char *name;
        uint32_t last_value;
        const std::atomic<uint32_t> &cur_value;
        char *stat_str;
    } s_stat_items[] = {
        { "inference", ctx->skipped_inference_count, ctx->skipped_inference_count, s_infer_stat },
        { "saving", ctx->skipped_saving_count, ctx->skipped_saving_count, s_save_stat },
        { "sending", ctx->skipped_sending_count, ctx->skipped_sending_count, s_send_stat },
    };

    localtime_r(&cur_time.tv_sec, &s_now);

    for (auto &item : s_stat_items)
    {
        if (0 == item.cur_value)
        {
            item.stat_str[0] = '\0';

            continue;
        }

        if (item.cur_value != item.last_value || '\0' == item.stat_str[0])
        {
            item.last_value = item.cur_value;
            snprintf(item.stat_str, MAX_STAT_STRLEN, "\n%u frames dropped during %s",
                (uint32_t)item.cur_value, item.name);
        }
    }

    return QString::asprintf("%04d-%02d-%02d %02d:%02d:%02d.%03ld | %.1f fps%s%s%s",
        s_now.tm_year + 1900, s_now.tm_mon + 1, s_now.tm_mday, s_now.tm_hour, s_now.tm_min, s_now.tm_sec,
        cur_time.tv_nsec / 1000000, calculate_fps(frame_interval), s_infer_stat, s_save_stat, s_send_stat);
}

__attribute__((weak))
void biz_render(biz_context_t *ctx, int index)
{
    SET_THREAD_NAME("lanc/render");

    struct timespec start_time;
    struct timespec end_time;
    std::vector<struct timespec> cost_times;
    struct timespec last_frame_time;
    struct timespec frame_interval;
    struct timespec cur_time;
    uint8_t last_buf_idx = 0xff;
    uint8_t cur_buf_idx = 0xff;
    int stat_samples = atoi(QString::fromLocal8Bit(qgetenv("LANC_RENDER_STAT_SAMPLES")).toStdString().c_str());
    int i = 0;

    if (stat_samples > 0)
        cost_times.resize(stat_samples % (32 + 1), {});

    clock_gettime(CLOCK_MONOTONIC, &last_frame_time);

    while (true)
    {
        if (sig_check_critical_flag())
            break;

        if (true)
        {
            std::unique_lock<std::mutex> lock(*ctx->capture_lock);

            ctx->capture_notifier->wait_for(lock, std::chrono::milliseconds(BIZ_POLL_TIMEOUT_MSECS));
        }

        if ((cur_buf_idx = ctx->buf_index_of_latest_frame) == last_buf_idx)
            continue;
        else
            last_buf_idx = cur_buf_idx;

        if (stat_samples > 0)
            clock_gettime(CLOCK_MONOTONIC, &start_time);

        ctx->widget->glwdtCanvas->render(cur_buf_idx);

        clock_gettime(CLOCK_MONOTONIC, &end_time);
        frame_interval = subtract_timespec(end_time, last_frame_time);
        last_frame_time = end_time;
        if (stat_samples > 0)
            cost_times[i++ % stat_samples] = subtract_timespec(end_time, start_time);

        cur_time = ctx->timestamps[cur_buf_idx];
        ctx->widget->lblRealtimeInfo->setText(make_realtime_info(ctx, cur_time, frame_interval));
    } // while (true)

    ctx->widget->delegatingClose();

    if (stat_samples > 0)
    {
        for (i = 0; i < stat_samples; ++i)
        {
            LOG_INFO("[%d] Cost time of rendering: %ld.%09ld s",
                i, cost_times[i].tv_sec, cost_times[i].tv_nsec);
        }
    }

    if (!sig_check_critical_flag())
        raise(SIGTERM); // Notify other threads to terminate.
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-13, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

