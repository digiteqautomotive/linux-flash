CFLAGS = -O2 -Wall -g
FW_FLASH = fw-flash
INCLUDE = include
DEPS = $(INCLUDE)/libmtd.h crc32.h header.h
LIBDIR = lib
LIB = $(LIBDIR)/libmtd.a
OBJ = fw-flash.o crc32.o

.PHONY: all
all: $(FW_FLASH)

%.o: %.c $(DEPS)
	$(CC) -I$(INCLUDE) -c -o $@ $< $(CFLAGS)

$(LIB):
	$(MAKE) -C $(LIBDIR)

$(FW_FLASH): $(OBJ) $(LIB)
	$(CC) -o $@ $(LIB) $^ $(CFLAGS)

.PHONY: clean
clean:
	rm -f *.o $(FW_FLASH)
	$(MAKE) -C $(LIBDIR) clean
