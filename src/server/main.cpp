// SPDX-License-Identifier: Apache-2.0

/*
 * Entry point of server program.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#ifdef NEED_OS_SIGNALS
#include <signal.h>
#include "signal_handling.h"
#endif
#include "communication_protocol.h"

#include <thread>
#include <opencv2/core/mat.hpp>

#include "fmt_log.hpp"
#include "cmdline_args.hpp"
#include "config_file.hpp"
#include "biz_common.hpp"

static int show_available_capture_sizes(const conf_file_t &conf)
{
    const auto &sizes = conf.camera.image_sizes;

    for (unsigned int i = 0; i < sizes.size(); ++i)
    {
        printf("%u. %dx%d\n", i + 1, sizes[i].first, sizes[i].second);
    }

    return 0;
}

log_filter_t g_log_filter = { LOG_LEVEL_INFO };

__attribute__((weak))
int logger_init(const cmd_args_t &args, const conf_file_t &conf)
{
#ifdef HAS_LOGGER
    std::string log_level = ("config" == args.log_level) ? conf.logger.level : args.log_level;

    g_log_filter.log_level = to_log_level(log_level.c_str());
#endif
    return 0;
}

__attribute__((weak))
void logger_finalize(void)
{
#ifdef HAS_LOGGER
    ; // do nothing
#endif
}

#ifdef NEED_OS_SIGNALS
static const char *S_CRITICAL_SIGNALS[] = { "INT", "ABRT", "TERM", "QUIT", "BUS", "PWR", NULL };
static const char *S_PLAIN_SIGNALS[] = { "CHLD", "PIPE", "USR1", "USR2", NULL };
#endif

static int register_signals(const cmd_args_t &args, const conf_file_t &conf)
{
#ifdef NEED_OS_SIGNALS
    int err = sig_simple_register(S_CRITICAL_SIGNALS, S_PLAIN_SIGNALS);

    if (err < 0)
    {
        LOG_ERROR("**** Failed to register OS signals: %s", sig_error(err));

        return EXIT_FAILURE;
    }
#endif
    return 0;
}

static void print_received_signals(void)
{
#ifdef NEED_OS_SIGNALS
    const char **crit_sig;
    const char **plain_sig;

    for (crit_sig = S_CRITICAL_SIGNALS; NULL != *crit_sig; ++crit_sig)
    {
        int signum = sig_name_to_number(*crit_sig, strlen(*crit_sig));

        if (sig_has_happened(signum))
            LOG_WARNING("* Received SIG%s during program execution.", *crit_sig);
    }

    for (plain_sig = S_PLAIN_SIGNALS; NULL != *plain_sig; ++plain_sig)
    {
        int signum = sig_name_to_number(*plain_sig, strlen(*plain_sig));

        if (sig_has_happened(signum))
            LOG_WARNING("* Received SIG%s during program execution.", *plain_sig);
    }
#endif
}

__attribute__((weak))
int construct_private_context(biz_context_t &ctx)
{
    ctx.priv = std::shared_ptr<struct private_context>(nullptr);

    return 0;
}

static int biz_context_construct(int argc, char **argv, cmd_args_t &parsed_args, conf_file_t &conf, biz_context_t &ctx)
{
    const auto &sizes = conf.camera.image_sizes;
    int which = (parsed_args.which_size <= 0)
        ? std::min(std::max(1, (int)conf.camera.which_size), (int)sizes.size())
        : std::min(std::max(1, parsed_args.which_size), (int)sizes.size());
    int width = sizes[which - 1].first;
    int height = sizes[which - 1].second;

    conf.camera.which_size = which;

    ctx.argc = argc;
    ctx.argv = argv;
    ctx.cmd_args = &parsed_args;
    ctx.conf = &conf;
    // ----
    ctx.timestamps.resize(conf.camera.buffer_count, {});
    ctx.raw_buf_size = 0;
    ctx.rgb_buffers.resize(conf.camera.buffer_count);
    ctx.resized_buffers.resize(conf.camera.buffer_count);
    ctx.rgb_matrixes.reserve(conf.camera.buffer_count);
    ctx.resized_matrixes.reserve(conf.camera.buffer_count);
    for (uint8_t i = 0; i < conf.camera.buffer_count; ++i)
    {
        ctx.rgb_buffers[i].resize(width * height * 3);
        ctx.resized_buffers[i].resize(conf.inference.model.width * conf.inference.model.height * 3, 0);

        ctx.rgb_matrixes.push_back(cv::Mat(height, width, CV_8UC3, ctx.rgb_buffers[i].data()));
        ctx.resized_matrixes.push_back(cv::Mat(conf.inference.model.height, conf.inference.model.width,
            CV_8UC3, ctx.resized_buffers[i].data()));
    }
    // ----
    ctx.capture_lock = std::make_shared<std::mutex>();
    ctx.capture_notifier = std::make_shared<std::condition_variable>();
    ctx.infer_lock = std::make_shared<std::mutex>();
    ctx.infer_notifier = std::make_shared<std::condition_variable>();
    for (size_t i = 0; i < sizeof(ctx.flush_locks) / sizeof(ctx.flush_locks[0]); ++i)
    {
        ctx.flush_locks[i] = std::make_shared<std::mutex>();
        ctx.flush_notifiers[i]= std::make_shared<std::condition_variable>();
    }
    memset(&ctx.unflushed_files, 0, sizeof(ctx.unflushed_files));
    ctx.unflushed_info_lock = std::make_shared<std::mutex>();
    // ----
    ctx.startup_time_secs = 0;
    ctx.frame_seq = 0;
    ctx.total_saving_count = 0;
    ctx.total_sending_count = 0;
    ctx.total_inference_count = 0;
    ctx.skipped_saving_count = 0;
    ctx.skipped_sending_count = 0;
    ctx.skipped_inference_count = 0;
    // ----
    ctx.buf_index_of_latest_frame = 0;
    ctx.buf_index_to_infer = -1;
    ctx.saver_index = 0;
    ctx.unflushed_count = 0;
    ctx.unsaved_count = 0;
    ctx.unsent_count = 0;
    ctx.uninferred_count = 0;
    // ----
    ctx.should_save = ctx.inference_positive = ("test" == ctx.cmd_args->biz);
    ctx.inference_paused = false;
    ctx.needs_live_stream = ("test" == ctx.cmd_args->biz);

    return construct_private_context(ctx);
}

__attribute__((weak))
void destruct_private_context(biz_context_t &ctx)
{
    ; // do nothing
}

static void biz_context_destruct(biz_context_t &ctx)
{
    destruct_private_context(ctx);
}

extern void biz_listen(biz_context_t *ctx, int index);
extern void biz_capture_image(biz_context_t *ctx, int index);
//extern void biz_capture_audio(biz_context_t *ctx, int index);
extern void biz_resize(biz_context_t *ctx, int index);
extern void biz_infer(biz_context_t *ctx, int index);
extern void biz_save_video(biz_context_t *ctx, int index);
//extern void biz_save_audio(biz_context_t *ctx, int index);
//extern void biz_flush(biz_context_t *ctx, int index);
extern void biz_send_image(biz_context_t *ctx, int index);
//extern void biz_send_audio(biz_context_t *ctx, int index);

typedef void (*biz_exec_func_t)(biz_context_t *, int);

static DECLARE_BIZ_FUN(server_biz)
{
    if (ROLE_SERVER != ctx.conf->role.type)
    {
        LOG_ERROR("*** %s", "This is not a server configuration!");

        return -EINVAL;
    }

    int proto_ret = commproto_init();

    if (proto_ret < 0)
    {
        LOG_ERROR("*** Communication facility initialization failed: %s!", commproto_error(proto_ret));

        return -1;
    }

    bool is_test = ("test" == ctx.cmd_args->biz);
#if 1
    std::vector<std::thread> biz_threads;
    struct
    {
        biz_exec_func_t func;
        int index;
    } biz_executors[] = {
        { biz_send_image, 0 },
        //{ biz_send_audio, 0 },
        //{ biz_flush, 0 },
        //{ biz_flush, 1 },
        { biz_save_video, 0 },
        { biz_save_video, 1 },
        //{ biz_save_audio, 0 },
        //{ biz_save_audio, 1 },
        { biz_infer, 0 },
        { biz_resize, 0 },
        { biz_capture_image, 0 },
        //{ biz_capture_audio, 0 },
        { biz_listen, 0 },
    };

    //biz_threads.reserve(sizeof(biz_executors) / sizeof(biz_executors[0])); // no help for preventing crash
    for (size_t i = 0; i < sizeof(biz_executors) / sizeof(biz_executors[0]); ++i)
    {
        if (biz_executors[i].index > 0 && is_test)
            continue;

        biz_threads.push_back(std::thread([&](){
            biz_executors[i].func(&ctx, biz_executors[i].index);
        }));
        usleep(1000); // FIXME: Will crash without this delay, why?!
    }
#else // FIXME: But delay is not needed when threads are made one by one, why?!
    std::thread send_image_thread([&](){
        biz_send_image(&ctx, 0);
    });
    //std::thread send_audio_thread([&](){
    //    biz_send_audio(&ctx, 0);
    //});

    //std::thread flush_thread1([&](){
    //    biz_flush(&ctx, 0);
    //});
    //std::thread flush_thread2 = is_test ? std::thread()
    //    : std::thread([&]() { biz_flush(&ctx, 1); }); 
    std::thread save_video_thread1([&](){
        biz_save_video(&ctx, 0);
    });
    std::thread save_video_thread2 = is_test ? std::thread()
        : std::thread([&]() { biz_save_video(&ctx, 1); }); 
    //std::thread save_audio_thread1([&](){
    //    biz_save_audio(&ctx, 0);
    //});
    //std::thread save_audio_thread2 = is_test ? std::thread()
    //    : std::thread([&]() { biz_save_audio(&ctx, 1); }); 

    std::thread infer_thread([&](){
        biz_infer(&ctx, 0);
    });

    std::thread resize_thread([&](){
        biz_resize(&ctx, 0);
    });

    std::thread capture_image_thread([&](){
        biz_capture_image(&ctx, 0);
    });
    //std::thread capture_audio_thread([&](){
    //    biz_capture_audio(&ctx, 0);
    //});

    std::thread listen_thread([&](){
        biz_listen(&ctx, 0);
    });
#endif

#if 1
    for (int i = biz_threads.size() - 1; i >= 0; --i)
    {
        biz_threads[i].join();
    }
#else
    listen_thread.join();
    //capture_audio_thread.join();
    capture_image_thread.join();
    resize_thread.join();
    infer_thread.join();
    //if (!is_test)
    //    save_audio_thread2.join();
    //save_audio_thread1.join();
    if (!is_test)
        save_video_thread2.join();
    save_video_thread1.join();
    //if (!is_test)
    //    flush_thread2.join();
    //flush_thread1.join();
    //send_audio_thread.join();
    send_image_thread.join();
#endif

    print_received_signals();

    return sig_has_happened(SIGTERM) ? EXIT_SUCCESS : EXIT_FAILURE;
}

DEFINE_THREAD_NAME_VAR();

int main(int argc, char **argv)
{
    SET_THREAD_NAME("lanc_server");

    cmd_args_t parsed_args = parse_cmdline(argc, argv);
    conf_file_t conf;
    biz_context_t ctx;
    std::map<std::string, biz_func_t> biz_handlers = {
        { "server", BIZ_FUN(server_biz) },
        { "test", BIZ_FUN(server_biz) },
    };
    biz_func_t biz_func = nullptr;
    int ret;

    assert_parsed_args(parsed_args);

    if (nullptr == (biz_func = biz_handlers[parsed_args.biz]))
    {
        fprintf(stderr, "*** Biz[%s] is not supported yet!\n", parsed_args.biz.c_str());

        return ENOTSUP;
    }

    if ((ret = load_config_file(parsed_args.config_file.c_str(), conf, !parsed_args.show_sizes)) < 0)
        return -ret;

    if (parsed_args.show_sizes)
    {
        ret = show_available_capture_sizes(conf);

        goto lbl_unload_conf;
    }

    if ((ret = logger_init(parsed_args, conf)) < 0)
        goto lbl_unload_conf;

    if ((ret = register_signals(parsed_args, conf)) < 0)
        goto lbl_finalize_log;

    if ((ret = biz_context_construct(argc, argv, parsed_args, conf, ctx)) >= 0)
        ret = biz_func(ctx);

    biz_context_destruct(ctx);

lbl_finalize_log:
    logger_finalize();

lbl_unload_conf:
    unload_config_file(conf);

    return abs(ret);
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

