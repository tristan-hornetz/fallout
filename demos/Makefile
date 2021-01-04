override CFLAGS += -O3 -pthread -Wno-attributes
CC=gcc

SOURCES := $(wildcard *.c)
BINARIES := $(SOURCES:%.c=%.elf)

all: $(BINARIES)

tsx_available:
	cp tsx_available._c tsx_available.c
	$(CC) -D $(notsx) tsx_available.c -o $@
	rm tsx_available.c


%.o: %.c tsx_available
	$(CC) -D $(shell ./tsx_available) -c $< -o $@ -m64 -static $(CFLAGS)

%.elf: %.o ../primitives/basic_primitives.o
	$(CC) $^ -o $@ -m64 -static $(CFLAGS)
	mv $@ ../demo_$*

clean:
	rm -f *.o *.so *.a tsx_available
	rm -f ../demo_*
