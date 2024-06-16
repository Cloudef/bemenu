bemenu
======

Dynamic menu library and client program inspired by dmenu

![preview](.github/preview.svg)

## Releases

Releases are signed with [29317348D687B86B](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x29317348D687B86B) and published [on GitHub](https://github.com/Cloudef/bemenu/releases).

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

### Homebrew

```sh
# Make sure you have GNU Make and pkg-config installed
brew install make pkg-config

# You may need to setup your pkg-config to point to the brew version of the libraries
# For example to build curses backend, you'd do:
PKG_CONFIG_PATH="/usr/local/opt/ncurses/lib/pkgconfig" sh build-osx.sh curses

# Other than that, follow the normal build steps, but use `build-osx.sh` instead of make
```

### Nix

There is default.nix provided in this repo, you can install bemenu with it by running
```sh
nix-env -i -f default.nix
```

This installs only the curses backend.

## Dependencies

- C compiler
- scdoc to generate manpage

### Backend-specific

All dependencies below are searched with `pkg-config`

| Backend  | Dependencies                                                           |
|----------|------------------------------------------------------------------------|
| curses   | ncursesw                                                               |
| x11      | x11, xinerama, cairo, pango, pangocairo                                |
| Wayland  | wayland-client, wayland-protocols, cairo, pango, pangocairo, xkbcommon |

Currently, pasting from clipboard is done at runtime with `wl-paste` and `xclip`, attempted in that order.

### Installing the dependencies

#### Ubuntu 20.04

```sh
sudo apt install scdoc wayland-protocols libcairo-dev libpango1.0-dev libxkbcommon-dev libwayland-dev
```

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

### Default Bindings
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
| Ctrl + y            |  Paste clipboard                                               |

### Vim Bindings
Vim bindings can be activated via the `--binding vim` option. All bindings are in vim
`normal` mode. When bemenu is started with vim bindings it will be in `insert` mode. By 
pressing `escape`, `normal` mode can be activated.

**Note**: The default bindings can still be used for actions that do not have a separate
vim binding such as launching a program or pasting.

|  Key  |                    Binding                    |
|-------|-----------------------------------------------|
|  j/n  | Goto next item                                |
|  k/p  | Goto previous item                            |
|  h    | Move cursor left                              |
|  l    | Move cursor right                             |
|  q    | Quit bemenu                                   |
|  v    | Toggle item selection                         |
|  i    | Enter insert mode                             |
|  I    | Move to line start and enter insert mode      |
|  a    | Move to the right and enter insert mode       |
|  A    | Move to line end and enter insert mode        |
|  w    | Move a word                                   |
|  b    | Move a word backwards                         |
|  e    | Move to end of word                           |
|  x    | Delete a character                            |
|  X    | Delete a character before the cursor          |
|  0    | Move to line start                            |
|  $    | Move to line end                              |
|  gg   | Goto first item                               |
|  G    | Goto last item                                |
|  H    | Goto first item in view                       |
|  M    | Goto middle item in view                      |
|  L    | Goto last item in view                        |
|  F    | Scroll one page of items down                 |
|  B    | Scroll one page of items up                   |
|  dd   | Delete the whole line                         |
|  dw   | Delete a word                                 |
|  db   | Delete a word backwards                       |
|  d0   | Delete to start of line                       |
|  d$   | Delete to end of line                         |
|  cc   | Change the whole line                         |
|  cw   | Change a word                                 |
|  cb   | Change a word backwards                       |
|  c0   | Change to start of line                       |
|  c$   | Change to end of line                         |

## Projects using bemenu

* [pinentry-bemenu](https://github.com/t-8ch/pinentry-bemenu)

## License

* [GNU GPLv3 (or any later version)](LICENSE-CLIENT) for client program[s] and
  other sources except library and bindings
* [GNU LGPLv3 (or any later version)](LICENSE-LIB) for library and bindings
