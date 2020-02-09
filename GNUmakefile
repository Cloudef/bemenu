VERSION ?= $(shell cat VERSION)
PREFIX ?= /usr/local
bindir ?= /bin
libdir ?= /lib
mandir ?= /share/man/man1

GIT_SHA1 = $(shell git rev-parse HEAD 2>/dev/null || printf 'nogit')
MAKEFLAGS += --no-builtin-rules

WARNINGS = -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-aliasing=3 -Wstrict-overflow=5 -Wstack-usage=12500 \
	-Wfloat-equal -Wcast-align -Wpointer-arith -Wchar-subscripts -Warray-bounds=2 -Wno-unknown-warning-option

override CFLAGS ?= -g -O2 $(WARNINGS)
override CFLAGS += -std=c99
override CPPFLAGS ?= -D_FORTIFY_SOURCE=2
override CPPFLAGS += -DBM_VERSION=\"$(VERSION)\" -DBM_PLUGIN_VERSION=\"$(VERSION)-$(GIT_SHA1)\" -DINSTALL_LIBDIR=\"$(PREFIX)$(libdir)\"
override CPPFLAGS += -D_DEFAULT_SOURCE -Ilib

libs = libbemenu.so
pkgconfigs = bemenu.pc
bins = bemenu bemenu-run
renderers = bemenu-renderer-x11.so bemenu-renderer-curses.so bemenu-renderer-wayland.so
all: $(bins) $(renderers)
clients: $(bins)
curses: bemenu-renderer-curses.so
x11: bemenu-renderer-x11.so
wayland: bemenu-renderer-wayland.so

# support non git builds
.git/index:
	mkdir -p .git
	touch .git/index

%.a:
	$(LINK.c) -c $(filter %.c,$^) $(LDLIBS) -o $@

$(libs): %: VERSION .git/index
	$(LINK.c) -shared -fPIC $(filter %.c %.a,$^) $(LDLIBS) -o $(addsuffix .$(VERSION), $@)
	ln -fs $(addsuffix .$(VERSION), $@) $(addsuffix .$(firstword $(subst ., ,$(VERSION))), $@)
	ln -fs $(addsuffix .$(VERSION), $@) $@

$(pkgconfigs): %: VERSION %.in
	sed "s/@VERSION@/$(VERSION)/;s,@PREFIX@,$(PREFIX),;s,@LIBDIR@,$(libdir)," $(addsuffix .in, $@) > $@

$(renderers): %: VERSION .git/index | $(libs)
	$(LINK.c) -shared -fPIC $(filter %.c %.a,$^) $(LDLIBS) -L. -lbemenu -o $@

$(bins): %: | $(libs)
	$(LINK.c) $(filter %.c %.a,$^) $(LDLIBS) -L. -lbemenu -o $@

cdl.a: lib/3rdparty/cdl.c lib/3rdparty/cdl.h

libbemenu.so: private override LDLIBS += -ldl
libbemenu.so: lib/bemenu.h lib/internal.h lib/filter.c lib/item.c lib/library.c lib/list.c lib/menu.c lib/util.c cdl.a

bemenu-renderer-curses.so: private override LDLIBS += $(shell pkg-config --libs ncursesw) -lm
bemenu-renderer-curses.so: private override CPPFLAGS += $(shell pkg-config --cflags-only-I ncursesw)
bemenu-renderer-curses.so: lib/renderers/curses/curses.c

bemenu-renderer-x11.so: private override LDLIBS += $(shell pkg-config --libs x11 xinerama cairo pango pangocairo)
bemenu-renderer-x11.so: private override CPPFLAGS += $(shell pkg-config --cflags-only-I x11 xinerama cairo pango pangocairo)
bemenu-renderer-x11.so: lib/renderers/cairo.h lib/renderers/x11/x11.c lib/renderers/x11/x11.h lib/renderers/x11/window.c lib/renderers/x11/xkb_unicode.c lib/renderers/x11/xkb_unicode.h

