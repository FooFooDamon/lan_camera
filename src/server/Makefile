# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

.PHONY: all prepare dependencies .ALWAYS_MAKE dist distclean install_debug_info install uninstall

LAZY_CODING_URL ?= https://github.com/FooFooDamon/lazy_coding_skills
COMMON_DIR := ../common

LAZY_CODING_MAKEFILES ?= $(abspath ${COMMON_DIR}/__ver__.mk c_and_cpp.mk)

override PREREQUISITE_FILES := \
    ${LAZY_CODING_URL}/raw/e3c8088f4c13818d5c85818d3c43ad2d10a4f0ed/c_and_cpp/native/camera_v4l2.c \
    ${LAZY_CODING_URL}/raw/b3a4d5667aedabb967090ca5e02e9e633d8366ce/c_and_cpp/native/camera_v4l2.h \

ifeq ($(shell [ true $(foreach i, ${LAZY_CODING_MAKEFILES} $(notdir ${PREREQUISITE_FILES}), -a -s ${i}) ] && echo 1 || echo 0),0)

all prepare: dependencies
	@for i in ${LAZY_CODING_MAKEFILES}; \
	do \
		mkdir -p $$(dirname $${i}); \
		[ -s $${i} ] || wget -c -O $${i} "${LAZY_CODING_URL}/raw/main/makefiles/$$(basename $${i})"; \
	done
	@for i in ${PREREQUISITE_FILES}; \
	do \
		[ -s $$(basename $${i}) ] || wget -c -O $$(basename $${i}) "$${i}"; \
	done
	@echo "~ ~ ~ Minimum preparation finished successfully ~ ~ ~"
	@echo "Re-run your command again to continue your work."

else

all: dependencies

COMMON_C_SRCS ?= ./signal_handling.c ./communication_protocol.c
COMMON_CXX_SRCS ?= ./cmdline_args.cpp ./config_file.cpp ./biz_protocols.cpp ./biz_save.cpp

C_SRCS := $(shell find ./ -name "*.c" | grep -v '\.priv\.c$$')
C_SRCS := $(sort ${C_SRCS} ${COMMON_C_SRCS})

