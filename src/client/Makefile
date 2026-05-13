# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

.PHONY: all prepare dependencies distclean

LAZY_CODING_URL ?= https://github.com/FooFooDamon/lazy_coding_skills
COMMON_DIR := ../common

override undefine PREREQUISITE_FILES
PREREQUISITE_FILES := ${LAZY_CODING_URL}/raw/main/makefiles/__ver__.mk \
    ${LAZY_CODING_URL}/raw/01c4299d4858189ba7072789ff2a6842ecbb44d1/c_and_cpp/qt/qt_print.hpp \
    ${LAZY_CODING_URL}/raw/1967505fc56bce02022d7e896554e5c406c2c57b/c_and_cpp/qt/QPlayerSlider.cpp \
    ${LAZY_CODING_URL}/raw/1967505fc56bce02022d7e896554e5c406c2c57b/c_and_cpp/qt/QPlayerSlider.hpp \
    ${LAZY_CODING_URL}/raw/1967505fc56bce02022d7e896554e5c406c2c57b/c_and_cpp/qt/QVideoCanvas.cpp \
    ${LAZY_CODING_URL}/raw/1967505fc56bce02022d7e896554e5c406c2c57b/c_and_cpp/qt/QVideoCanvas.hpp \

override COMMON_C_SRCS := signal_handling.c communication_protocol.c
override COMMON_CXX_SRCS := cmdline_args.cpp config_file.cpp biz_protocols.cpp biz_save.cpp

ifeq ($(shell [ -s QtMakefile $(foreach i, $(notdir ${PREREQUISITE_FILES}) ${COMMON_C_SRCS} ${COMMON_CXX_SRCS}, -a -s ${i}) ] && echo 1 || echo 0),0)

all prepare: dependencies ${COMMON_C_SRCS} ${COMMON_CXX_SRCS}
	@for i in ${PREREQUISITE_FILES}; \
	do \
		[ -s $$(basename $${i}) ] || wget -c -O $$(basename $${i}) "$${i}"; \
	done
	@[ -s QtMakefile ] || qmake -o QtMakefile
	@echo "~ ~ ~ Minimum preparation finished successfully ~ ~ ~"
	@echo "Re-run your command again to continue your work."

${COMMON_C_SRCS} ${COMMON_CXX_SRCS}: %: ${COMMON_DIR}/%
	ln -s $< $@

else

ifeq (${MAKELEVEL}, 0)

.PHONY: fast

fast:
	${MAKE} -j $$(nproc)

endif

all: dependencies

include __ver__.mk
include QtMakefile

override EXE_DIR := ${HOME}/bin
override LIB_DIR := ${HOME}/lib
override CORE_LIB := liblancc_core.so
override CUST_LIB := $(wildcard ${LIB_DIR}/liblancc_cust.so)
__COMMON_VER__ ?= $(strip $(shell ${MAKE} -s -C ${COMMON_DIR} revision | grep -i '^Revision:' | awk -F : '{ print $$2 }'))
DEFINES += -D__VER__='"${__VER__}"' -D__COMMON_VER__='"${__COMMON_VER__}"' -DCOMMPROTO_LITTLE_ENDIAN \
            -DNEED_OS_SIGNALS -DHAS_CONFIG_FILE \
            -DHAS_LOGGER -DUSE_CUSTOM_FMT_LOG -DCUSTOM_FMT_LOG_H='"fmt_log.hpp"' \
            $(if ${FMT_LOG_IMPL_H}, -DFMT_LOG_IMPL_H='"${FMT_LOG_IMPL_H}"') \
            $(if $(strip $(filter-out n N no NO No 0, ${DISABLE_OPENGL_SHADERS})),-DDISABLE_OPENGL_SHADERS)
