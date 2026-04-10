/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Formatted logging API wrappers.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#ifndef __FMT_LOG_HPP__
#define __FMT_LOG_HPP__

#include <typeinfo>

#ifndef FMT_LOG_IMPL_H
#define FMT_LOG_IMPL_H      "formatted_logging_adapter.h"
#endif
#define NO_DEFAULT_FMT_LOG_WARNING
#include FMT_LOG_IMPL_H

typedef struct log_filter
{
    unsigned char log_level;
} log_filter_t;

extern log_filter_t g_log_filter;

#define BASE_LOG(_tag_, _fmt_, ...)             FMT_LOG_SIMPLE(&g_log_filter, _tag_, _fmt_, ##__VA_ARGS__)

extern thread_local char __thread_name__[16];

#define DEFINE_THREAD_NAME_VAR()                thread_local char __thread_name__[16] = { 0 }

#define SET_THREAD_NAME(_name_)                 do { \
    if (0 == pthread_setname_np(pthread_self(), _name_)) \
        strncpy(__thread_name__, _name_, sizeof(__thread_name__) - 1); \
} while (0)

#define THREAD_NAME()                           __thread_name__

#undef FMT_LOG
#define FMT_LOG(_filter_, _tag_, _fmt_, ...)    FMT_LOG_SIMPLE(&g_log_filter, _tag_, "(T:%s) " _fmt_, \
                                                    THREAD_NAME(), ##__VA_ARGS__)

#undef FMT_LOG_V
#define FMT_LOG_V(_filter_, _tag_, _fmt_, ...)  FMT_LOG_SIMPLE(&g_log_filter, _tag_, "(T:%s) " __FILE__ ":%d %s(): " _fmt_, \
                                                    THREAD_NAME(), __LINE__, __func__, ##__VA_ARGS__)

// CLS means class.
#define CLS_VLOG(_tag_, _fmt_, ...)             FMT_LOG_SIMPLE(&g_log_filter, _tag_, \
                                                    "(T:%s) " __FILE__ ":%d %s::%s(): " _fmt_, \
                                                    THREAD_NAME(), __LINE__, typeid(*this).name(), __func__, ##__VA_ARGS__)

// NS means namespace.
#define NS_VLOG(_ns_, _tag_, _fmt_, ...)        FMT_LOG_SIMPLE(&g_log_filter, _tag_, \
                                                    "(T:%s) " __FILE__ ":%d " #_ns_ "%s(): " _fmt_, \
                                                    THREAD_NAME(), __LINE__, __func__, ##__VA_ARGS__)

#define LOG_DEBUG(_fmt_, ...)                   FMT_LOG_V(_, D, _fmt_, ##__VA_ARGS__)
#define LOG_INFO(_fmt_, ...)                    FMT_LOG_V(_, I, _fmt_, ##__VA_ARGS__)
#define LOG_NOTICE(_fmt_, ...)                  FMT_LOG_V(_, N, _fmt_, ##__VA_ARGS__)
#define LOG_WARNING(_fmt_, ...)                 FMT_LOG_V(_, W, _fmt_, ##__VA_ARGS__)
#define LOG_ERROR(_fmt_, ...)                   FMT_LOG_V(_, E, _fmt_, ##__VA_ARGS__)

#define LOG_DEBUG_C(_fmt_, ...)                 CLS_VLOG(D, _fmt_, ##__VA_ARGS__)
#define LOG_INFO_C(_fmt_, ...)                  CLS_VLOG(I, _fmt_, ##__VA_ARGS__)
#define LOG_NOTICE_C(_fmt_, ...)                CLS_VLOG(N, _fmt_, ##__VA_ARGS__)
#define LOG_WARNING_C(_fmt_, ...)               CLS_VLOG(W, _fmt_, ##__VA_ARGS__)
#define LOG_ERROR_C(_fmt_, ...)                 CLS_VLOG(E, _fmt_, ##__VA_ARGS__)

#define LOG_DEBUG_NS(_ns_, _fmt_, ...)          NS_VLOG(_ns_, D, _fmt_, ##__VA_ARGS__)
#define LOG_INFO_NS(_ns_, _fmt_, ...)           NS_VLOG(_ns_, I, _fmt_, ##__VA_ARGS__)
#define LOG_NOTICE_NS(_ns_, _fmt_, ...)         NS_VLOG(_ns_, N, _fmt_, ##__VA_ARGS__)
#define LOG_WARNING_NS(_ns_, _fmt_, ...)        NS_VLOG(_ns_, W, _fmt_, ##__VA_ARGS__)
#define LOG_ERROR_NS(_ns_, _fmt_, ...)          NS_VLOG(_ns_, E, _fmt_, ##__VA_ARGS__)

#endif /* #ifndef __FMT_LOG_HPP__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-03-31, Man Hung-Coeng <udc577@126.com>:
 *  01. Initial commit.
 *
 * >>> 2026-04-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Always use g_log_filter regardless of the actual value of _filter_
 *      in FMT_LOG*() re-definitions.
 */

