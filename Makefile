# GNU Make + MinGW-w64 (MSYS2)
#   make                  # optimized Release, build/gcc/Release/trackpoint.exe
#   make debug            # unoptimized Debug, build/gcc/Debug/trackpoint.exe
#   make clean            # remove GCC build outputs
# Use the compiler matching the installed Synaptics COM server (x86 or x64).

CC = gcc
CONFIG ?= Release
SOURCE := trackfix.c
OUT_DIR := build/gcc/$(CONFIG)
TARGET := $(OUT_DIR)/trackpoint.exe

CPPFLAGS ?=
CFLAGS ?= -std=c11 -Wall -Wextra -Werror
LDFLAGS ?=
LDLIBS ?= -lole32 -loleaut32 -luuid -luser32

ifeq ($(CONFIG),Debug)
    CFLAGS += -O0 -g
else ifeq ($(CONFIG),Release)
    CFLAGS += -O2
else
    $(error Unsupported CONFIG '$(CONFIG)'; use Debug or Release)
endif

.DEFAULT_GOAL := all
.PHONY: all release debug clean

all: $(TARGET)

release:
	$(MAKE) CONFIG=Release all

debug:
	$(MAKE) CONFIG=Debug all

$(TARGET): $(SOURCE) | $(OUT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCE) -o $@ $(LDFLAGS) $(LDLIBS)

$(OUT_DIR):
	mkdir -p $@

clean:
	rm -rf build/gcc
