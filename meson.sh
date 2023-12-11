#!/bin/sh

cat > Makefile <<EOF

quiet_build =
quiet_reconfigure =
Q =
VERBOSE =

ifeq ("\$(origin V)", "command line")
  VERBOSE = \$(V)
endif

ifneq (\$(findstring 0, \$(VERBOSE)),)
  quiet_build = --quiet
  Q = @
endif

.PHONY: default docs
default: all

build/build.ninja:
	\$(Q)mkdir -p build
	\$(Q)meson build \$(quiet_build)

all: build/build.ninja
	\$(Q)ninja -C build \$(quiet_build)

clean: build/build.ninja
	\$(Q)ninja -C build clean \$(quiet_build)

test: build/build.ninja
	\$(Q)ninja -C build test \$(quiet_build)

reconfigure: build/build.ninja
	\$(Q)ninja -C build reconfigure

check distcheck dist distclean:
	@echo "This is the meson wrapper, not automake" && false

install: build/build.ninja
	\$(Q)ninja -C build install \$(quiet_build)

uninstall: build/build.ninja
	\$(Q)ninja -C build uninstall \$(quiet_build)

docs:
	\$(Q)ninja -C build igt-gpu-tools-doc \$(quiet_build)

EOF

git config format.subjectprefix "PATCH i-g-t"

make $@