CXX_SRCS := $(shell find ./ -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" | grep -v '\.priv\.[^.]\+$$')
CXX_SRCS := $(sort ${CXX_SRCS} ${COMMON_CXX_SRCS})

override EXE_DIR := ${HOME}/bin
override LIB_DIR := ${HOME}/lib
override EXE_FILE := lanc_server
override CORE_LIB := liblancs_core.so
override CUST_LIB := $(wildcard ${LIB_DIR}/liblancs_cust.so)

ifeq (${CUST_LIB},) # No customization.

GOAL := ${EXE_FILE}

# Just defining dependencies is enough. No linking rule needed.
${GOAL}: $(addsuffix .o, $(basename ${C_SRCS} ${CXX_SRCS}))

else # Found customization.

GOALS := ${CORE_LIB} ${EXE_FILE}

#
# All weak symbols must be packed into a shared library
# so that they can be overridden by the ones in ${CUST_LIB}.
#
# Also be aware that ${CUST_LIB} should be preloaded
# by setting LD_PRELOAD environment variable
# before starting ${EXE_FILE}.
#
${CORE_LIB}: $(filter-out ./main.o, $(addsuffix .o, $(basename ${C_SRCS} ${CXX_SRCS})))

${EXE_FILE}: main.o ${CORE_LIB}

endif

__COMMON_VER__ ?= $(strip $(shell ${MAKE} -s -C ${COMMON_DIR} revision | grep -i '^Revision:' | awk -F : '{ print $$2 }'))
C_DEFINES += -D__COMMON_VER__='"${__COMMON_VER__}"' -DCOMMPROTO_LITTLE_ENDIAN
CXX_DEFINES += -D__COMMON_VER__='"${__COMMON_VER__}"' -DCOMMPROTO_LITTLE_ENDIAN \
                -DNEED_OS_SIGNALS -DHAS_CONFIG_FILE \
                -DHAS_LOGGER -DUSE_CUSTOM_FMT_LOG -DCUSTOM_FMT_LOG_H='"fmt_log.hpp"' \
                $(if ${FMT_LOG_IMPL_H}, -DFMT_LOG_IMPL_H='"${FMT_LOG_IMPL_H}"')
C_INCLUDES += -I${COMMON_DIR}
CXX_INCLUDES += -I${COMMON_DIR} $(shell pkg-config --cflags opencv4)
OTHER_CFLAGS +=
OTHER_CXXFLAGS +=
C_LDFLAGS +=
CXX_LDFLAGS += $(shell pkg-config --libs opencv4) -ljsoncpp -llz4 -lz

cmdline_args.o: CXX_DEFINES += -DBIZ_TYPE_CANDIDATES='"server,test"' -DBIZ_TYPE_DEFAULT='"server"' \
                -DBRIEF_INTRO='"Camera Server running on Local Area Network"'

signal_handling.o: C_DEFINES += -U__STRICT_ANSI__

include ${LAZY_CODING_MAKEFILES}

distclean: clean clean_downloads dependencies

clean_downloads:
	${Q}${RM} $(notdir ${LAZY_CODING_MAKEFILES} ${PREREQUISITE_FILES} ${COMMON_C_SRCS} ${COMMON_CXX_SRCS})

versions.h: .revision
	${Q}touch $@

.revision: .ALWAYS_MAKE
	${Q}[ -e $@ ] || touch $@
	${Q}[ '$(file < $@)' = '${__VER__}|${__COMMON_VER__}' ] || printf '${__VER__}|${__COMMON_VER__}' > $@

.ALWAYS_MAKE:

MAIN_VER ?= $(shell grep "#define [A-Z_]*_VER[[:blank:]]" versions.h | awk '{ print $$3 }' | sed 'N;N; s/\n/./g')
FULL_VER ?= ${MAIN_VER}.${__VER__}.${__COMMON_VER__}
DEBUG_DIR ?= ${HOME}/debug/${EXE_FILE}/${MAIN_VER}/${__VER__}.${__COMMON_VER__}
override INSTALL := install -D --compare --backup=numbered
override REALPATH := realpath -m
override LS := ls -h -g -o
override OLD_VER_WARN := awk '{ print $$3, $$4, $$5, $$6, $$7 }' | xargs -I {} echo "*** Found old version: {}"
override DEBUG_WARN := printf "\033[0;33m*** It's STRONGLY RECOMMENDED to move this file to another private host for safety! ***\033[0m\n"

install_debug_info:
	$(if $(wildcard ${CORE_LIB}.dbgi),$(if ${Q},@printf 'INSTALL\t${DEBUG_DIR}/${CORE_LIB}.dbgi\n'))
	$(if $(wildcard ${CORE_LIB}.dbgi),${Q}${INSTALL} --backup=off ${CORE_LIB}.dbgi ${DEBUG_DIR}/${CORE_LIB}.dbgi)
	$(if $(wildcard ${CORE_LIB}.dbgi),$(if ${Q},@${DEBUG_WARN}))
	$(if $(wildcard ${EXE_FILE}.dbgi),$(if ${Q},@printf 'INSTALL\t${DEBUG_DIR}/${EXE_FILE}.dbgi\n'))
	$(if $(wildcard ${EXE_FILE}.dbgi),${Q}${INSTALL} --backup=off ${EXE_FILE}.dbgi ${DEBUG_DIR}/${EXE_FILE}.dbgi)
	$(if $(wildcard ${EXE_FILE}.dbgi),$(if ${Q},@${DEBUG_WARN}))

install: $(if $(wildcard ${CORE_LIB}), ${GOALS}, ${GOAL}) install_debug_info
	$(if $(wildcard ${CORE_LIB}),$(if ${Q},@printf 'INSTALL\t${LIB_DIR}/${CORE_LIB}.${FULL_VER}\n'))
	$(if $(wildcard ${CORE_LIB}),${Q}${INSTALL} ${CORE_LIB} ${LIB_DIR}/${CORE_LIB}.${FULL_VER})
	$(if $(wildcard ${CORE_LIB}),${Q}cd ${LIB_DIR} && ${SYMLINK} ${CORE_LIB}.${FULL_VER} ${CORE_LIB})
	$(if $(wildcard ${CORE_LIB}),${Q}${LS} ${LIB_DIR}/${CORE_LIB}.${FULL_VER}.* 2> /dev/null | ${OLD_VER_WARN})
	$(if ${Q},@printf 'INSTALL\t${EXE_DIR}/${EXE_FILE}.${FULL_VER}\n')
	${Q}${INSTALL} ${EXE_FILE} ${EXE_DIR}/${EXE_FILE}.${FULL_VER}
	${Q}cd ${EXE_DIR} && ${SYMLINK} ${EXE_FILE}.${FULL_VER} ${EXE_FILE}
	${Q}${LS} ${EXE_DIR}/${EXE_FILE}.${FULL_VER}.* 2> /dev/null | ${OLD_VER_WARN}

uninstall:
	$(if ${Q},@printf 'RM\t$(shell ${REALPATH} ${EXE_DIR}/${EXE_FILE})\n')
	${Q}${LS} $(shell ${REALPATH} ${EXE_DIR}/${EXE_FILE}).* 2> /dev/null | ${OLD_VER_WARN}
	${Q}${RM} ${EXE_DIR}/${EXE_FILE} $(shell ${REALPATH} ${EXE_DIR}/${EXE_FILE})
	$(if $(wildcard ${LIB_DIR}/${CORE_LIB}),$(if ${Q},@printf 'RM\t$(shell ${REALPATH} ${LIB_DIR}/${CORE_LIB})\n'))
	$(if $(wildcard ${LIB_DIR}/${CORE_LIB}*),${Q}${LS} $(shell ${REALPATH} ${LIB_DIR}/${CORE_LIB}).* 2> /dev/null | ${OLD_VER_WARN})
	$(if $(wildcard ${LIB_DIR}/${CORE_LIB}),${Q}${RM} ${LIB_DIR}/${CORE_LIB} $(shell ${REALPATH} ${LIB_DIR}/${CORE_LIB}))

${COMMON_C_SRCS} ${COMMON_CXX_SRCS}: %: ${COMMON_DIR}/%
	$(if ${Q},@printf 'LINK\t$<\n')
	${Q}ln -s $< $@

camera_v4l2.o: camera_v4l2.c
	$(if ${Q},@printf 'CXX\t$<\n')
	${Q}${CXX_COMPILE}

endif

DEPENDENCY_DIRS := $(abspath ../3rdparty ${COMMON_DIR})

dependencies:
	@for i in ${DEPENDENCY_DIRS}; \
	do \
		[ -s $${i}/[Mm]akefile ] || continue; \
		${MAKE} -C $${i} $(filter all prepare distclean, ${MAKECMDGOALS}); \
	done

