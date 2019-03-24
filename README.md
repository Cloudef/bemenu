bemenu
======

![Linux](http://build.cloudef.pw/platform/linux.svg) [![buildhck status](http://build.cloudef.pw/build/bemenu/master/linux%20x86_64/current/status.svg)](http://build.cloudef.pw/build/bemenu/master/linux%20x86_64)
![Darwin](http://build.cloudef.pw/platform/darwin.svg) [![buildhck status](http://build.cloudef.pw/build/bemenu/master/darwin%20x86_64/current/status.svg)](http://build.cloudef.pw/build/bemenu/master/darwin%20x86_64)

Dynamic menu library and client program inspired by dmenu

## Renderers

bemenu supports three different renderers:

- ncurses
- X11
- Wayland

Enable/disable the renderers by appending these CMake options when executing `cmake <dir>`:

- `-DBEMENU_CURSES_RENDERER=[OFF|ON]`
- `-DBEMENU_X11_RENDERER=[OFF|ON]`
- `-DBEMENU_WAYLAND_RENDERER=[OFF|ON]`

## License
* [GNU GPLv3 (or any later version)](LICENSE-CLIENT) for client program[s] and
  other sources except library and bindings
* [GNU LGPLv3 (or any later version)](LICENSE-LIB) for library and bindings
