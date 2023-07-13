# CFLAGS ?= -O2 -Wall -Wextra -arch arm64 -arch x86_64
CFLAGS ?= -O0 -Wall -Wextra -g3

ifmtuset: ifmtuset.c Makefile
	$(CC) $(CFLAGS) -o $@ $<
