VERSION ?= $(shell cat VERSION)
PREFIX ?= /usr/local
includedir ?= /include
bindir ?= /bin
libdir ?= /lib
mandir ?= /share/man/man1
PKG_CONFIG ?= pkg-config

GIT_SHA1 = $(shell git rev-parse HEAD 2>/dev/null || printf 'nogit')
GIT_TAG = $(shell git tag --points-at HEAD 2>/dev/null || cat VERSION)
MAKEFLAGS += --no-builtin-rules

WARNINGS = -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-aliasing=3 -Wstrict-overflow=5 -Wstack-usage=12500 \
	-Wcast-align -Wpointer-arith -Wchar-subscripts -Warray-bounds=2 -Wno-unknown-warning-option
VISIBILITY ?= -fvisibility=hidden

override CFLAGS ?= -g -O2 $(WARNINGS) $(EXTRA_WARNINGS)
override CFLAGS += -std=c99 $(VISIBILITY)
override CPPFLAGS ?= -D_FORTIFY_SOURCE=2
override CPPFLAGS += -DBM_VERSION=\"$(VERSION)\" -DBM_PLUGIN_VERSION=\"$(VERSION)-$(GIT_SHA1)\" -DINSTALL_LIBDIR=\"$(PREFIX)$(libdir)\"
override CPPFLAGS += -D_DEFAULT_SOURCE -Ilib

libs = libbemenu.so
pkgconfigs = bemenu.pc
bins = bemenu bemenu-run
mans = bemenu.1
renderers = bemenu-renderer-x11.so bemenu-renderer-curses.so bemenu-renderer-wayland.so
all: $(bins) $(renderers) $(mans)
clients: $(bins)
curses: bemenu-renderer-curses.so
x11: bemenu-renderer-x11.so
wayland: bemenu-renderer-wayland.so

# support non git builds
.git/index:
	[ -f .git ] && exit; \
	mkdir -p .git; \
	touch .git/index

%.a:
	$(LINK.c) -c $(filter %.c,$^) $(LDLIBS) -o $@

$(libs): private override full=$(addsuffix .$(VERSION), $@)
$(libs): private override major=$(addsuffix .$(firstword $(subst ., ,$(VERSION))), $@)
$(libs): %: VERSION .git/index
	$(LINK.c) -shared -fPIC $(filter %.c %.a,$^) $(LDLIBS) -o $(full) -Wl,-soname,$(major)
	ln -fs $(full) $(major)
	ln -fs $(full) $@

$(pkgconfigs): %: VERSION %.in
	sed "s/@VERSION@/$(VERSION)/;s,@PREFIX@,$(PREFIX),;s,@LIBDIR@,$(libdir),;s,@INCLUDEDIR@,$(includedir)," $(addsuffix .in, $@) > $@

bemenu.1: man/bemenu.1.scd.in
	-sed 's,@LIBDIR@,$(PREFIX)$(libdir),' $< | scdoc > $@

$(renderers): %: VERSION .git/index | $(libs)
	$(LINK.c) -shared -fPIC $(filter %.c %.a,$^) $(LDLIBS) -L. -lbemenu -o $@

$(bins): %: | $(libs)
	$(LINK.c) $(filter %.c %.a,$^) $(LDLIBS) -L. -lbemenu -o $@

cdl.a: private override LDFLAGS += -fPIC
cdl.a: lib/3rdparty/cdl.c lib/3rdparty/cdl.h

util.a: private override LDFLAGS += -fPIC
util.a: lib/util.c lib/internal.h

libbemenu.so: private override LDLIBS += -ldl
libbemenu.so: lib/bemenu.h lib/internal.h lib/filter.c lib/item.c lib/library.c lib/list.c lib/menu.c lib/vim.c util.a cdl.a

bemenu-renderer-curses.so: private override LDLIBS += $(shell $(PKG_CONFIG) --libs ncursesw) -lm
bemenu-renderer-curses.so: private override CPPFLAGS += $(shell $(PKG_CONFIG) --cflags-only-I ncursesw)
bemenu-renderer-curses.so: lib/renderers/curses/curses.c util.a

