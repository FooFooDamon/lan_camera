/* SPDX-License-Identifier: Apache-2.0 */

/*
 * APIs for parsing and validating command-line arguments.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#ifndef __CMDLINE_ARGS_HPP__
#define __CMDLINE_ARGS_HPP__

#include <string>
#include <vector>

typedef struct cmd_args
{
    std::vector<std::string> orphan_args;
    std::string biz;
    std::string config_file;
#ifdef HAS_LOGGER
    std::string log_file;
    std::string log_level;
#else
    bool verbose;
    bool debug;
#endif
    std::string play_dir;
    std::string fps;
    int which_size;
    bool show_sizes;
} cmd_args_t;

cmd_args_t parse_cmdline(int argc, char **argv);

void assert_parsed_args(const cmd_args_t &args);

#endif /* #ifndef __CMDLINE_ARGS_HPP__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 */

