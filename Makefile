CFLAGS = -O2 -Wall
FW_FLASH = fw-flash
INCLUDE = src/include
DEPS = $(INCLUDE)/libmtd.h src/crc32.h src/header.h
LIBDIR = src/lib
LIB = $(LIBDIR)/libmtd.a
OBJ = src/fw-flash.o src/crc32.o
ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif


.PHONY: all clean install

all: $(FW_FLASH)

%.o: %.c $(DEPS)
	$(CC) -I$(INCLUDE) -c -o $@ $< $(CFLAGS)

$(LIB):
	$(MAKE) -C $(LIBDIR)

$(FW_FLASH): $(OBJ) $(LIB)
	$(CC) -o $@ $(LIB) $^ $(CFLAGS)

clean:
	rm -f src/*.o $(FW_FLASH)
	$(MAKE) -C $(LIBDIR) clean

install:
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m755 $(FW_FLASH) $(DESTDIR)$(PREFIX)/bin
