#!/bin/bash

# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

#
# Example of crontab item:
#
# 0 0 * * * bash /opt/lan_camera/scripts/clean_expired_videos.sh $HOME/etc/lan_camera.srv.json
#

if [ $# -lt 1 ]; then
    echo "Usage: $0 /path/to/server/config.json" >&2
    echo "Example: $0 ~/etc/lan_camera.srv.json" >&2
    exit 1
fi

conf_file="$1"
if [ ! -e "${conf_file}" ]; then
    echo "*** File does not exist: ${conf_file}" >&2
    exit 1
fi

root_dir=$(jq -r ".save.dir" "${conf_file}")/server
days_kept=$(($(jq -r '.save."backup_history_days"' "${conf_file}") + 1))
total_count=0
clean_count=0

while read i
do
    total_count=$((total_count + 1))
    [ ${total_count} -gt ${days_kept} ] || continue
    rm -r ${i} && echo "Cleaned: ${i}" && clean_count=$((clean_count + 1))
done <<< $(find ${root_dir}/ -mindepth 3 -maxdepth 3 -a -type d | sort -r)

echo "=== ${total_count} days in total, ${clean_count} days have been cleaned."

