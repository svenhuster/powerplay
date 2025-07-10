CC = gcc
CFLAGS += -Wall -Wextra -Wpedantic -Wconversion -Wformat -Wformat-signedness -Wsign-conversion
CFLAGS += -Wshadow -Wstrict-prototypes -Wundef -Wdouble-promotion -Wcast-qual -Wpointer-arith
CFLAGS += -Wredundant-decls -Wmissing-declarations -Wswitch -Wswitch-enum -Wlogical-op -Wstrict-overflow
CFLAGS += -Wnull-dereference -Wstack-protector -Wformat-overflow -Wimplicit-function-declaration
CFLAGS += -fstack-protector-strong -fsanitize=undefined -fno-omit-frame-pointer
CFLAGS += -std=c99 -ggdb3 -D_FORTIFY_SOURCE=2
CFLAGS += $(shell pkg-config --cflags libmodbus)
LDFLAGS += -fstack-protector-strong -fsanitize=undefined
LDFLAGS += $(shell pkg-config --libs libmodbus)

sparkshift: sparkshift.o powerplay.o

powerplay.o: powerplay.h
sparkshift.o: powerplay.h

.PHONY: install
install: sparkshift
	install -m755 -Dt $(out)/bin/ $?

.PHONY: clean
clean:
	rm -rf sparkshift *.o result
