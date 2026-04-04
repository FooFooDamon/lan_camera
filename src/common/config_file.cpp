// SPDX-License-Identifier: Apache-2.0

/*
 * APIs for loading and unloading configuration file.
 *
 * Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
 */

#include "config_file.hpp"

#include <linux/videodev2.h>

#include <fstream>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>

#include "fmt_log.hpp"

static int s_log_level = LOG_LEVEL_INFO;

#define PRINT_INFO(_fmt_, ...)              do { \
    if (LOG_LEVEL_INFO <= s_log_level) \
        printf(_fmt_ "\n", ##__VA_ARGS__); \
} while (0)

#define PRINT_WARNING(_fmt_, ...)           do { \
    if (LOG_LEVEL_WARNING <= s_log_level) \
        fprintf(stderr, "\033[0;33m" __FILE__ ":%d %s():\n" _fmt_ "\n\033[0m", __LINE__, __func__, ##__VA_ARGS__); \
} while (0)

#define PRINT_ERROR(_fmt_, ...)             do { \
    if (LOG_LEVEL_ERR <= s_log_level) \
        fprintf(stderr, "\033[0;31m*** " __FILE__ ":%d %s():\n" _fmt_ "\n\033[0m", __LINE__, __func__, ##__VA_ARGS__); \
} while (0)

