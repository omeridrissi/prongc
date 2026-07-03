bin_PROGRAMS = prongc

prongc_SOURCES = \
	src/prong.c \
	src/core.c \
	src/func_info.c \
	src/cursor_arr.c \
	src/dyn_aos.c \
	src/log.c \
	src/var_access.c \
	src/prong_thread_pool.c \
	src/lock_tracking.c

prongc_CFLAGS = $(PRONGC_CFLAGS) $(LIBCLANG_CPPFLAGS) -I$(srcdir)/inc
prongc_LDFLAGS = $(LIBCLANG_LDFLAGS)
prongc_LDADD = $(LIBCLANG_LIBS)

dist-hook:
	find $(distdir) -name .DS_Store -exec rm -f {} \;
