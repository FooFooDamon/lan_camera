// SPDX-License-Identifier: Apache-2.0

/*
 * Biz executors of saving video and audio files.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include <signal.h>
#include <sys/stat.h> // for mkdir()

#include "signal_handling.h"

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio/videoio.hpp>

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"

static const cv::String S_FONT_EXAMPLE = "2026-04-01 00:00:00.123";
static const int S_FONT_TYPE = cv::FONT_HERSHEY_DUPLEX;
static const double S_FONT_SCALE = 0.5;
static const cv::Scalar S_FONT_COLOR = cv::Scalar(0, 0, 255);
static const int S_FONT_THICKNESS = 1;
static thread_local struct tm s_now = {};
static thread_local char s_img_timestamp[32] = {};

static inline void add_timestamp(const struct timespec &timestamp, cv::Mat &mat)
{
    static cv::Size s_text_size = cv::getTextSize(S_FONT_EXAMPLE,
        S_FONT_TYPE, S_FONT_SCALE * mat.cols / 640, S_FONT_THICKNESS, nullptr);
    static cv::Point s_text_origin_point(0, s_text_size.height * mat.cols / 640);
    int ret; // For eliminating -Wformat-truncation warning.

    localtime_r(&timestamp.tv_sec, &s_now);
    ret = snprintf(s_img_timestamp, sizeof(s_img_timestamp) - 1, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
        s_now.tm_year + 1900, s_now.tm_mon + 1, s_now.tm_mday,
        s_now.tm_hour, s_now.tm_min, s_now.tm_sec, timestamp.tv_nsec / 1000000);
    if (ret > 0 && ret < (int)sizeof(s_img_timestamp) - 1)
    {
        cv::putText(mat, s_img_timestamp, s_text_origin_point, S_FONT_TYPE, S_FONT_SCALE * mat.cols / 640,
            S_FONT_COLOR, S_FONT_THICKNESS, cv::LINE_AA);
    }
}

static inline bool make_multilevel_directory(const char *dir, size_t len)
{
    char *path = const_cast<char *>(dir);

    for (size_t i = 1; i <= len; ++i)
    {
        if ('/' != path[i])
            continue;

        path[i] = '\0';

        if (access(path, F_OK) < 0 && mkdir(path, 0755) < 0)
        {
            LOG_ERROR("**** %s: Failed to create directory: %s", path, strerror(errno));
            path[i] = '/';

            return false;
        }

        path[i] = '/';
    }

    return true;
}

static inline void rename_video_file(uint32_t total_frame_count, float fps, const std::pair<uint16_t, uint16_t> &size,
    const char *suffix, std::string &path)
{
    static thread_local std::string s_old_path(path.length(), '\0');
    static thread_local char s_video_desc[NAME_MAX + 1] = { 0 };
    uint32_t total_secs = 1.0f / fps * total_frame_count;
    uint32_t total_hours = total_secs / 3600;
    uint32_t total_minutes = (total_secs % 3600) / 60;

    s_old_path = path;
    path = s_old_path.substr(0, s_old_path.length() - strlen(suffix));
    snprintf(s_video_desc, sizeof(s_video_desc) - 1, "_%02dh%02dm%02ds_%dx%d_%.0ffps%s",
        total_hours, total_minutes, total_secs % 60, size.first, size.second, fps, suffix);
    path += s_video_desc;
    if (rename(s_old_path.c_str(), path.c_str()) < 0)
    {
        int err = errno;

        LOG_ERROR("*** Failed to rename/link %s to %s: err = %d, desc = %s",
            s_old_path.c_str(), path.c_str(), err, strerror(err));
    }
}

#define DISPLAY_RESIZED_IMAGES                  0
//#define DISPLAY_RESIZED_IMAGES                1

__attribute__((weak))
void biz_video_save(biz_context_t *ctx, int index)
{
    char thread_name[16] = { 0 };
    const int NEIGHBOR_INDEX = (index + 1) % 2;
    bool is_test = ("test" == ctx->cmd_args->biz);
    const auto &conf = *ctx->conf;
    const auto &model = conf.inference.model;
    const auto &size = DISPLAY_RESIZED_IMAGES ? std::pair<uint16_t, uint16_t>(model.width, model.height)
        : conf.camera.image_sizes[conf.camera.which_size - 1];
    std::string root_dir = (/* FIXME: enabled later: conf.save.ramfs.enabled ? conf.save.ramfs.path : */conf.save.dir)
        + ((ROLE_SERVER == conf.role.type) ? "/server" : (conf.save.enabled ? "/client" : "/server"));
    std::string path;
    char date_dir[16] = { 0 }; // format: /%04d/%02d/%02d/%02d
    char video_file[16] = { 0 }; // format: /%02d%02d.mp4
    cv::VideoWriter writer;
    const char *suffix = ".mp4";
    int fourcc = cv::VideoWriter::fourcc('H'/*'X'*/, '2', '6', '4'/*'m', 'p', '4', 'v'*/);
    std::string fourcc_str(reinterpret_cast<char *>(&fourcc), 4);
    float fps = -1.0;
    bool is_color_img = (0 != strcasecmp(conf.camera.capture_format.c_str(), "GREY")
        && 0 != strcasecmp(conf.camera.capture_format.c_str(), "GRAY"));
    std::vector<cv::Mat> &matrixes = DISPLAY_RESIZED_IMAGES ? ctx->resized_matrixes : ctx->rgb_matrixes;
    uint32_t total_frame_count = 0;
    uint32_t total_dropped_count = 0;
    struct timespec start_time;
    struct timespec end_time;
    std::vector<struct timespec> cost_times;
    struct timespec cur_time;
    struct tm now;
    bool last_save_flag = false;
    bool cur_save_flag;
    uint8_t buf_idx = 0xff;
    int i = 0;

    snprintf(thread_name, sizeof(thread_name) - 1, "lanc/save:v:%d", (index % 2) + 1);
    SET_THREAD_NAME(thread_name);

    if (is_test)
        cost_times.resize(conf.test.capture_duration_secs, {});

    while (true)
    {
        if (sig_check_critical_flag())
            break;

        if (ctx->unsaved_count < 1)
        {
            std::unique_lock<std::mutex> lock(*ctx->capture_lock);

            ctx->capture_notifier->wait_for(lock, std::chrono::milliseconds(BIZ_POLL_TIMEOUT_MSECS));
        }

        if (ctx->unsaved_count < 1/* in case of spurious wake-up */ || index != ctx->saver_index/* not my task */)
            continue;

        if (--ctx->unsaved_count > 0)
        {
            if (ctx->should_save)
                ++ctx->skipped_saving_count;

            continue;
        }

        if (!conf.save.enabled)
            continue;

        if (false == (cur_save_flag = ctx->should_save) && cur_save_flag == last_save_flag)
            continue;

        last_save_flag = cur_save_flag;
        buf_idx = ctx->buf_index;

        if (is_test)
            clock_gettime(CLOCK_MONOTONIC, &start_time);

        if (false == cur_save_flag)
        {
            ctx->saver_index = NEIGHBOR_INDEX; // Next video processing will be assigned to the other thread.
            add_timestamp(ctx->timestamps[buf_idx], matrixes[buf_idx]);
            writer.write(matrixes[buf_idx]);
            writer.release();
            ++total_frame_count;
            rename_video_file(total_frame_count, fps, size, suffix, path);
            total_frame_count = 0;
            LOG_NOTICE("Finished making and renaming/linking video[%s] with %u frames dropped",
                path.c_str(), ctx->skipped_saving_count - total_dropped_count);
            total_dropped_count = ctx->skipped_saving_count;
        }
        else
        {
            if (!writer.isOpened())
            {
                clock_gettime(CLOCK_REALTIME, &cur_time);
                localtime_r(&cur_time.tv_sec, &now);
                path = root_dir;
                snprintf(date_dir, sizeof(date_dir) - 1, "/%04d/%02d/%02d/%02d",
                    now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour);
                path += date_dir;
                if (!make_multilevel_directory(path.c_str(), path.size()))
                {
                    LOG_ERROR("*** Failed to create video directory: %s", path.c_str());
                    raise(SIGABRT);

                    break;
                }
                snprintf(video_file, sizeof(video_file) - 1, "/%02d%02d%s", now.tm_min, now.tm_sec, suffix);
                path += video_file;
                if (fps < 0.0)
                {
                    fps = atof(ctx->cmd_args->fps.c_str());
                }
                if (!writer.open(path, fourcc, fps, cv::Size(size.first, size.second), is_color_img))
                {
                    LOG_ERROR("*** Failed to open video writer for %s", path.c_str());
                    raise(SIGABRT);

                    break;
                }
                LOG_NOTICE("Making video: Path = %s, FourCC = %s, FPS = %.1f",
                    path.c_str(), fourcc_str.c_str(), fps);
            } // if (!writer.isOpened())

            add_timestamp(ctx->timestamps[buf_idx], matrixes[buf_idx]);
            writer.write(matrixes[buf_idx]);
            ++total_frame_count;
        }

        if (is_test)
        {
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            cost_times[i++ % conf.test.capture_duration_secs] = subtract_timespec(end_time, start_time);
        }
    } // while (true)

    if (writer.isOpened())
    {
        writer.release();
        rename_video_file(total_frame_count, fps, size, suffix, path);
        LOG_NOTICE("Finished making and renaming/linking video[%s] with %u frames dropped",
            path.c_str(), ctx->skipped_saving_count - total_dropped_count);
    }

    if (is_test)
    {
        for (i = 0; i < conf.test.capture_duration_secs; ++i)
        {
            LOG_INFO("[%d] Cost time of saving video: %ld.%09ld s",
                i, cost_times[i].tv_sec, cost_times[i].tv_nsec);
        }
    }
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-07, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

