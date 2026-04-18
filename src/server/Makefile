# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

.PHONY: all prepare dependencies .ALWAYS_MAKE install uninstall

LAZY_CODING_URL ?= https://github.com/FooFooDamon/lazy_coding_skills
COMMON_DIR := ../common

export LAZY_CODING_MAKEFILES ?= $(abspath ${COMMON_DIR}/__ver__.mk c_and_cpp.mk)

override undefine PREREQUISITE_FILES
PREREQUISITE_FILES := \
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

export EVAL_VERSION_ONCE ?= Y
override NO_PRIV_STUFF := $(strip $(filter-out n N no NO No 0, ${NO_PRIV_STUFF}))
export COMMON_C_SRCS ?= signal_handling.c communication_protocol.c
export COMMON_CXX_SRCS ?= cmdline_args.cpp config_file.cpp biz_protocols.cpp biz_save.cpp

C_SRCS := $(shell find ./ -name "*.c" | grep -v '\.priv\.c$$')
C_SRCS := $(sort ${C_SRCS} ${COMMON_C_SRCS})
ifeq (${NO_PRIV_STUFF},)
    C_SRCS := $(foreach i, $(filter-out %.priv.c, ${C_SRCS}), \
        $(if $(wildcard $(basename ${i}).priv.c), $(basename ${i}).priv.c, ${i}) \
    )
endif

CXX_SRCS := $(shell find ./ -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" | grep -v '\.priv\.[^.]\+$$')
CXX_SRCS := $(sort ${CXX_SRCS} ${COMMON_CXX_SRCS})
ifeq (${NO_PRIV_STUFF},)
    CXX_SRCS := $(foreach i, $(filter-out $(addprefix %.priv, $(suffix ${CXX_SRCS})), ${CXX_SRCS}), \
        $(if $(wildcard $(basename ${i}).priv$(suffix ${i})), $(basename ${i}).priv$(suffix ${i}), ${i}) \
    )
endif

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

cmdline_args.o: CXX_DEFINES += -DBIZ_TYPE_CANDIDATES='"server,test"' -DBIZ_TYPE_DEFAULT='"server"' \
                -DBRIEF_INTRO='"Camera Server running on Local Area Network"'
signal_handling.o: C_DEFINES += -U__STRICT_ANSI__

include ${LAZY_CODING_MAKEFILES}

versions.h: .revision
	${Q}touch $@

.revision: .ALWAYS_MAKE
	${Q}[ -e $@ ] || touch $@
	${Q}[ '$(file < $@)' = '${__VER__}|${__COMMON_VER__}' ] || printf '${__VER__}|${__COMMON_VER__}' > $@

.ALWAYS_MAKE:

override INSTALL := install -D --compare --backup=numbered

install:
	$(if $(wildcard ${CORE_LIB}),$(if ${Q},@printf 'INSTALL\t${LIB_DIR}/${CORE_LIB}\n'))
	$(if $(wildcard ${CORE_LIB}),${Q}${INSTALL} ${CORE_LIB} ${LIB_DIR}/${CORE_LIB})
	$(if $(wildcard ${CORE_LIB}),${Q}ls ${LIB_DIR}/${CORE_LIB}.* 2> /dev/null || :)
	$(if ${Q},@printf 'INSTALL\t${EXE_DIR}/${EXE_FILE}\n')
	${Q}${INSTALL} ${EXE_FILE} ${EXE_DIR}/${EXE_FILE}
	${Q}ls ${EXE_DIR}/${EXE_FILE}.* 2> /dev/null || :

uninstall:
	$(if ${Q},@printf 'RM\t${EXE_DIR}/${EXE_FILE}\n')
	${Q}rm -f ${EXE_DIR}/${EXE_FILE}
	${Q}ls ${EXE_DIR}/${EXE_FILE}.* 2> /dev/null || :
	$(if $(wildcard ${LIB_DIR}/${CORE_LIB}),$(if ${Q},@printf 'RM\t${LIB_DIR}/${CORE_LIB}\n'))
	$(if $(wildcard ${LIB_DIR}/${CORE_LIB}),${Q}rm ${LIB_DIR}/${CORE_LIB})
	$(if $(wildcard ${LIB_DIR}/${CORE_LIB}*),${Q}ls ${LIB_DIR}/${CORE_LIB}.* 2> /dev/null || :)

export __COMMON_VER__ ?= $(strip $(shell ${MAKE} -s -C ${COMMON_DIR} revision | grep -i '^Revision:' | awk -F : '{ print $$2 }'))
C_DEFINES += -D__COMMON_VER__='"${__COMMON_VER__}"' -DCOMMPROTO_LITTLE_ENDIAN
CXX_DEFINES += -D__COMMON_VER__='"${__COMMON_VER__}"' -DCOMMPROTO_LITTLE_ENDIAN \
                -DNEED_OS_SIGNALS -DHAS_CONFIG_FILE \
                -DHAS_LOGGER -DUSE_CUSTOM_FMT_LOG -DCUSTOM_FMT_LOG_H='"fmt_log.hpp"'
C_INCLUDES += -I${COMMON_DIR}
CXX_INCLUDES += -I${COMMON_DIR} $(shell pkg-config --cflags opencv4)
OTHER_CFLAGS +=
OTHER_CXXFLAGS +=
C_LDFLAGS +=
CXX_LDFLAGS += $(shell pkg-config --libs opencv4) -ljsoncpp -llz4 -lz

${COMMON_C_SRCS} ${COMMON_CXX_SRCS}: %: ${COMMON_DIR}/%
	$(if ${Q},@printf 'LINK\t$<\n')
	${Q}ln -s $< $@

camera_v4l2.o: camera_v4l2.c
	$(if ${Q},@printf 'CXX\t$<\n')
	${Q}${CXX_COMPILE}

endif

export DEPENDENCY_DIRS ?= $(abspath ../3rdparty ${COMMON_DIR})

dependencies:
	@for i in ${DEPENDENCY_DIRS}; \
	do \
		[ -s $${i}/[Mm]akefile ] && ${MAKE} $(filter all prepare, ${MAKECMDGOALS}) -C $${i} || true; \
	done