CFLAGS += -Wno-unused-parameter
CXXFLAGS += -Wno-unused-parameter
INCPATH += -I${COMMON_DIR} $(shell pkg-config --cflags opencv4)
LFLAGS +=
LIBS += $(shell pkg-config --libs opencv4) -ljsoncpp -llz4 -lz
NO_CPPCHECK = true
OS_MACRO ?= -D__linux__
PREDEFS_FOR_CPPCHECK ?= $(if $(wildcard moc_predefs.h), --include=moc_predefs.h, \
    ${OS_MACRO} $$(g++ -dM -E - < /dev/null | grep ENDIAN | awk '{ printf("-D%s=%s\n", $$2, $$3) }'))

ifneq (${CUST_LIB},) # Found customization.

CORE_OBJS := $(filter-out main.o, ${OBJECTS})
OBJECTS = main.o ${CORE_LIB}

# WEIRD BUG: This dependency has to be declared explicitly even though the library is already in OBJECTS!
${TARGET}: ${CORE_LIB}

#
# All weak symbols must be packed into a shared library
# so that they can be overridden by the ones in ${CUST_LIB}.
#
# Also be aware that ${CUST_LIB} should be preloaded
# by setting LD_PRELOAD environment variable
# before starting ${TARGET}.
#
${CORE_LIB}: ${CORE_OBJS}
	${CXX} -shared -o $@ $^
	if [ $$(echo "${DEFINES}" | grep -c "QT_NO_DEBUG") -gt 0 ]; then \
		objcopy --only-keep-debug $@ $@.dbgi; \
		${STRIP} --strip-debug $@; \
		objcopy --add-gnu-debuglink=$@.dbgi $@; \
	fi

endif

cmdline_args.o: DEFINES += -DBIZ_TYPE_CANDIDATES='"client,player"' -DBIZ_TYPE_DEFAULT='"client"' \
                -DBRIEF_INTRO='"Camera Client running on Local Area Network"'
signal_handling.o: DEFINES += -U__STRICT_ANSI__

.PHONY: .ALWAYS_MAKE install_debug_info install uninstall ext_clean check debug_patch ui_fix

versions.h: .revision
	@touch $@

.revision: .ALWAYS_MAKE
	@[ -e $@ ] || touch $@
	@[ '$(file < $@)' = '${__VER__}|${__COMMON_VER__}' ] || printf '${__VER__}|${__COMMON_VER__}' > $@

.ALWAYS_MAKE:

MAIN_VER ?= $(shell grep "#define [A-Z_]*_VER[[:blank:]]" versions.h | awk '{ print $$3 }' | sed 'N;N; s/\n/./g')
FULL_VER ?= ${MAIN_VER}.${__VER__}.${__COMMON_VER__}
DEBUG_DIR ?= ${HOME}/debug/${TARGET}/${MAIN_VER}/${__VER__}.${__COMMON_VER__}
override INSTALL := install -D --compare --backup=numbered
override REALPATH := realpath -m
override LS := ls -h -g -o
override OLD_VER_WARN := awk '{ print $$3, $$4, $$5, $$6, $$7 }' | xargs -I {} echo "*** Found old version: {}"
override DEBUG_WARN := printf "\033[0;33m*** It's STRONGLY RECOMMENDED to move this file to another private host for safety! ***\033[0m\n"

install_debug_info:
	$(if $(wildcard ${CORE_LIB}.dbgi),${INSTALL} --backup=off ${CORE_LIB}.dbgi ${DEBUG_DIR}/${CORE_LIB}.dbgi)
	$(if $(wildcard ${CORE_LIB}.dbgi),@${DEBUG_WARN})
	$(if $(wildcard ${TARGET}.dbgi),${INSTALL} --backup=off ${TARGET}.dbgi ${DEBUG_DIR}/${TARGET}.dbgi)
	$(if $(wildcard ${TARGET}.dbgi),@${DEBUG_WARN})

