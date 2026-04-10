/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Common biz declarations and definitions.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#ifndef __BIZ_COMMON_HPP__
#define __BIZ_COMMON_HPP__

#include <memory>
#include <vector>
#include <atomic>
#include <condition_variable>

namespace cv
{

class Mat;

}

struct cmd_args;
struct conf_file;
struct private_context;
class QApplication;
class QLANCamera;

typedef struct biz_context
{
    int argc;
    char **argv;
    struct cmd_args *cmd_args;
    struct conf_file *conf;
    std::shared_ptr<QApplication> app;
    std::shared_ptr<QLANCamera> widget;
    // ----
    std::vector<struct timespec> timestamps;
    volatile uint32_t raw_buf_size;
    std::vector<unsigned char *> raw_buffers;
    std::vector<std::vector<unsigned char>> rgb_buffers;
    std::vector<std::vector<unsigned char>> resized_buffers;
    std::vector<cv::Mat> rgb_matrixes; // may be BGR order
    std::vector<cv::Mat> resized_matrixes;
    // ----
    std::shared_ptr<std::mutex> capture_lock;
    std::shared_ptr<std::condition_variable> capture_notifier;
    std::shared_ptr<std::mutex> infer_lock;
    std::shared_ptr<std::condition_variable> infer_notifier;
    std::shared_ptr<std::mutex> flush_locks[2];
    std::shared_ptr<std::condition_variable> flush_notifiers[2];
    struct
    {
        volatile int64_t timestamp;
        volatile int32_t duration;
        volatile int32_t should_delete:1;
        volatile int32_t unused:31;
    } unflushed_files[2]; // [0]: current file, [1]: previous file
    std::shared_ptr<std::mutex> unflushed_info_lock;
    // ----
    volatile int64_t startup_time_secs;
    std::atomic_uint_fast64_t frame_seq;
    std::atomic<uint64_t> total_saving_count;
    std::atomic<uint64_t> total_sending_count;
    std::atomic<uint64_t> total_inference_count;
    std::atomic<uint32_t> skipped_saving_count;
    std::atomic<uint32_t> skipped_sending_count;
    std::atomic<uint32_t> skipped_inference_count;
    // ----
    std::atomic_uint_fast8_t buf_index_of_latest_frame;
    std::atomic_int_fast8_t buf_index_to_infer;
    std::atomic_uint_fast8_t saver_index; // [0, (<count-of-save-threads> - 1)]
    std::atomic_uint_fast8_t unflushed_count; // should be <= 1
    std::atomic_uint_fast8_t unsaved_count;
    std::atomic_uint_fast8_t unsent_count;
    std::atomic_uint_fast8_t uninferred_count;
    // ----
    std::atomic_bool should_save;
    std::atomic_bool inference_positive; // whether the latest inference result is positive or negative
    std::atomic_bool inference_paused; // whether the inference thread is running or not
    std::atomic_bool needs_live_stream;
    std::atomic_bool connected_to_server; // for client only
    // ----
    std::shared_ptr<struct private_context> priv;
} biz_context_t;

#define BIZ_FUN_ARG_LIST                biz_context_t &ctx

#define DECLARE_BIZ_FUN(name)           int name(BIZ_FUN_ARG_LIST)
#define BIZ_FUN(name)                   name

typedef int (*biz_func_t)(BIZ_FUN_ARG_LIST);

#ifndef TODO
#define TODO()                          LOG_NOTICE("TODO ...")
#endif

#ifndef BIZ_POLL_TIMEOUT_MSECS
#define BIZ_POLL_TIMEOUT_MSECS          500
#endif

inline struct timespec subtract_timespec(const struct timespec &l, const struct timespec &r)
{
    struct timespec result;

    result.tv_sec = l.tv_sec - r.tv_sec - ((l.tv_nsec >= r.tv_nsec) ? 0 : 1);
    result.tv_nsec = l.tv_nsec + ((l.tv_nsec >= r.tv_nsec) ? 0 : 1000000000) - r.tv_nsec;

    return result;
}

#endif /* #ifndef __BIZ_COMMON_HPP__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-06, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 *
 * >>> 2026-04-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Include the header file of condition_variable instead of
 *      using forward declaration.
 *  02. Update several fields of biz_context_t:
 *      (1) Rename buf_index to buf_index_of_latest_frame,
 *          infer_index to buf_index_to_infer;
 *      (2) Delete the unused time_of_1st_positive_inference;
 *      (3) Add volatile modifier to raw_buf_size, startup_time_secs
 *          and fields within struct type of unflushed_files.
 *  03. Remove the index parameter from macro BIZ_FUN_ARG_LIST.
 */