bemenu-renderer-x11.so: private override LDLIBS += $(shell $(PKG_CONFIG) --libs x11 xinerama cairo pango pangocairo)
bemenu-renderer-x11.so: private override CPPFLAGS += $(shell $(PKG_CONFIG) --cflags-only-I x11 xinerama cairo pango pangocairo)
bemenu-renderer-x11.so: lib/renderers/cairo_renderer.h lib/renderers/x11/x11.c lib/renderers/x11/x11.h lib/renderers/x11/window.c lib/renderers/x11/xkb_unicode.c lib/renderers/x11/xkb_unicode.h util.a

lib/renderers/wayland/xdg-shell.c:
	wayland-scanner private-code < "$$($(PKG_CONFIG) --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml" > $@

lib/renderers/wayland/wlr-layer-shell-unstable-v1.h: lib/renderers/wayland/wlr-layer-shell-unstable-v1.xml
	wayland-scanner client-header < $^ > $@

lib/renderers/wayland/wlr-layer-shell-unstable-v1.c: lib/renderers/wayland/wlr-layer-shell-unstable-v1.xml
	wayland-scanner private-code < $^ > $@

lib/renderers/wayland/fractional-scale-v1.h: lib/renderers/wayland/fractional-scale-v1.xml
	wayland-scanner client-header < $^ > $@

lib/renderers/wayland/fractional-scale-v1.c: lib/renderers/wayland/fractional-scale-v1.xml
	wayland-scanner private-code < $^ > $@

lib/renderers/wayland/viewporter.h: lib/renderers/wayland/viewporter.xml
	wayland-scanner client-header < $^ > $@

lib/renderers/wayland/viewporter.c: lib/renderers/wayland/viewporter.xml
	wayland-scanner private-code < $^ > $@

xdg-shell.a: private override LDFLAGS += -fPIC
xdg-shell.a: private override CPPFLAGS += $(shell $(PKG_CONFIG) --cflags-only-I wayland-client)
xdg-shell.a: lib/renderers/wayland/xdg-shell.c
wlr-layer-shell.a: private override LDFLAGS += -fPIC
wlr-layer-shell.a: private override CPPFLAGS += $(shell $(PKG_CONFIG) --cflags-only-I wayland-client)
wlr-layer-shell.a: lib/renderers/wayland/wlr-layer-shell-unstable-v1.c lib/renderers/wayland/wlr-layer-shell-unstable-v1.h
fractional-scale.a: private override LDFLAGS += -fPIC
fractional-scale.a: private override CPPFLAGS += $(shell $(PKG_CONFIG) --cflags-only-I wayland-client)
fractional-scale.a: lib/renderers/wayland/fractional-scale-v1.c lib/renderers/wayland/fractional-scale-v1.h
viewporter.a: private override LDFLAGS += -fPIC
viewporter.a: private override CPPFLAGS += $(shell $(PKG_CONFIG) --cflags-only-I wayland-client)
viewporter.a: lib/renderers/wayland/viewporter.c lib/renderers/wayland/viewporter.h
bemenu-renderer-wayland.so: private override LDLIBS += $(shell $(PKG_CONFIG) --libs wayland-client cairo pango pangocairo xkbcommon)
bemenu-renderer-wayland.so: private override CPPFLAGS += $(shell $(PKG_CONFIG) --cflags-only-I wayland-client cairo pango pangocairo xkbcommon)
bemenu-renderer-wayland.so: lib/renderers/cairo_renderer.h lib/renderers/wayland/wayland.c lib/renderers/wayland/wayland.h lib/renderers/wayland/registry.c lib/renderers/wayland/window.c xdg-shell.a wlr-layer-shell.a fractional-scale.a viewporter.a util.a

common.a: client/common/common.c client/common/common.h
bemenu: common.a client/bemenu.c
bemenu-run: common.a client/bemenu-run.c

install-pkgconfig: $(pkgconfigs)
	mkdir -p "$(DESTDIR)$(PREFIX)$(libdir)/pkgconfig"
	cp $^ "$(DESTDIR)$(PREFIX)$(libdir)/pkgconfig"

