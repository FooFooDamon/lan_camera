# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

.PHONY: all prepare revision

LAZY_CODING_URL ?= https://github.com/FooFooDamon/lazy_coding_skills
REV_MKFILE := $(abspath __ver__.mk)

ifeq ($(shell [ -s ${REV_MKFILE} ] && echo 1 || echo 0),0)

all prepare:
	[ -e $$(dirname ${REV_MKFILE}) ] || mkdir -p $$(dirname $${REV_MKFILE})
	[ -s ${REV_MKFILE} ] || wget -c -O ${REV_MKFILE} "${LAZY_CODING_URL}/raw/main/makefiles/$$(basename ${REV_MKFILE})"
	@echo "~ ~ ~ Minimum preparation finished successfully ~ ~ ~"
	@echo "Re-run your command again to continue your work."

else

all:
	@:

revision:
	@echo "Revision: ${__VER__}"

export EVAL_VERSION_ONCE ?= Y

include ${REV_MKFILE}

endif

