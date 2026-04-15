# SPDX-License-Identifier: Apache-2.0

#
# Copyright (c) 2026 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#

#SRC_DIR ?= .
SRC_DIR ?= src

override undefine __MODULES
MODULES_HARDCODED ?= 0
ifeq ($(if $(strip $(filter-out n N no NO No 0, ${MODULES_HARDCODED})),1,),1)
    __MODULES := server client
else
    override undefine __PREFIX_MATCHED_ITEMS __EXACTLY_MATCHED_ITEMS
    __PREFIX_MATCHED_ITEMS := \. _ lib[36x]*
    __SUFFIX_MATCHED_ITEMS := [._-]priv [._-]private
    __EXACTLY_MATCHED_ITEMS := bin etc conf config[s]* doc[s]* include[s]* log[s]* obj[s]* t[e]*mp test[s]* \
                               build cache common public private
    __PREFIX_MATCHED_ITEMS := ^$(shell echo '${__PREFIX_MATCHED_ITEMS}' | sed 's/ /\\|^/g')
    __SUFFIX_MATCHED_ITEMS := $(shell echo '${__SUFFIX_MATCHED_ITEMS}' | sed 's/ /$$\\|/g')$$
    __EXACTLY_MATCHED_ITEMS := ^$(shell echo '${__EXACTLY_MATCHED_ITEMS}' | sed 's/ /$$\\|^/g')$$
    __MODULES := $(shell ls ${SRC_DIR} | grep -i -v '${__PREFIX_MATCHED_ITEMS}\|${__SUFFIX_MATCHED_ITEMS}\|${__EXACTLY_MATCHED_ITEMS}')
endif

override undefine __OTHER_TARGETS
__OTHER_TARGETS := prepare clean distclean dist install uninstall check test

.PHONY: all help list-modules ${__MODULES} ${__OTHER_TARGETS}
.PHONY: $(foreach i, ${__OTHER_TARGETS}, $(foreach j, ${__MODULES}, ${i}-${j}))

export NDEBUG O V VERBOSE

# Q is short for "quiet".
Q := $(if $(strip $(filter-out n N no NO No 0, ${V} ${VERBOSE})),,@)

all: ${__MODULES}

${__MODULES}: %:
	${Q}${MAKE} -C ${SRC_DIR}/$@

$(foreach i, ${__OTHER_TARGETS}, $(eval ${i}: $(foreach j, ${__MODULES}, ${i}-${j})))
# Can NOT work:
#$(foreach i, ${__OTHER_TARGETS}, ${i}): $(foreach j, ${__MODULES}, %-${j})

define modular_target_rule
$(strip ${2})-$(strip ${1}):
	${Q}${MAKE} -C ${SRC_DIR}/$(strip ${1}) $(strip ${2})
endef

$(foreach i, ${__OTHER_TARGETS}, \
	$(foreach j, ${__MODULES}, \
		$(eval $(call modular_target_rule, ${j}, ${i})) \
	) \
)

help:
	@echo "Available commands:"
	@echo "==================="
	@echo "  all               - Build all modules."
	@echo "  <module>          - Build the specified module only."
	@echo "                      See also: list-modules"
	@echo "  list-modules      - Show available module names."
	@echo "--------"
	@echo "  prepare           - Make some preparations first."
	@echo "  clean             - Remove [MOST] generated files and directories."
	@echo "  distclean         - Remove [ALL] generated files and directories."
	@echo "  dist              - Make a distribution package for product release."
	@echo "  install           - Install all modules."
	@echo "  uninstall         - Uninstall all modules."
	@echo "  check             - Perform some checkings, e.g. code static check."
	@echo "  test              - Run customized tests."
	@echo "  *-<module>        - Perform one of operations above on a module."
	@echo "                      See also: list-modules"
	@if [ -e rsync.priv.mk ]; then \
		echo "--------"; \
		echo "  sync_to_*         - Synchronize project contents to remote side"; \
		echo "                      depending on your own rsync.priv.mk."; \
	fi

list-modules:
	$(foreach i, ${__MODULES}, $(info ${i}))
	@:
  

-include rsync.priv.mk

