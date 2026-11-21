# wmsystemtray

A freedesktop.org system tray dockapp for Window Maker, based on the
downloaded wmsystemtray 1.4 source release by Brad Jorsch. The original
release history is in [ChangeLog](ChangeLog).

## Build

The distributed configure script requires a C compiler, pkg-config, and X11
development libraries including Xext, Xfixes, Xmu or Xmuu, and Xpm.

```sh
./configure
make
make check
```

Run `./wmsystemtray --help` for options, or consult the generated
`wmsystemtray.1` manual page. To install, run `make install` with the
appropriate permissions.

The icon leak test requires Valgrind. The X11 integration test requires Xvfb,
xvfb-run, and xauth. Each test is skipped if its tools are missing.

## License

GPL-2.0-or-later; see [COPYING](COPYING) and the original source notices.
