#
# -- udis86/libudis86
#

UKVM_CC=gcc
UKVM_CFLAGS=-Wall -Werror -std=c99 -O0 -g

.PHONY: all
all: libudis86.a

UDIS_OBJS = \
	itab.o \
	decode.o \
	syn.o \
	syn-intel.o \
	syn-att.o \
	udis86.o

UDIS_HEADERS = decode.h  extern.h  itab.h  syn.h  types.h  udint.h

$(UDIS_OBJS): $(UDIS_HEADERS)

%.o: %.c
	$(UKVM_CC) $(UKVM_CFLAGS) -c $< -o $@

libudis86.a: $(UDIS_OBJS)
	ar rcs $@ $^

.PHONY: clean
clean:
	rm -f *.o
	rm -f libudis86.a