#define ASSERT_JSON_TYPE(_node_, _type_)    do { \
    if (!(_node_).is##_type_()) \
        throw Json::Exception(#_type_ " type is expected! Check and fix it:\n" + (_node_).toStyledString()); \
} while (0)

#ifndef LANC_ROOT_DIR
#define LANC_ROOT_DIR                       "/opt/lan_camera"
#endif

#ifndef ETC_ROOT_DIR
#define ETC_ROOT_DIR                        LANC_ROOT_DIR "/etc"
#endif

static int parse_logger_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    const Json::Value &part_obj = root[part];
    const char *obj_name;
    const char *item_key;

    try
    {
        obj_name = nullptr;
        item_key = "level";
        result.logger.level = part_obj.get(item_key, "notice").asString();
        s_log_level = to_log_level(result.logger.level.c_str());
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\tlevel: %s", result.logger.level.c_str());

    return 0;
}

static int parse_role_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    const Json::Value &part_obj = root[part];
    const std::string &type_str = part_obj.get("type", "unknown").asString();
    const char *obj_name;
    const char *item_key;

    try
    {
        obj_name = nullptr;
        item_key = "type";
        if ("server" == type_str || "client" == type_str)
            result.role.type = ("server" == type_str) ? ROLE_SERVER : ROLE_CLIENT;
        else
            throw Json::Exception("Role type is neither [server] nor [client]: [" + type_str + "]");
        item_key = "name";
        result.role.name = part_obj.get(item_key, "lanc-9527").asString();
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\ttype: %s", (ROLE_SERVER == result.role.type) ? "server" : "client");
    PRINT_INFO("\tname: %s", result.role.name.c_str());

    return 0;
}

static int parse_network_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    const Json::Value &part_obj = root[part];
    bool is_server = (ROLE_SERVER == result.role.type);
    const char *endpoint = is_server ? "bind" : "connect";
    const char *obj_name = endpoint;
    const char *item_key = "";
    const Json::Value &obj_endpoint = part_obj[obj_name];
    const Json::Value &obj_multicast = part_obj["multicast"];
    Json::Value obj_send_policy;
    auto &config_endpoint = result.network.endpoint;
    auto &config_multicast = result.network.multicast;
    auto &config_send_policy = config_multicast.send_policy;
    const uint16_t DEFAULT_SERVER_PORT = 59394;

    try
    {
        ASSERT_JSON_TYPE(obj_endpoint, Object);
        if (is_server)
        {
            item_key = "ip";
            config_endpoint.self_ip = obj_endpoint.get(item_key, "0.0.0.0").asString();
            item_key = "port";
            config_endpoint.self_port = obj_endpoint.get(item_key, DEFAULT_SERVER_PORT).asUInt();
        }
        else
        {
            item_key = "ip";
            config_endpoint.self_ip = "";
            config_endpoint.peer_ip = obj_endpoint.get(item_key, "127.0.0.1").asString();
            item_key = "port";
            config_endpoint.self_port = 0;
            config_endpoint.peer_port = obj_endpoint.get(item_key, DEFAULT_SERVER_PORT).asUInt();
        }
        item_key = "heartbeat_msecs";
        config_endpoint.heartbeat_msecs = obj_endpoint.get(item_key, is_server ? 1000 : 500).asUInt();
        item_key = "poll_timeout_msecs";
        config_endpoint.poll_timeout_msecs = obj_endpoint.get(item_key, is_server ? 1000 : 10000).asUInt();
        if (!is_server)
            goto lbl_print_role_info;

        obj_name = "multicast";
        item_key = "";
        ASSERT_JSON_TYPE(obj_multicast, Object);
        item_key = "ip";
        config_multicast.ip = obj_multicast.get(item_key, "239.0.0.0").asString();
        item_key = "port";
        config_multicast.port = obj_multicast.get(item_key, 28384).asUInt();
        item_key = "max_payload_size";
        config_multicast.max_payload_size = obj_multicast.get(item_key, 1400).asUInt();
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

    try
    {
        obj_name = "send_policy";
        item_key = "";
        obj_send_policy = obj_multicast[obj_name];
        ASSERT_JSON_TYPE(obj_send_policy, Object);
        item_key = "interface";
        config_send_policy.interface = obj_send_policy.get(item_key, "eth0").asString();
        item_key = "needs_local_copy";
        config_send_policy.needs_local_copy = obj_send_policy.get(item_key, false).asBool();
        item_key = "packets_per_batch";
        config_send_policy.packets_per_batch = obj_send_policy.get(item_key, 5).asUInt();
        item_key = "batch_gap_usecs";
        config_send_policy.batch_gap_usecs = obj_send_policy.get(item_key, 50).asUInt();
        item_key = "sendbuf_size";
        config_send_policy.sendbuf_size = obj_send_policy.get(item_key, 1024 * 1024 * 8).asUInt();
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s/multicast/%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

lbl_print_role_info:

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\t%s:", endpoint);
    if (is_server)
    {
        PRINT_INFO("\t\tip: %s", config_endpoint.self_ip.c_str());
        PRINT_INFO("\t\tport: %d", config_endpoint.self_port);
    }
    else
    {
        PRINT_INFO("\t\tip: %s", config_endpoint.peer_ip.c_str());
        PRINT_INFO("\t\tport: %d", config_endpoint.peer_port);
    }
    PRINT_INFO("\t\theartbeat_msecs: %d", config_endpoint.heartbeat_msecs);
    PRINT_INFO("\t\tpoll_timeout_msecs: %d", config_endpoint.poll_timeout_msecs);
    if (is_server)
    {
        PRINT_INFO("\t%s:", "multicast");
        PRINT_INFO("\t\tip: %s", config_multicast.ip.c_str());
        PRINT_INFO("\t\tport: %d", config_multicast.port);
        PRINT_INFO("\t\tmax_payload_size: %d", config_multicast.max_payload_size);
        PRINT_INFO("\t\t%s:", "send_policy");
        PRINT_INFO("\t\t\tinterface: %s", config_send_policy.interface.c_str());
        PRINT_INFO("\t\t\tneeds_local_copy: %s", config_send_policy.needs_local_copy ? "true" : "false");
        PRINT_INFO("\t\t\tpackets_per_batch: %d", config_send_policy.packets_per_batch);
        PRINT_INFO("\t\t\tbatch_gap_usecs: %d", config_send_policy.batch_gap_usecs);
        PRINT_INFO("\t\t\tsendbuf_size: %u", config_send_policy.sendbuf_size);
    }

    return 0;
}

static int parse_save_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    const Json::Value &part_obj = root[part];
    const Json::Value &obj_ramfs = part_obj["ramfs"];
    const Json::Value &obj_sync = part_obj["sync"];
    std::string SYNC_DEFAULT_PASSWORD = (ROLE_SERVER == result.role.type)
        ? "file:///etc/rsyncd.secrets"
        : "file:///usr/local/etc/rsync-lanc.pswd";
    const char *obj_name;
    const char *item_key;

    try
    {
        obj_name = nullptr;
        item_key = "enabled";
        result.save.enabled = part_obj.get(item_key, true).asBool();
        item_key = "dir";
        result.save.dir = part_obj.get(item_key, LANC_ROOT_DIR "/save").asString();
        item_key = "delays_flushing";
        result.save.delays_flushing = part_obj.get(item_key, false).asBool();
        item_key = "min_duration_secs";
        result.save.min_duration_secs = part_obj.get(item_key, 10).asUInt();
        item_key = "backup_history_days";
        result.save.backup_history_days = part_obj.get(item_key, 90).asUInt();

        obj_name = "ramfs";
        item_key = "";
        ASSERT_JSON_TYPE(obj_ramfs, Object);
        item_key = "enabled";
        result.save.ramfs.enabled = obj_ramfs.get(item_key, true).asBool();
        item_key = "path";
        result.save.ramfs.path = obj_ramfs.get(item_key, "/tmp").asString();

        obj_name = "sync";
        item_key = "";
        ASSERT_JSON_TYPE(obj_sync, Object);
        item_key = "ip";
        result.save.sync.ip = obj_sync.get(item_key, "127.0.0.1").asString();
        item_key = "port";
        result.save.sync.port = obj_sync.get(item_key, 873).asUInt();
        item_key = "user";
        result.save.sync.user = obj_sync.get(item_key, "lanc").asString();
        item_key = "password";
        result.save.sync.password = obj_sync.get(item_key, SYNC_DEFAULT_PASSWORD).asString();
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\tenable: %s", result.save.enabled ? "true" : "false");
    PRINT_INFO("\tdir: %s", result.save.dir.c_str());
    PRINT_INFO("\t%s:", "ramfs");
    PRINT_INFO("\t\tenabled: %s", result.save.ramfs.enabled ? "true" : "false");
    PRINT_INFO("\t\tpath: %s", result.save.ramfs.path.c_str());
    PRINT_INFO("\tdelays_flushing: %s", result.save.delays_flushing ? "true" : "false");
    PRINT_INFO("\tmin_duration_secs: %d", result.save.min_duration_secs);
    PRINT_INFO("\tbackup_history_days: %d", result.save.backup_history_days);
    PRINT_INFO("\t%s:", "sync");
    PRINT_INFO("\t\tip: %s", result.save.sync.ip.c_str());
    PRINT_INFO("\t\tport: %d", result.save.sync.port);
    PRINT_INFO("\t\tuser: %s", result.save.sync.user.c_str());
    PRINT_INFO("\t\tpassword: %s", ("" == result.save.sync.password) ? "" : "...");

    return 0;
}

static int parse_video_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    if (ROLE_SERVER != result.role.type)
        return 0;

    const Json::Value &part_obj = root[part];
    const Json::Value &obj_compression = part_obj["compression"];
    auto &config_compression = result.video.compression;
    std::pair<int, int> compression_range = { 0, 0 };
    std::string obj_name;
    const char *item_key;

    try
    {
        obj_name.clear();
        item_key = "play_command";
        result.video.play_command = part_obj.get(item_key, "mpv").asString();

        obj_name = "compression";
        item_key = "";
        ASSERT_JSON_TYPE(obj_compression, Object);
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name.empty() ? "" : obj_name.c_str()), (obj_name.empty() ? "" : "/"), item_key, e.what());

        return -1;
    }

    try
    {
        config_compression = { "<UNSET>", 0 };
        for (const auto &key : obj_compression.getMemberNames())
        {
            const Json::Value &obj_compress_method = obj_compression[key];

            obj_name = key;
            item_key = "";
            ASSERT_JSON_TYPE(obj_compress_method, Object);

            item_key = "enabled";
            if (obj_compress_method.get(item_key, false).asBool())
            {
                Json::Value arr_range;

                config_compression.first = obj_name;
                item_key = "level";
                config_compression.second = obj_compress_method.get(item_key, 0).asInt();
                item_key = "range";
                arr_range = obj_compress_method[item_key];
                if (!arr_range.isArray() || arr_range.size() < 2)
                {
                    char buf[128] = { 0 };

                    snprintf(buf, sizeof(buf) - 1,
                        "2-element array [<for-biggest-size>, <for-smallest-size>] is expected! Check and fix it:\n");
                    throw Json::Exception(Json::String(buf) + arr_range.toStyledString());
                }
                compression_range = { arr_range[0].asInt(), arr_range[1].asInt() };

                break;
            }
        }
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [compression/%s/%s]: %s", part, obj_name.c_str(), item_key, e.what());

        return -1;
    }

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\tplay_command: %s", result.video.play_command.c_str());
    PRINT_INFO("\t%s:", "compression");
    PRINT_INFO("\t\talgorithm: %s", config_compression.first.c_str());
    PRINT_INFO("\t\tlevel: %d", config_compression.second);
    PRINT_INFO("\t\trange: [%d, %d] // [<for-biggest-size>, <for-smallest-size>]",
        compression_range.first, compression_range.second);

    return 0;
}

