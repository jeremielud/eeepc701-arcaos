/*
 * osdep.h : services que la plate-forme fournit au cœur du pilote (Linux en espace
 * utilisateur pour les essais, OS/2 pour le vrai pilote).
 */
#ifndef OSDEP_H
#define OSDEP_H
#ifdef __WATCOMC__
#define __attribute__(x)        /* OpenWatcom : pas d'attributs à la GCC */
#endif
#include <stdint.h>

void os_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
void os_fatal(const char *file, int line) __attribute__((noreturn));
void os_udelay(unsigned long us);
unsigned long os_jiffies(void);            /* millisecondes */
/* Mémoire pour le DMA : adresse virtuelle, et adresse physique dans *phys. */
void *os_dma_alloc(uint32_t size, uint32_t *phys);
void os_dma_free(void *virt, uint32_t size);

#endif
