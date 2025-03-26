# fw-flash

fw-flash is a tool for flashing firmware to
[FG4](https://www.digiteqautomotive.com/en/products/grabbers-and-image-tools/framegrabber-4)
grabber cards under Linux.


```
./fw-flash - mgb4 firmware flash tool.

Usage:
./fw-flash [-s SN] FILE
./fw-flash -i FILE
./fw-flash -l
./fw-flash -v

Options:
  -s SN    Flash card serial number SN
  -i FILE  Show firmware info and exit
  -l       List available devices (SNs) and exit
  -v       Show program version and exit

```

## Build
Build requirements:
* C compiler + make

Build steps:
```shell
make
```

## License
fw-flash is licensed under GPL-3.0 (only).
fw-flash uses 3rd party code from mtd-utils (GPL-2) and zlib (zlib license),
see the relevant source files for details.
