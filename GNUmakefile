VERSION ?= 0.3.0
PREFIX ?= /usr/local
bindir ?= /bin
libdir ?= /lib
mandir ?= /share/man/man1

MAKEFLAGS += --no-builtin-rules

WARNINGS = -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-aliasing=3 -Wstrict-overflow=5 -Wstack-usage=12500 \
	-Wfloat-equal -Wcast-align -Wpointer-arith -Wchar-subscripts -Warray-bounds=2

override CFLAGS ?= -g -O2 $(WARNINGS)
override CFLAGS += -std=c99
override CPPFLAGS ?= -D_FORTIFY_SOURCE=2 -D_DEFAULT_SOURCE
override CPPFLAGS += -DBM_VERSION=\"$(VERSION)\" -DBM_PLUGIN_VERSION=\"$(VERSION)-$(GIT_SHA1)\" -DINSTALL_LIBDIR=\"$(PREFIX)$(libdir)\"
override CPPFLAGS += -Ilib

libs = libbemenu.so
bins = bemenu bemenu-run
renderers = bemenu-renderer-x11.so bemenu-renderer-curses.so bemenu-renderer-wayland.so
all: $(bins) $(renderers)
clients: $(bins)
curses: bemenu-renderer-curses.so
x11: bemenu-renderer-x11.so
wayland: bemenu-renderer-wayland.so

%.a:
	$(LINK.c) -c $(filter %.c,$^) $(LDLIBS) -o $@

$(libs): %:
	$(LINK.c) -shared -fPIC $(filter %.c %.a,$^) $(LDLIBS) -o $(addsuffix .$(VERSION), $@)
	ln -fs $(addsuffix .$(VERSION), $@) $(addsuffix .$(firstword $(subst ., ,$(VERSION))), $@)
	ln -fs $(addsuffix .$(VERSION), $@) $@

$(renderers): %: | $(libs)
	$(LINK.c) -shared -fPIC $(filter %.c %.a,$^) $(LDLIBS) -L. -lbemenu -o $@

$(bins): %: | $(libs)
	$(LINK.c) $(filter %.c %.a,$^) $(LDLIBS) -L. -lbemenu -o $@

cdl.a: lib/3rdparty/cdl.c lib/3rdparty/cdl.h

libbemenu.so: private override LDLIBS += -ldl
libbemenu.so: lib/bemenu.h lib/internal.h lib/filter.c lib/item.c lib/library.c lib/list.c lib/menu.c lib/util.c cdl.a

bemenu-renderer-curses.so: private override LDLIBS += `pkg-config --libs ncurses`
bemenu-renderer-curses.so: private override CPPFLAGS += `pkg-config --cflags-only-I ncurses`
bemenu-renderer-curses.so: lib/renderers/curses/curses.c

bemenu-renderer-x11.so: private override LDLIBS += `pkg-config --libs x11 cairo pango`
bemenu-renderer-x11.so: private override CPPFLAGS += `pkg-config --cflags-only-I x11 cairo pango`
bemenu-renderer-x11.so: lib/renderers/cairo.h lib/renderers/x11/x11.c lib/renderers/x11/x11.h lib/renderers/x11/window.c lib/renderers/x11/xkb_unicode.c lib/renderers/x11/xkb_unicode.h

lib/renderers/wayland/wlr-layer-shell-unstable-v1.h:
	wayland-scanner client-header < $(subst .h,.xml,$@) > $@

lib/renderers/wayland/wlr-layer-shell-unstable-v1.c:
	wayland-scanner code < $(subst .c,.xml,$@) > $@

wlr-layer-shell.a: lib/renderers/wayland/wlr-layer-shell-unstable-v1.c lib/renderers/wayland/wlr-layer-shell-unstable-v1.h
bemenu-renderer-wayland.so: private override LDLIBS += `pkg-config --libs wayland-client cairo pango xkbcommon`
bemenu-renderer-wayland.so: private override CPPFLAGS += `pkg-config --cflags-only-I wayland-client cairo pango xkbcommon`
bemenu-renderer-wayland.so: lib/renderers/cairo.h lib/renderers/wayland/wayland.c lib/renderers/wayland/wayland.h lib/renderers/wayland/registry.c lib/renderers/wayland/window.c wlr-layer-shell.a

common.a: client/common/common.c client/common/common.h
bemenu: common.a client/bemenu.c
bemenu-run: common.a client/bemenu-run.c

install-libs: $(libs)
	install -Dm755 $(addsuffix .$(VERSION), $^) -t "$(DESTDIR)$(PREFIX)$(libdir)"

install-lib-symlinks: $(libs) | install-libs
	 cp -P $^ $(addsuffix .$(firstword $(subst ., ,$(VERSION))), $^) "$(DESTDIR)$(PREFIX)$(libdir)"

install-renderers:
	install -Dm755 $(renderers) -t "$(DESTDIR)$(PREFIX)$(libdir)/bemenu" || true

install-bins:
	install -Dm755 $(bins) -t "$(DESTDIR)$(PREFIX)$(bindir)" || true

install-man: man/bemenu.1 man/bemenu-run.1
	install -Dm644 $^ -t "$(DESTDIR)$(PREFIX)$(mandir)"

install: install-lib-symlinks install-renderers install-bins install-man
	@echo "Install OK!"

doxygen:
	BM_VERSION=$(VERSION) doxygen doxygen/Doxyfile
	cp -r doxygen/doxygen-qmi-style/navtree html
	cp -r doxygen/doxygen-qmi-style/search html/search

clean:
	$(RM) $(libs) $(bins) $(renderers) *.a *.so.*
	$(RM) lib/renderers/wayland/wlr-*.h lib/renderers/wayland/wlr-*.c
	$(RM) -r html

.PHONY: all clean install install-libs install-lib-symlinks install-man install-bins install-renderers doxygen clients curses x11 wayland
