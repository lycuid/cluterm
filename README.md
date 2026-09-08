# cluless terminal

![demo](https://lycuid.github.io/assets/cluterm/demo.gif)

Requirements
-----
 - [sdl2](https://github.com/libsdl-org/SDL)
 - [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)
 - [fontconfig](https://gitlab.freedesktop.org/fontconfig/fontconfig)
 - pkg-config (optional).

_**Note**_: default configs can be updated in `lib/config.h` file.

Build
-----
```sh
make clean && make -j build
```

Run
-----
```sh
make run
```
Usage
-----
```txt
Usage: cluterm [options] [-e command [args...]]

Options:
  -h   help        Show this help.
  -t   title       Set window title.
  -g   geometry    Set window window (COLSxROWS).
  -fg  color       Set foreground color (#RRGGBB).
  -bg  color       Set background color (#RRGGBB).
  -tw  width       Set tab width.
  -pt  size        Set top padding.
  -pr  size        Set right padding.
  -pb  size        Set bottom padding.
  -pl  size        Set left padding.
  -fn  font        Set font family.
  -fs  size        Set font size.
  -e   command...  Execute command and pass remaining arguments.
```

Licence:
--------
[GPLv3](https://gnu.org/licenses/gpl.html)