static int parse_audio_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    if (ROLE_SERVER == result.role.type)
        return 0;

    const Json::Value &part_obj = root[part];
    const char *obj_name;
    const char *item_key;

    try
    {
        obj_name = nullptr;
        item_key = "record_start";
        result.audio.record_start = part_obj.get(item_key, ETC_ROOT_DIR "/audio/record-start.flac").asString();
        item_key = "record_end";
        result.audio.record_end = part_obj.get(item_key, ETC_ROOT_DIR "/audio/record-end.flac").asString();
        item_key = "volume";
        result.audio.volume = part_obj.get(item_key, 0.67).asFloat();
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\trecord_start: %s", result.audio.record_start.c_str());
    PRINT_INFO("\trecord_end: %s", result.audio.record_end.c_str());
    PRINT_INFO("\tvolume: %.2f", result.audio.volume);

    return 0;
}

static int parse_camera_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    if (ROLE_SERVER != result.role.type)
        return 0;

    const Json::Value &part_obj = root[part];
    const Json::Value &arr_image_sizes = part_obj["image_sizes"];
    const Json::Value &arr_io_modes = part_obj["io_modes"];
    auto &config_img_sizes = result.camera.image_sizes;
    auto &config_io_modes = result.camera.io_modes;
    const char *obj_name;
    const char *item_key;

    try
    {
        obj_name = nullptr;
        item_key = "device";
        result.camera.device = part_obj.get(item_key, "/dev/video0").asString();
        item_key = "fallback_fps";
        result.camera.fallback_fps = part_obj.get(item_key, 15.0).asFloat();
        item_key = "capture_format";
        result.camera.capture_format = part_obj.get(item_key, "auto").asString();
        item_key = "result_format";
        result.camera.result_format = part_obj.get(item_key, "BGR").asString();
        item_key = "image_sizes";
        config_img_sizes.clear();
        ASSERT_JSON_TYPE(arr_image_sizes, Array);
        for (Json::ArrayIndex i = 0; i < arr_image_sizes.size(); ++i)
        {
            const auto &image_size = arr_image_sizes[i];

            if (!image_size.isArray() || image_size.size() < 2)
            {
                char buf[128] = { 0 };

                snprintf(buf, sizeof(buf) - 1,
                    "[%d]: 2-element array [<width>, <height>] is expected! Check and fix it:\n", i + 1);
                throw Json::Exception(Json::String(buf) + image_size.toStyledString());
            }

            config_img_sizes.push_back({ image_size[0].asUInt(), image_size[1].asUInt() });
        }
        item_key = "which_size";
        result.camera.which_size = part_obj.get(item_key, 1).asUInt();
        item_key = "buffer_count";
        result.camera.buffer_count = part_obj.get(item_key, 4).asUInt();
        item_key = "io_modes";
        config_io_modes.clear();
        ASSERT_JSON_TYPE(arr_io_modes, Array);
        for (Json::ArrayIndex i = 0; i < arr_io_modes.size(); ++i)
        {
            const std::string &io_mode = arr_io_modes[i].asString();

            if ("dmabuf" == io_mode)
                config_io_modes.push_back(V4L2_MEMORY_DMABUF);
            else if ("mmap" == io_mode)
                config_io_modes.push_back(V4L2_MEMORY_MMAP);
            else if ("userptr" == io_mode)
                config_io_modes.push_back(V4L2_MEMORY_USERPTR);
            else
                throw Json::Exception("Unknown I/O mode: " + io_mode);
        }
        if (!config_io_modes.empty())
            config_io_modes.push_back(0); // sentinel
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\tdevice: %s", result.camera.device.c_str());
    PRINT_INFO("\tfallback_fps: %.2f", result.camera.fallback_fps);
    PRINT_INFO("\tcapture_format: %s", result.camera.capture_format.c_str());
    PRINT_INFO("\tresult_format: %s", result.camera.result_format.c_str());
    PRINT_INFO("\t%s:", "image_sizes");
    for (size_t i = 0; i < config_img_sizes.size(); ++i)
    {
        PRINT_INFO("\t\t[%d]: [%d, %d]", (int)i + 1, config_img_sizes[i].first, config_img_sizes[i].second);
    }
    PRINT_INFO("\twhich_size: %d", result.camera.which_size);
    PRINT_INFO("\tbuffer_count: %d", result.camera.buffer_count);
    PRINT_INFO("\t%s:", "io_modes");
    for (const auto &io_mode : arr_io_modes)
    {
        PRINT_INFO("\t\t%s", io_mode.asCString());
    }

    return 0;
}

