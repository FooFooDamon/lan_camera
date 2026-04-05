/* SPDX-License-Identifier: Apache-2.0 */

/*
 * APIs for loading and unloading configuration file.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#ifndef __CONFIG_FILE_HPP__
#define __CONFIG_FILE_HPP__

#include <stdint.h>

#include <string>
#include <vector>
#include <map>
#include <memory>

enum role_e
{
    ROLE_UNKNOWN,
    ROLE_SERVER,
    ROLE_CLIENT,
    ROLE_FOR_ALL
};

#define DEFAULT_IMG_WIDTH               640
#define DEFAULT_IMG_HEIGHT              480

struct priv_config;

typedef struct conf_file
{
    std::string path;
    struct
    {
        std::string level;
    } logger;
    struct
    {
        role_e type;
        std::string name;
    } role;
    struct
    {
        struct
        {
            std::string self_ip;
            std::string peer_ip;
            uint16_t self_port;
            uint16_t peer_port;
            uint16_t heartbeat_msecs;
            uint16_t poll_timeout_msecs;
        } endpoint;
        struct
        {
            std::string ip;
            uint16_t port;
            uint16_t max_payload_size;
            struct
            {
                std::string interface;
                bool needs_local_copy;
                uint16_t packets_per_batch;
                uint16_t batch_gap_usecs;
                uint32_t sendbuf_size;
            } send_policy;
        } multicast;
    } network;
    struct
    {
        bool enabled;
        bool delays_flushing;
        union
        {
            uint8_t tail_waiting_secs;
            uint8_t min_duration_secs;
        };
        uint8_t backup_history_days;
        struct
        {
            bool enabled;
            std::string path;
        } ramfs;
        std::string dir;
        struct
        {
            std::string ip;
            uint16_t port;
            std::string user;
            std::string password;
        } sync;
    } save;
    struct
    {
        std::string play_command;
        std::pair<std::string, int> compression;
    } video;
    struct
    {
        std::string record_start;
        std::string record_end;
        float volume;
    } audio;
    struct
    {
        std::string device;
        std::string capture_format;
        std::string result_format;
        std::vector<uint32_t> io_modes;
        std::vector<std::pair<uint16_t, uint16_t>> image_sizes;
        uint8_t which_size;
        uint8_t buffer_count;
        float fallback_fps;
    } camera;
    struct
    {
        std::vector<std::pair<uint16_t, uint16_t>> canvas_sizes;
        uint8_t which_size;
        struct
        {
            std::string version;
            std::string profile;
        } opengl;
    } player;
    struct
    {
        uint16_t spacing_frame_count;
        struct
        {
            uint16_t width;
            uint16_t height;
            std::string path;
        } model;
        struct
        {
            std::string file;
            std::map<std::string, float> thresholds;
        } label;
    } inference;
    struct
    {
        uint16_t capture_duration_secs;
    } test;
    std::shared_ptr<struct priv_config> priv;
} conf_file_t;

int load_config_file(const char *path, conf_file_t &result, bool debug = true);

void unload_config_file(conf_file_t &result);

#endif /* #ifndef __CONFIG_FILE_HPP__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-04, Man Hung-Coeng <udc577@126.com>:
 *  01. Ported from another private personal project.
 *
 * >>> 2026-04-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Add debug parameter to load_config_file().
 */

