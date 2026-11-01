# Essais du cœur athos sous Linux (sur le 701, démarré sur la clé).
CFLAGS = -std=gnu11 -O2 -Wall -Wno-unused-function -I../core -I../compat -I.. 
CORE = attach caps desc dma eeprom gpio initvals pcu phy qcu reset ani glue athos wlan
OBJS = $(CORE:%=%.o) osdep_linux.o
VPATH = ../core

all: essai vider connecter

connecter: connecter.o $(OBJS)
	$(CC) -o $@ $^

essai: essai.o $(OBJS)
	$(CC) -o $@ $^

vider: vider.c vider.h
	$(CC) -O2 -o $@ vider.c

clean:
	rm -f *.o essai vider connecter
