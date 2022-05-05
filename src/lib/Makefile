CFLAGS = -O2 -Wall
LIB = libmtd.a
INCLUDE = ../include
DEPS = $(INCLUDE)/libmtd.h libmtd_int.h common.h xalloc.h
OBJ = libmtd.o libmtd_legacy.o

.PHONY: all
all: $(LIB)

%.o: %.c $(DEPS)
	$(CC) -I$(INCLUDE) -c -o $@ $< $(CFLAGS)

$(LIB): $(OBJ)
	ar ru $@ $^
	ranlib $@

.PHONY: clean
clean:
	rm -f *.o $(LIB)
