/*
 * osdep_os2.c : services de plate-forme du cœur athos dans un pilote OS/2 32 bits
 * (kit Drv32 de David Azarewicz). Fournit aussi la petite bibliothèque C dont le cœur
 * a besoin (calloc, free, qsort, vsnprintf), absente dans un pilote.
 *
 * Le journal est gardé dans un tampon circulaire en mémoire, que l'utilitaire
 * athoslog.exe lit par IOCtl ; pendant l'initialisation, il s'affiche aussi à l'écran.
 */
#include <stdarg.h>
#include "Dev32lib.h"
#include "osdep.h"

#define JOURNAL_TAILLE 16384

char athos_journal[JOURNAL_TAILLE];
unsigned long athos_journal_pos;        /* octets écrits depuis le démarrage */
int athos_journal_ecran = 1;            /* recopier à l'écran (initialisation) */

/* --- vsnprintf réduit : %c %s %d %u %x %X %p %%, largeur, 0, l, ll ignoré -------- */
static int ecrire_nombre(char *b, int n, int pos, unsigned long v, int base, int neg, int largeur, int zero, int maj)
{
	char tmp[16];
	int k = 0;
	const char *ch = maj ? "0123456789ABCDEF" : "0123456789abcdef";

	do {
		tmp[k++] = ch[v % base];
		v /= base;
	} while (v && k < 15);
	if (neg)
		tmp[k++] = '-';
	while (k < largeur && k < 15)
		tmp[k++] = zero && !neg ? '0' : ' ';
	while (k > 0) {
		if (pos < n - 1)
			b[pos] = tmp[k - 1];
		pos++;
		k--;
	}
	return pos;
}

int vsnprintf(char *b, size_t n, const char *f, va_list ap)
{
	int pos = 0;

	for (; *f; f++) {
		int largeur = 0, zero = 0;
		if (*f != '%') {
			if (pos < (int)n - 1)
				b[pos] = *f;
			pos++;
			continue;
		}
		f++;
		if (*f == '-')
			f++;
		if (*f == '0') {
			zero = 1;
			f++;
		}
		while (*f >= '0' && *f <= '9')
			largeur = largeur * 10 + (*f++ - '0');
		if (*f == '.') {                /* précision : ignorée */
			f++;
			while (*f >= '0' && *f <= '9')
				f++;
		}
		while (*f == 'l' || *f == 'z' || *f == 'h')
			f++;
		switch (*f) {
		case 'c':
			if (pos < (int)n - 1)
				b[pos] = (char)va_arg(ap, int);
			pos++;
			break;
		case 's': {
			const char *s = va_arg(ap, const char *);
			if (!s)
				s = "(nul)";
			while (*s) {
				if (pos < (int)n - 1)
					b[pos] = *s;
				pos++;
				s++;
			}
			break;
		}
		case 'd':
		case 'i': {
			long v = va_arg(ap, long);
			pos = ecrire_nombre((char *)b, (int)n, pos, v < 0 ? -v : v, 10, v < 0, largeur, zero, 0);
			break;
		}
		case 'u':
			pos = ecrire_nombre((char *)b, (int)n, pos, va_arg(ap, unsigned long), 10, 0, largeur, zero, 0);
			break;
		case 'x':
		case 'p':
			pos = ecrire_nombre((char *)b, (int)n, pos, va_arg(ap, unsigned long), 16, 0, largeur, zero, 0);
			break;
		case 'X':
			pos = ecrire_nombre((char *)b, (int)n, pos, va_arg(ap, unsigned long), 16, 0, largeur, zero, 1);
			break;
		case '%':
			if (pos < (int)n - 1)
				b[pos] = '%';
			pos++;
			break;
		default:
			break;
		}
	}
	if (n)
		b[pos < (int)n ? pos : (int)n - 1] = 0;
	return pos;
}

void os_log(const char *format, ...)
{
	char ligne[200];
	va_list ap;
	int len;

	va_start(ap, format);
	len = vsnprintf(ligne, sizeof(ligne), format, ap);
	va_end(ap);
	if (len > (int)sizeof(ligne) - 1)
		len = sizeof(ligne) - 1;
	for (int i = 0; i < len; i++)
		athos_journal[athos_journal_pos++ % JOURNAL_TAILLE] = ligne[i];
	if (athos_journal_ecran)
		iprintf("%s", ligne);
}

void os_fatal(const char *file, int line)
{
	os_log("athos : ARRÊT %s:%d\n", file, line);
	for (;;)
		UtMSleep(1000);
}

void os_udelay(unsigned long us)
{
	if (us >= 1000)
		mdelay(us / 1000);
	if (us % 1000)
		udelay(us % 1000);
}

unsigned long os_jiffies(void)
{
	return TimeGetMs();
}

/* Mémoire DMA : une page alignée par allocation (jamais à cheval sur deux pages),
 * prise dans le tas du kit qui donne aussi l'adresse physique et l'adresse 16:16. */
void *os_dma_alloc(uint32_t size, uint32_t *phys)
{
	static char *bloc;
	static uint32_t bloc_phys, bloc_reste;
	void *p;

	if (size > 4096)
		return 0;
	/* Les descripteurs (quelques dizaines d'octets) se partagent une page. */
	if (size <= 128) {
		if (bloc_reste < 128) {
			bloc = MemAllocAlign(4096, 4096);
			if (!bloc)
				return 0;
			memset(bloc, 0, 4096);
			bloc_phys = MemPhysAdr(bloc);
			bloc_reste = 4096;
		}
		p = bloc + (4096 - bloc_reste);
		*phys = bloc_phys + (4096 - bloc_reste);
		bloc_reste -= 128;
		return p;
	}
	p = MemAllocAlign(size <= 2048 ? 2048 : 4096, size <= 2048 ? 2048 : 4096);
	if (!p)
		return 0;
	memset(p, 0, size);
	*phys = MemPhysAdr(p);
	return p;
}

void os_dma_free(void *virt, uint32_t size)
{
	(void)size;
	MemFree(virt);
}

/* --- Petite bibliothèque C pour le cœur -------------------------------------- */
/* Versions appelables (le kit les déclare intrinsèques, mais le cœur, compilé avec
 * les en-têtes d'OpenWatcom, peut les appeler pour de bon). */
#pragma function(memset, strcmp)
void *memset(void *s, short c, size_t n)
{
	unsigned char *p = s;
	while (n--)
		*p++ = (unsigned char)c;
	return s;
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

void *calloc(size_t c, size_t n)
{
	void *p = MemAlloc(c * n);
	if (p)
		memset(p, 0, c * n);
	return p;
}

void *malloc(size_t n)
{
	return MemAlloc(n);
}

void free(void *p)
{
	if (p)
		MemFree(p);
}

/* Tri par insertion : seule l'EEPROM s'en sert, sur quelques éléments. */
void qsort(void *base, size_t num, size_t size, int (*cmp)(const void *, const void *))
{
	char *b = base, tmp[64];

	if (size > sizeof(tmp))
		return;
	for (size_t i = 1; i < num; i++) {
		size_t j = i;
		memcpy(tmp, b + i * size, size);
		while (j > 0 && cmp(b + (j - 1) * size, tmp) > 0) {
			memcpy(b + j * size, b + (j - 1) * size, size);
			j--;
		}
		memcpy(b + j * size, tmp, size);
	}
}
