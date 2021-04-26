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
PKG_CONFIG_PATH="/usr/local/opt/ncurses/lib/pkgconfig" sh build-osx.sh curses

# Other than that, follow the normal build steps, but use `build-osx.sh` instead of make
```

## Dependencies

All dependencies are searched with `pkg-config`

| Backend  | Dependencies                                                           |
|----------|------------------------------------------------------------------------|
| curses   | ncursesw                                                               |
| x11      | x11, xinerama, cairo, pango, pangocairo                                |
| Wayland  | wayland-client, wayland-protocols, cairo, pango, pangocairo, xkbcommon |

## Environment variables

| Variable         | Description                             | Value                |
|------------------|-----------------------------------------|----------------------|
| BEMENU_OPTS      | Options for bemenu, bemenu-run from env | Any cli argument     |
| BEMENU_BACKEND   | Force backend by name                   | x11, wayland, curses |
| BEMENU_RENDERER  | Force backend by loading a .so file     | Path to the .so file |
| BEMENU_RENDERERS | Override the backend search path        | Path to a directory  |
| BEMENU_SCALE     | Override the rendering scale factor     | Float value          |

## About Wayland support

Wayland is only supported by compositors that implement the [wlr-layer-shell](https://github.com/swaywm/wlr-protocols/tree/master/unstable) protocol.
Typically [wlroots](https://github.com/swaywm/wlroots)-based compositors.

## Keybindings


|         Key         |                             Binding                            |
|---------------------|----------------------------------------------------------------|
| Left Arrow          |  Move cursor left                                              |
| Right Arrow         |  Move cursor right                                             |
| Up Arrow            |  Move to previous item                                         |
| Down Arrow          |  Move to next item                                             |
| Shift + Left Arrow  |  Select previous item                                          |
| Shift + Right Arrow |  Select next item                                              |
| Shift + Alt + <     |  Select first item in actual list                              |
| Shift + Alt + >     |  Select last item in actual list                               |
| Shift + Page Up     |  Select first item in actual list                              |
| Shift + Page Down   |  Select last item in actual list                               |
| Page Up             |  Select first item in displayed list                           |
| Page Down           |  Select last item in displayed list                            |
| Tab                 |  Move to next item                                             |
| Shift + Tab         |  Select item and place it in filter                            |
| Esc                 |  Exit bemenu                                                   |
| Insert              |  Return filter text or selected items if multi selection       |
| Shift + Return      |  Return filter text or selected items if multi selection       |
| Return              |  Execute selected item                                         |
| Home                |  Curses cursor set to 0                                        |
| End                 |  Cursor set to end of filter text                              |
| Backspace           |  Delete character at cursor                                    |
| Delete              |  Delete character at cursor                                    |
| Delete Left         |  Delete text before cursor                                     |
| Delete Right        |  Delete text after cursor                                      |
| Word Delete         |  Delete all text in filter                                     |
| Alt + v             |  Select last item in displayed list                            |
| Alt + j             |  Select next item                                              |
| Alt + d             |  Select last item in display list                              |
| Alt + l             |  Select previous item                                          |
| Alt + f             |  Select next item                                              |
| Alt + 0-9           |  Execute selected item with custom exit code                   |
| Ctrl + Return       |  Select item but don't quit to select multiple items           |
| Ctrl + g            |  Exit bemenu                                                   |
| Ctrl + n            |  Select next item                                              |
| Ctrl + p            |  Select previous item                                          |
| Ctrl + a            |  Move cursor to beginning of text in filter                    |
| Ctrl + e            |  Move cursor to end of text in filter                          |
| Ctrl + h            |  Delete character at cursor                                    |
| Ctrl + u            |  Kill text behind cursor                                       |
| Ctrl + k            |  Kill text after cursor                                        |
| Ctrl + w            |  Kill all text in filter                                       |
| Ctrl + m            |  Execute selected item                                         |

## Projects using bemenu

* [pinentry-bemenu](https://github.com/t-8ch/pinentry-bemenu)

## License

* [GNU GPLv3 (or any later version)](LICENSE-CLIENT) for client program[s] and
  other sources except library and bindings
* [GNU LGPLv3 (or any later version)](LICENSE-LIB) for library and bindings