static int parse_player_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    if (ROLE_SERVER == result.role.type)
        return 0;

    const Json::Value &part_obj = root[part];
    const Json::Value &arr_canvas_sizes = part_obj["canvas_sizes"];
    const Json::Value &obj_opengl = part_obj["opengl"];
    auto &config_opengl = result.player.opengl;
    auto &config_sizes = result.player.canvas_sizes;
    const char *obj_name;
    const char *item_key;

    try
    {
        obj_name = nullptr;
        item_key = "canvas_sizes";
        config_sizes.clear();
        ASSERT_JSON_TYPE(arr_canvas_sizes, Array);
        for (Json::ArrayIndex i = 0; i < arr_canvas_sizes.size(); ++i)
        {
            const auto &canvas_size = arr_canvas_sizes[i];

            if (!canvas_size.isArray() || canvas_size.size() < 2)
            {
                char buf[128] = { 0 };

                snprintf(buf, sizeof(buf) - 1,
                    "[%d]: 2-element array [<width>, <height>] is expected! Check and fix it:\n", i + 1);
                throw Json::Exception(Json::String(buf) + canvas_size.toStyledString());
            }

            config_sizes.push_back({ canvas_size[0].asUInt(), canvas_size[1].asUInt() });
        }
        item_key = "which_size";
        result.player.which_size = part_obj.get(item_key, 1).asUInt();

        obj_name = "opengl";
        ASSERT_JSON_TYPE(obj_opengl, Object);
        item_key = "version";
        config_opengl.version = obj_opengl.get(item_key, "320 es").asString();
        item_key = "profile";
        config_opengl.profile = obj_opengl.get(item_key, "core").asString();
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\t%s:", "canvas_sizes");
    for (size_t i = 0; i < config_sizes.size(); ++i)
    {
        PRINT_INFO("\t\t[%d]: [%d, %d]", (int)i + 1, config_sizes[i].first, config_sizes[i].second);
    }
    PRINT_INFO("\twhich_size: %d", result.player.which_size);
    PRINT_INFO("\t%s:", "opengl");
    PRINT_INFO("\t\tversion: %s", config_opengl.version.c_str());
    PRINT_INFO("\t\tprofile: %s", config_opengl.profile.c_str());

    return 0;
}

