/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Thread settings.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#ifndef __THREAD_SETTINGS_HPP__
#define __THREAD_SETTINGS_HPP__

#include <stdint.h>

#include <vector>

struct conf_file;

bool is_root_thread(void);

void init_cv_backend_thread_pool(int nthreads);

int set_cpu_affinity_for_self(const std::vector<int16_t> &cpu_ids);

void do_biz_thread_settings(const struct conf_file &conf, const char *thread_name);

#define DO_ROOT_THREAD_SETTINGS(_conf_)             init_cv_backend_thread_pool((_conf_).schedule.cv_backend_thread_count)

#define DO_BIZ_THREAD_SETTINGS(_conf_, _name_)      do_biz_thread_settings(_conf_, _name_)

#endif /* #ifndef __THREAD_SETTINGS_HPP__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-07-01, Man Hung-Coeng <udc577@126.com>:
 *  01. Initial commit.
 */

