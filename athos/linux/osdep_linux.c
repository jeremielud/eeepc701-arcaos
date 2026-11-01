/*
 * osdep_linux.c : services de plate-forme pour les essais du cœur sous Linux, en
 * espace utilisateur (root). Le DMA se fait dans des pages verrouillées en mémoire,
 * dont l'adresse physique est lue dans /proc/self/pagemap ; une allocation ne
 * dépasse jamais une page (4 Ko), qui est donc physiquement contiguë.
 */
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "osdep.h"

FILE *os_journal;

void os_log(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
	if (os_journal) {
		va_start(args, format);
		vfprintf(os_journal, format, args);
		va_end(args);
		fflush(os_journal);
	}
	fflush(stdout);
}

void os_fatal(const char *file, int line)
{
	os_log("ARRÊT : %s:%d\n", file, line);
	exit(3);
}

void os_udelay(unsigned long us)
{
	struct timespec t0, t;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	if (us >= 1000) {
		struct timespec d = { us / 1000000, (us % 1000000) * 1000 };
		nanosleep(&d, NULL);
		return;
	}
	do clock_gettime(CLOCK_MONOTONIC, &t);
	while ((t.tv_sec - t0.tv_sec) * 1000000L + (t.tv_nsec - t0.tv_nsec) / 1000 < (long)us);
}

unsigned long os_jiffies(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000UL + t.tv_nsec / 1000000;
}

void *os_dma_alloc(uint32_t size, uint32_t *phys)
{
	static int pagemap = -1;
	uint64_t entry;
	void *p;

	if (size > 4096)
		os_fatal(__FILE__, __LINE__);
	p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED | MAP_POPULATE, -1, 0);
	if (p == MAP_FAILED)
		return NULL;
	*(volatile char *)p = 0;
	if (pagemap < 0)
		pagemap = open("/proc/self/pagemap", O_RDONLY);
	if (pread(pagemap, &entry, 8, ((uintptr_t)p / 4096) * 8) != 8 || !(entry & (1ULL << 63)))
		os_fatal(__FILE__, __LINE__);
	entry &= (1ULL << 55) - 1;
	if (entry >= (1ULL << 20))      /* au-delà de 4 Go : inutilisable en DMA 32 bits */
		os_fatal(__FILE__, __LINE__);
	*phys = (uint32_t)(entry * 4096);
	return p;
}

void os_dma_free(void *virt, uint32_t size)
{
	(void)size;
	munmap(virt, 4096);
}

/* Trace des accès aux registres (compilation avec -DATHOS_TRACE), au format de
 * mmiotrace réduit : « R|W décalage valeur ». */
FILE *os_trace;
void *os_trace_base;

uint32_t os_mmio_read(volatile void *addr)
{
	uint32_t v = *(volatile uint32_t *)addr;
	if (os_trace)
		fprintf(os_trace, "R %04lx %08x\n", (unsigned long)((char *)addr - (char *)os_trace_base), v);
	return v;
}

void os_mmio_write(uint32_t val, volatile void *addr)
{
	if (os_trace)
		fprintf(os_trace, "W %04lx %08x\n", (unsigned long)((char *)addr - (char *)os_trace_base), val);
	*(volatile uint32_t *)addr = val;
}
