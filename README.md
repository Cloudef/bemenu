bemenu
======

Dynamic menu library and client program inspired by dmenu

![preview](.github/preview.svg)

## Building

```sh
# Build everything
make

# To build only certain features, pass the targets which you are interested into
#
# You can also use the following meta-targets for common features:
# - clients (bemenu, bemenu-run)
# - x11
# - wayland
# - curses
#
# For example this would build the bemenu and bemenu-run binaries and the x11 backend:
make clients x11

# To install the built features, simply run:
make install

# NOTE: You may get errors during install when not building all the features.
#       These errors are free to ignore if `Install OK!` is printed.

# By default that will install to /usr/local, but you can change this with PREFIX
make install PREFIX=/usr

# Other usual variables are available for modifying such as DESTDIR, bindir, libdir and mandir
# Note that if you want a custom PREFIX or libdir, you should pass those during build as well,
# since they will be used compile-time to figure out where to load backends from!

# HTML API documentation (requires doxygen installed):
make doxygen

# To test from source, you have to point the LD_LIBRARY_PATH and BEMENU_RENDERERS variables:
LD_LIBRARY_PATH=. BEMENU_RENDERERS=. ./bemenu-run
```

## OSX

```sh
# Make sure you have GNU Make and pkg-config installed
brew install make pkg-config

# You may need to setup your pkg-config to point to the brew version of the libraries
# For example to build curses backend, you'd do:
PKG_CONFIG_PATH="/usr/local/opt/ncurses/lib/pkgconfig" gmake curses

# Other than that, follow the normal build steps, but use gmake instead of make
```

## Dependencies

All dependencies are searched with `pkg-config`

| Backend  | Dependencies                                                           |
|----------|------------------------------------------------------------------------|
| curses   | ncursesw                                                               |
| x11      | x11, xinerama, cairo, pango, pangocairo                                |
| Wayland  | wayland-client, wayland-protocols, cairo, pango, pangocairo, xbkcommon |

## Environment variables

| Variable         | Description                             | Value                |
|------------------|-----------------------------------------|----------------------|
| BEMENU_OPTS      | Options for bemenu, bemenu-run from env | Any cli argument     |
| BEMENU_BACKEND   | Force backend by name                   | x11, wayland, curses |
| BEMENU_RENDERER  | Force backend by loading a .so file     | Path to the .so file |
| BEMENU_RENDERERS | Override the backend search path        | Path to a directory  |

## About Wayland support

Wayland is only supported by compositors that implement the [wlr-layer-shell](https://github.com/swaywm/wlr-protocols/tree/master/unstable) protocol.
Typically [wlroots](https://github.com/swaywm/wlroots)-based compositors.

## License

* [GNU GPLv3 (or any later version)](LICENSE-CLIENT) for client program[s] and
  other sources except library and bindings
* [GNU LGPLv3 (or any later version)](LICENSE-LIB) for library and bindings
