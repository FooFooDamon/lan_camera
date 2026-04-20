# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

.PHONY: all prepare revision distclean

export LAZY_CODING_URL ?= https://github.com/FooFooDamon/lazy_coding_skills

export REV_MKFILE ?= $(abspath __ver__.mk)

override undefine PREREQUISITE_FILES
PREREQUISITE_FILES := ${LAZY_CODING_URL}/raw/77d444b429e8ac0166e876834a52e97e0d3b3237/c_and_cpp/native/signal_handling.c \
    ${LAZY_CODING_URL}/raw/77d444b429e8ac0166e876834a52e97e0d3b3237/c_and_cpp/native/signal_handling.h \
    ${LAZY_CODING_URL}/raw/69f6cff480ad6503c33bad6811113d887c291dce/c_and_cpp/native/formatted_logging_adapter.h \
    ${LAZY_CODING_URL}/raw/ffd4f563b2fadf9dc02e801054d813ab57f6dbf5/c_and_cpp/native/communication_protocol.c \
    ${LAZY_CODING_URL}/raw/03a9e315b647f660132042cfd9ea62605d0bbe70/c_and_cpp/native/communication_protocol.h \

ifeq ($(shell [ true $(foreach i, ${REV_MKFILE} $(notdir ${PREREQUISITE_FILES}), -a -s ${i}) ] && echo 1 || echo 0),0)

all prepare:
	@[ -e $$(dirname ${REV_MKFILE}) ] || mkdir -p $$(dirname $${REV_MKFILE})
	@[ -s ${REV_MKFILE} ] || wget -c -O ${REV_MKFILE} "${LAZY_CODING_URL}/raw/main/makefiles/$$(basename ${REV_MKFILE})"
	@for i in ${PREREQUISITE_FILES}; \
	do \
		[ -s $$(basename $${i}) ] || wget -c -O $$(basename $${i}) "$${i}"; \
	done
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

distclean:
	@${RM} $(notdir ${REV_MKFILE} ${PREREQUISITE_FILES})