install-include: lib/bemenu.h
	mkdir -p "$(DESTDIR)$(PREFIX)$(includedir)"
	cp $^ "$(DESTDIR)$(PREFIX)$(includedir)"

install-libs: $(libs)
	mkdir -p "$(DESTDIR)$(PREFIX)$(libdir)"
	cp $(addsuffix .$(VERSION), $^) "$(DESTDIR)$(PREFIX)$(libdir)"

install-lib-symlinks: $(libs) | install-libs
	cp -RP $^ $(addsuffix .$(firstword $(subst ., ,$(VERSION))), $^) "$(DESTDIR)$(PREFIX)$(libdir)"

install-bins:
	mkdir -p "$(DESTDIR)$(PREFIX)$(bindir)"
	-cp $(bins) "$(DESTDIR)$(PREFIX)$(bindir)"
	-chmod 0755 $(addprefix "$(DESTDIR)$(PREFIX)$(bindir)"/,$(bins))

install-man: bemenu.1
	-hash scdoc && mkdir -p "$(DESTDIR)$(PREFIX)$(mandir)"
	-hash scdoc && cp $< "$(DESTDIR)$(PREFIX)$(mandir)"

install-renderers: install-curses install-wayland install-x11

install-curses:
	mkdir -p "$(DESTDIR)$(PREFIX)$(libdir)/bemenu"
	-cp bemenu-renderer-curses.so "$(DESTDIR)$(PREFIX)$(libdir)/bemenu"

install-wayland:
	mkdir -p "$(DESTDIR)$(PREFIX)$(libdir)/bemenu"
	-cp bemenu-renderer-wayland.so "$(DESTDIR)$(PREFIX)$(libdir)/bemenu"

install-x11:
	mkdir -p "$(DESTDIR)$(PREFIX)$(libdir)/bemenu"
	-cp bemenu-renderer-x11.so "$(DESTDIR)$(PREFIX)$(libdir)/bemenu"

install-docs: install-man

install-base: install-pkgconfig install-include install-lib-symlinks install-bins

install: install-base install-renderers install-docs
	@echo "Install OK!"

doxygen:
	BM_VERSION=$(VERSION) doxygen doxygen/Doxyfile
	cp -R doxygen/doxygen-qmi-style/navtree html
	cp -R doxygen/doxygen-qmi-style/search html/search

sign:
	test "x$(GIT_TAG)" = "x$(VERSION)"
	git archive --prefix="bemenu-$(VERSION)/" -o "bemenu-$(VERSION).tar.gz" "$(GIT_TAG)"
	gpg --default-key "$(GPG_KEY_ID)" --armor --detach-sign "bemenu-$(VERSION).tar.gz"

check-symbols: libbemenu.so lib/bemenu.h
	sh scripts/check-symbols.sh $^ bemenu-renderer-*.so

clean:
	$(RM) -r *.dSYM # OSX generates .dSYM dirs with -g ...
	$(RM) $(pkgconfigs) $(libs) $(bins) $(renderers) $(mans) *.a *.so.*
	$(RM) lib/renderers/wayland/wlr-*.h lib/renderers/wayland/wlr-*.c lib/renderers/wayland/xdg-shell.c
	$(RM) -r html

uninstall:
	$(RM) -r "$(DESTDIR)$(PREFIX)$(libdir)/bemenu"
	$(RM) "$(DESTDIR)$(PREFIX)$(mandir)/bemenu.1"
	$(RM) "$(DESTDIR)$(PREFIX)$(bindir)/bemenu"
	$(RM) "$(DESTDIR)$(PREFIX)$(bindir)/bemenu-run"
	$(RM) "$(DESTDIR)$(PREFIX)$(libdir)"/libbemenu.so*
	$(RM) "$(DESTDIR)$(PREFIX)$(includedir)/bemenu.h"

.DELETE_ON_ERROR:
.PHONY: all clean uninstall install install-base install-pkgconfig install-include install-libs install-lib-symlinks \
		install-man install-bins install-docs install-renderers install-curses install-wayland install-x11 \
		doxygen sign check-symbols clients curses x11 wayland
