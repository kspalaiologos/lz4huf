ACLOCAL_AMFLAGS = -I build-aux

AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/lz4 -I$(top_srcdir)/huff0 $(OPENMP_CFLAGS) -fvisibility=hidden

EXTRA_DIST = LICENSE lz4/LICENSE huff0/LICENSE README.md build-aux/git-version-gen

pkgconfig_DATA = lz4huf.pc

include_HEADERS = include/liblz4huf.h
noinst_HEADERS = include/getopt-shim.h

lib_LTLIBRARIES = liblz4huf.la
liblz4huf_la_SOURCES = src/liblz4huf.c huff0/entropy_common.c huff0/debug.c huff0/hist.c huff0/huf_compress.c huff0/huf_decompress.c huff0/fse_compress.c huff0/fse_decompress.c lz4/lz4.c lz4/lz4hc.c
liblz4huf_la_LDFLAGS = -no-undefined -version-info 0:0:0

bin_PROGRAMS = lz4huf
lz4huf_CFLAGS = $(AM_CFLAGS)
lz4huf_SOURCES = src/lz4huf-cli.c
lz4huf_LDADD = liblz4huf.la

CLEANFILES = $(bin_PROGRAMS)

# End standard generic autotools stuff

# Begin special handling for autoconf VERSION being updated on commit

BUILT_SOURCES = .version
CLEANFILES += $(BUILT_SOURCES) .version-prev

_BRANCH_REF != $(AWK) '{print ".git/" $$2}' .git/HEAD 2>/dev/null ||:

.version: $(_BRANCH_REF)
	@if [ -e "$(srcdir)/.tarball-version" ]; then \
		printf "$(VERSION)" > $@; \
	else \
		touch "$@-prev"; \
		if [ -e "$@" ]; then \
			cp "$@" "$@-prev"; \
		fi; \
		./build-aux/git-version-gen "$(srcdir)/.tarball-version" > $@; \
		cmp -s "$@" "$@-prev" || autoreconf configure.ac --force; \
	fi

dist-hook:
	printf "$(VERSION)" > "$(distdir)/.tarball-version"

# Begin developer convenience targets

.PHONY: format
format: $(lz4huf_SOURCES) $(liblz4huf_la_SOURCES) $(include_HEADERS)
	clang-format -i $^

.PHONY: cloc
cloc: $(lz4huf_SOURCES) $(liblz4huf_la_SOURCES) $(include_HEADERS)
	cloc $^