install: ${TARGET} install_debug_info
	$(if $(wildcard ${CORE_LIB}),${INSTALL} ${CORE_LIB} ${LIB_DIR}/${CORE_LIB}.${FULL_VER})
	$(if $(wildcard ${CORE_LIB}),cd ${LIB_DIR} && ${SYMLINK} ${CORE_LIB}.${FULL_VER} ${CORE_LIB})
	$(if $(wildcard ${CORE_LIB}),${LS} ${LIB_DIR}/${CORE_LIB}.${FULL_VER}.* 2> /dev/null | ${OLD_VER_WARN})
	${INSTALL} ${TARGET} ${EXE_DIR}/${TARGET}.${FULL_VER}
	cd ${EXE_DIR} && ${SYMLINK} ${TARGET}.${FULL_VER} ${TARGET}
	${LS} ${EXE_DIR}/${TARGET}.${FULL_VER}.* 2> /dev/null | ${OLD_VER_WARN}

uninstall:
	${LS} $(shell ${REALPATH} ${EXE_DIR}/${TARGET}).* 2> /dev/null | ${OLD_VER_WARN}
	${DEL_FILE} ${EXE_DIR}/${TARGET} $(shell ${REALPATH} ${EXE_DIR}/${TARGET})
	$(if $(wildcard ${LIB_DIR}/${CORE_LIB}*),${LS} $(shell ${REALPATH} ${LIB_DIR}/${CORE_LIB}).* 2> /dev/null | ${OLD_VER_WARN})
	$(if $(wildcard ${LIB_DIR}/${CORE_LIB}),${DEL_FILE} ${LIB_DIR}/${CORE_LIB} $(shell ${REALPATH} ${LIB_DIR}/${CORE_LIB}))

download_clean:
	${DEL_FILE} $(notdir ${PREREQUISITE_FILES} ${COMMON_C_SRCS} ${COMMON_CXX_SRCS})

distclean: download_clean dependencies

clean: ext_clean

ext_clean:
	${DEL_FILE} ${TARGET} *.d *.plist *.dbgi $(if ${CUST_LIB}, ${CORE_LIB} *.o)

check:
	-${NO_CPPCHECK} \
		|| cppcheck --quiet --force --enable=all --suppress=missingIncludeSystem \
		-j $$(nproc) --language=c --std=c11 \
		${PREDEFS_FOR_CPPCHECK} \
		${DEFINES} ${INCPATH} $(filter %.c, ${SOURCES})
	-${NO_CPPCHECK} && printf "\n\033[0;33m[Warning] Cppcheck has been disabled since it consumes too much time!\033[0m\n%s\n\n" \
		"If you want to enable it, run with NO_CPPCHECK=false" >&2 \
		|| cppcheck --quiet --force --enable=all --suppress=missingIncludeSystem \
		-j $$(nproc) --language=c++ --std=c++11 \
		--library=qt ${PREDEFS_FOR_CPPCHECK} \
		${DEFINES} ${INCPATH} $(filter-out %.c moc_%.cpp, ${SOURCES})
	clang --analyze $(filter %.c, ${SOURCES}) ${CFLAGS} ${INCPATH}
	clang --analyze $(filter-out %.c moc_%.cpp, ${SOURCES}) ${CXXFLAGS} ${INCPATH}

debug_patch:
	@if [ -s ${TARGET}.debug.pri ]; then \
		echo "${TARGET}.debug.pri already exists." >&2; \
	else \
		echo "CONFIG += debug" > ${TARGET}.debug.pri; \
		${MAKE} distclean; \
		printf "\n~ ~ ~ ${TARGET}.debug.pri was generated successfully. ~ ~ ~\n\nYou can re-build the project now.\n\n"; \
	fi

ui_fix: ui_${TARGET}.h
	${SED} -i 's/\(.*\<QPalette::PlaceholderText\>.*\)/\/\/\1/' $<

endif

DEPENDENCY_DIRS ?= $(abspath ../3rdparty ${COMMON_DIR})

dependencies:
	@for i in ${DEPENDENCY_DIRS}; \
	do \
		[ -s $${i}/[Mm]akefile ] || continue; \
		${MAKE} -C $${i} $(filter all prepare distclean, ${MAKECMDGOALS}); \
	done

