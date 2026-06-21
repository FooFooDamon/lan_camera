#!/bin/bash

# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

if [ -e ~/lib/liblancs_cust.so ]; then
    set -x
    export LD_PRELOAD=~/lib/liblancs_cust.so
    export LD_LIBRARY_PATH=/opt/lan_camera/src/server:$LD_LIBRARY_PATH
    set +x
fi

/opt/lan_camera/src/server/lanc_server -c ~/etc/lan_camera.srv.json "$@"