static int parse_inference_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    if (ROLE_SERVER != result.role.type)
        return 0;

    const Json::Value &part_obj = root[part];
    const Json::Value &obj_model = part_obj["model"];
    const Json::Value &obj_label = part_obj["label"];
    const Json::Value &obj_label_thresholds = obj_label["thresholds"];
    auto &model_config = result.inference.model;
    auto &label_config = result.inference.label;
    const char *obj_name;
    const char *item_key;

    try
    {
        obj_name = "model";
        item_key = "";
        ASSERT_JSON_TYPE(obj_model, Object);
        item_key = "width";
        model_config.width = obj_model.get(item_key, 640).asUInt();
        item_key = "height";
        model_config.height = obj_model.get(item_key, 640).asUInt();
        item_key = "path";
        model_config.path = obj_model.get(item_key, ETC_ROOT_DIR "/inference/classifier.model").asString();

        obj_name = "label";
        item_key = "";
        ASSERT_JSON_TYPE(obj_label, Object);
        item_key = "file";
        label_config.file = obj_label.get(item_key, ETC_ROOT_DIR "/inference/classifier.labels").asString();
        item_key = "thresholds";
        ASSERT_JSON_TYPE(obj_label_thresholds, Object);
        for (const auto &key : obj_label_thresholds.getMemberNames())
        {
            label_config.thresholds[key] = obj_label_thresholds.get(key, 0.99).asFloat();
        }

        obj_name = nullptr;
        item_key = "spacing_frame_count";
        result.inference.spacing_frame_count = part_obj.get(item_key, 14).asUInt();
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\t%s:", "model");
    PRINT_INFO("\t\twidth: %d", model_config.width);
    PRINT_INFO("\t\theight: %d", model_config.height);
    PRINT_INFO("\t\tpath: %s", model_config.path.c_str());
    PRINT_INFO("\t%s:", "label");
    PRINT_INFO("\t\tfile: %s", label_config.file.c_str());
    PRINT_INFO("\t\t%s:", "thresholds");
    for (const auto &threshold : label_config.thresholds)
    {
        PRINT_INFO("\t\t\t%s: %.2f", threshold.first.c_str(), threshold.second);
    }
    PRINT_INFO("\tspacing_frame_count: %d", result.inference.spacing_frame_count);

    return 0;
}

