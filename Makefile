# TrackFix - GNU Make with MinGW-w64 GCC (Windows)
#   make                 Build trackfix.exe (Release)
#   make debug           Build trackfix.exe (Debug)
#   make release         Build trackfix.exe (Release)
#   make CONFIG=Debug    Build trackfix.exe (Debug)
#   make clean           Remove trackfix.exe
#

CC := gcc
CONFIG ?= Release
CPPFLAGS :=
CFLAGS := -std=c11 -Wall -Wextra -Werror
LDFLAGS :=
LDLIBS := -lole32 -loleaut32 -luuid -luser32
OPTFLAGS = $(if $(filter Debug,$(CONFIG)),-O0 -g,-O2)

.DEFAULT_GOAL := all
.PHONY: all debug release clean

all:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OPTFLAGS) trackfix.c -o trackfix.exe $(LDFLAGS) $(LDLIBS)

debug:
	$(MAKE) CONFIG=Debug all

release:
	$(MAKE) CONFIG=Release all

clean:
	cmd.exe /C "if exist trackfix.exe del /Q trackfix.exe"
