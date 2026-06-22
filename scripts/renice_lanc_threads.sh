#!/bin/bash

# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

usage()
{
    echo "Usage: $0 /path/to/lan_camera.{srv,cli}.json"
    echo "Example: $0 ~/etc/lan_camera.srv.json"
}

for i in "$@"
do
    if [ "${i}" = "-h" ] || [ "${i}" = "--help" ]; then
        usage
        exit 0
    fi
done

if [ $# -lt 1 ] || [ ! -e "$1" ]; then
    echo "*** Configuration file non-existent or not specified: $1" >&2
    usage >&2
    exit 1
fi

config_file="$1"
program_type=$(jq -r .role.type "${config_file}")

ps -ef | grep "[l]anc_${program_type} -c" | awk '{ print $2 }' | while read pid
do
    ls /proc/${pid}/task | while read tid
    do
        name="$(cat /proc/${pid}/task/${tid}/comm)"
        value=$(jq -r ".schedule.nice_level.\"${name}\"" "${config_file}")

        [ "${value}" != "null" ] || continue

        echo "Setting the nice value of thread[${tid}|${name}] to ${value} ..."
        sudo renice ${value} -p ${tid}
    done

    break
done

