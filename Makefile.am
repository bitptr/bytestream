AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = -std=c99 -Wall -Wextra -pedantic-errors -Werror -DGSEAL_ENABLE \
						-Wno-unused-parameter
AM_CPPFLAGS = $(GTK_CFLAGS)
AM_LDFLAGS = $(GTK_LIBS)

bin_PROGRAMS = src/bytestream
dist_man_MANS = man/bytestream.1

dist_src_bytestream_SOURCES = src/main.c
