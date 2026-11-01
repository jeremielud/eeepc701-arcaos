# Essais du cœur athos sous Linux (sur le 701, démarré sur la clé).
CFLAGS = -std=gnu11 -O2 -Wall -Wno-unused-function -I../core -I../compat -I.. -DATHOS_TRACE
CORE = attach caps desc dma eeprom gpio initvals pcu phy qcu reset glue
OBJS = $(CORE:%=%.o) osdep_linux.o
VPATH = ../core

all: essai vider

essai: essai.o $(OBJS)
	$(CC) -o $@ $^

vider: vider.c vider.h
	$(CC) -O2 -o $@ vider.c

clean:
	rm -f *.o essai