static int parse_test_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    if (ROLE_SERVER != result.role.type)
        return 0;

    const Json::Value &part_obj = root["test"];
    const char *obj_name;
    const char *item_key;

    try
    {
        obj_name = nullptr;
        item_key = "capture_duration_secs";
        result.test.capture_duration_secs = part_obj.get(item_key, 10).asUInt();
    }
    catch (const Json::Exception &e)
    {
        PRINT_ERROR("/%s: Failed to parse [%s%s%s]: %s",
            part, (obj_name ? obj_name : ""), (obj_name ? "/" : ""), item_key, e.what());

        return -1;
    }

    PRINT_INFO("/%s:", part);
    PRINT_INFO("\tcapture_duration_secs: %d", result.test.capture_duration_secs);

    return 0;
}

__attribute__((weak))
int parse_private_config(const Json::Value &root, const char *part, conf_file_t &result)
{
    return 0; // do nothing by default
}

int load_config_file(const char *path, conf_file_t &result)
{
    std::ifstream stream(path);

    if (!stream)
    {
        int err = errno;

        PRINT_ERROR("%s: Failed to open: %s", path, strerror(err));

        return -err;
    }
    result.path = path;

    Json::Value root;

    try
    {
        stream >> root;
        stream.close();
    }
    catch (const std::exception &e)
    {
        PRINT_ERROR("%s: JSON error: %s", path, e.what());

        return -EILSEQ;
    }

    typedef int (*parse_config_func_t)(const Json::Value &, const char *, conf_file_t &);
    struct
    {
        const char *name;
        parse_config_func_t func;
        role_e for_who;
    } part_parsers[] = {
        { "logger", parse_logger_config, ROLE_FOR_ALL },
        { "role", parse_role_config, ROLE_FOR_ALL },
        { "network", parse_network_config, ROLE_FOR_ALL },
        { "save", parse_save_config, ROLE_FOR_ALL },
        { "video", parse_video_config, ROLE_SERVER },
        { "audio", parse_audio_config, ROLE_CLIENT },
        { "camera", parse_camera_config, ROLE_SERVER },
        { "player", parse_player_config, ROLE_CLIENT },
        { "inference", parse_inference_config, ROLE_SERVER },
        { "test", parse_test_config, ROLE_SERVER },
        { "priv", parse_private_config, ROLE_FOR_ALL }
    };
    int ret = 0;

    for (const auto &part_parser : part_parsers)
    {
        const char *part_name = part_parser.name;
        const Json::Value &part_obj = root[part_name];
        bool is_null = part_obj.isNull();

        try
        {
            if (is_null)
            {
                if ((ROLE_FOR_ALL == part_parser.for_who || part_parser.for_who == result.role.type)
                    && 0 != strcmp(part_name, "priv"))
                {
                    throw Json::Exception("Not found!");
                }
            }
            else
            {
                ASSERT_JSON_TYPE(part_obj, Object);
            }
        }
        catch (const Json::Exception &e)
        {
            PRINT_ERROR("/%s: %s", part_name, e.what());

            break;
        }

        if ((ret = part_parser.func(root, part_name, result)) < 0)
            break;
    }

    return ret;
}

void unload_config_file(conf_file_t &result)
{
    // empty
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2026-04-04, Man Hung-Coeng <udc577@126.com>:
 *  01. Initial commit.
 */

