/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 *
 * >>> V0.1.0|2026-04-13, Man Hung-Coeng <udc577@126.com>:
 *  01. Implement image receiving, video saving/playing functionalities.
 *
 * >>> V0.1.1|2026-04-19, Man Hung-Coeng <udc577@126.com>:
 *  01. Fix the customization bug.
 *
 * >>> V0.1.2|2026-04-22, Man Hung-Coeng <udc577@126.com>:
 *  01. Add copyright info tab.
 *  02. Adjust several default values of configuration.
 *  03. Optimize deployment with version control.
 *
 * >>> V0.1.3|2026-04-28, Man Hung-Coeng <udc577@126.com>:
 *  01. Optimize version control of communication protocol.
 *  02. Optimize program size of release mode.
 *
 * >>> V0.1.4|2026-05-25, Man Hung-Coeng <udc577@126.com>:
 *  01. Optimize image saving logic by adding control for dual saver threads
 *      and improving skipping logic for unhandled frames.
 *
 * >>> V0.1.5|2026-06-19, Man Hung-Coeng <udc577@126.com>:
 *  01. Add Status tab.
 *  02. Optimize the request for live stream.
 *  03. Optimize the real-time image rendering frequency.
 *  04. Eliminate the H264 codec warning.
 *  05. Print version info on program startup.
 *
 * >>> V0.1.6|2026-07-01, Man Hung-Coeng <udc577@126.com>:
 *  01. Add initialization for CV backend thread pool for main thread.
 *  02. Add support for setting nice level and CPU affinity for biz threads.
 *  03. Fix the bug of Status tab unable to update the uptime of rebooted server
 *      during runtime.
 */

#ifndef __VERSIONS_H__
#define __VERSIONS_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAJOR_VER
#define MAJOR_VER                       0
#endif

#ifndef MINOR_VER
#define MINOR_VER                       1
#endif

#ifndef PATCH_VER
#define PATCH_VER                       6
#endif

#ifndef __REVISION__
#define __REVISION__                    "<none>"
#endif

#ifndef __stringify
#define ___stringify(x)                 #x
#define __stringify(x)                  ___stringify(x)
#endif

#ifndef PRODUCT_VERSION
#define PRODUCT_VERSION                 __stringify(MAJOR_VER) "." __stringify(MINOR_VER) "." __stringify(PATCH_VER)
#endif

#ifndef FULL_VERSION
#define FULL_VERSION()                  (__REVISION__[0] ? (PRODUCT_VERSION "_" __REVISION__) : (PRODUCT_VERSION))
#endif

#ifdef __cplusplus
}
#endif

extern const char *get_private_revision(void);

#endif /* #ifndef __VERSIONS_H__ */