lib/renderers/wayland/xdg-shell.c:
	wayland-scanner private-code < "$$(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml" > $@

lib/renderers/wayland/wlr-layer-shell-unstable-v1.h: lib/renderers/wayland/wlr-layer-shell-unstable-v1.xml
	wayland-scanner client-header < $^ > $@

lib/renderers/wayland/wlr-layer-shell-unstable-v1.c: lib/renderers/wayland/wlr-layer-shell-unstable-v1.xml
	wayland-scanner private-code < $^ > $@

xdg-shell.a: lib/renderers/wayland/xdg-shell.c
wlr-layer-shell.a: lib/renderers/wayland/wlr-layer-shell-unstable-v1.c lib/renderers/wayland/wlr-layer-shell-unstable-v1.h
bemenu-renderer-wayland.so: private override LDLIBS += $(shell pkg-config --libs wayland-client cairo pango pangocairo xkbcommon)
bemenu-renderer-wayland.so: private override CPPFLAGS += $(shell pkg-config --cflags-only-I wayland-client cairo pango pangocairo xkbcommon)
bemenu-renderer-wayland.so: lib/renderers/cairo.h lib/renderers/wayland/wayland.c lib/renderers/wayland/wayland.h lib/renderers/wayland/registry.c lib/renderers/wayland/window.c xdg-shell.a wlr-layer-shell.a

common.a: client/common/common.c client/common/common.h
bemenu: common.a client/bemenu.c
bemenu-run: common.a client/bemenu-run.c

install-pkgconfig: $(pkgconfigs)
	mkdir -p "$(DESTDIR)$(PREFIX)$(libdir)/pkgconfig"
	cp $^ "$(DESTDIR)$(PREFIX)$(libdir)/pkgconfig"

install-libs: $(libs)
	mkdir -p "$(DESTDIR)$(PREFIX)$(libdir)"
	cp $(addsuffix .$(VERSION), $^) "$(DESTDIR)$(PREFIX)$(libdir)"

install-lib-symlinks: $(libs) | install-libs
	cp -RP $^ $(addsuffix .$(firstword $(subst ., ,$(VERSION))), $^) "$(DESTDIR)$(PREFIX)$(libdir)"

install-renderers:
	mkdir -p "$(DESTDIR)$(PREFIX)$(libdir)/bemenu"
	-cp $(renderers) "$(DESTDIR)$(PREFIX)$(libdir)/bemenu"

install-bins:
	mkdir -p "$(DESTDIR)$(PREFIX)$(bindir)"
	-cp $(bins) "$(DESTDIR)$(PREFIX)$(bindir)"
	-chmod 0755 "$(DESTDIR)$(PREFIX)$(bindir)"/*

install-man: man/bemenu.1 man/bemenu-run.1
	mkdir -p "$(DESTDIR)$(PREFIX)$(mandir)"
	cp $^ "$(DESTDIR)$(PREFIX)$(mandir)"

install: install-pkgconfig install-lib-symlinks install-renderers install-bins install-man
	@echo "Install OK!"

doxygen:
	BM_VERSION=$(VERSION) doxygen doxygen/Doxyfile
	cp -R doxygen/doxygen-qmi-style/navtree html
	cp -R doxygen/doxygen-qmi-style/search html/search

clean:
	$(RM) -r *.dSYM # OSX generates .dSYM dirs with -g ...
	$(RM) $(pkgconfigs) $(libs) $(bins) $(renderers) *.a *.so.*
	$(RM) lib/renderers/wayland/wlr-*.h lib/renderers/wayland/wlr-*.c lib/renderers/wayland/xdg-shell.c
	$(RM) -r html

.DELETE_ON_ERROR:
.PHONY: all clean install install-pkgconfig install-libs install-lib-symlinks install-man install-bins install-renderers doxygen clients curses x11 wayland
