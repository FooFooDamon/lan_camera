#!/bin/bash

# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

if [ -e ~/lib/liblancs_cust.so ]; then
    set -x
    export LD_PRELOAD=~/lib/liblancs_cust.so
    export LD_LIBRARY_PATH=~/lib:$LD_LIBRARY_PATH
    set +x
fi

~/bin/lanc_server -c ~/etc/lan_camera.srv.json "$@"

