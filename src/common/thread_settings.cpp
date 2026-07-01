// SPDX-License-Identifier: Apache-2.0

/*
 * Thread settings.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include "thread_settings.hpp"

#include <errno.h>
#include <unistd.h> // For gettid().
#include <dirent.h> // For *dir().
#include <stdio.h>
#include <sched.h>
#include <pthread.h>
#include <sys/resource.h> // For setpriority().

#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>

#include "fmt_log.hpp"
#include "config_file.hpp"

bool is_root_thread(void)
{
    return (getpid() == gettid());
}

__attribute__((weak))
void init_cv_backend_thread_pool(int nthreads)
{
    if (nthreads < 0)
        return;

    std::string old_name = THREAD_NAME();

    SET_THREAD_NAME("lanc/cv_backend"); // a temporary name inherited by CV backend threads

    cv::setNumThreads(nthreads);

    do
    {
        cv::Mat src(960, 1280, CV_8UC3); // Height and width should be big enough so that parallelization is needed!
        cv::Mat dst(src.rows / 2, src.cols / 4, src.type());

        cv::resize(src, dst, dst.size()); // to trigger OpenCV thread pool construction
    }
    while (0);

    SET_THREAD_NAME(old_name.c_str()); // restore the previous name of calling thread
}

int set_cpu_affinity_for_self(const std::vector<int16_t> &cpu_ids)
{
    int count = 0;
    cpu_set_t cpu_list;

    CPU_ZERO(&cpu_list);
    for (int16_t cpu_id : cpu_ids)
    {
        if (cpu_id >= 0)
        {
            CPU_SET(cpu_id, &cpu_list);
            ++count;
        }
    }

    if (0 == count)
        return 0;

    if (cpu_ids.size() > sizeof(cpu_list))
        return -ERANGE;

    int err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_list), &cpu_list);

    return err ? -err : cpu_ids.size();
}

__attribute__((weak))
void do_biz_thread_settings(const struct conf_file &conf, const char *thread_name)
{
    if (!is_root_thread())
        SET_THREAD_NAME(thread_name);

    auto aff_iter = conf.schedule.cpu_affinity.find(thread_name);

    if (conf.schedule.cpu_affinity.end() != aff_iter)
    {
        int err = set_cpu_affinity_for_self(aff_iter->second);

        if (err < 0)
            LOG_ERROR("*** Failed to set CPU affinity for this thread: %d (%s)", err, strerror(-err));
    }

    auto ni_iter = conf.schedule.nice_level.find(thread_name);

    if (conf.schedule.nice_level.end() != ni_iter && setpriority(PRIO_PROCESS, 0, ni_iter->second) < 0)
        LOG_WARNING("*** Failed to set nice level to %d for this thread: %s", ni_iter->second, strerror(errno));
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-07-01, Man Hung-Coeng <udc577@126.com>:
 *  01. Initial commit.
 */

