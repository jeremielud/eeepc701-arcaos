/* vider.c : relève les registres de la carte (même tenue par ath5k) dans un fichier. */
#include <fcntl.h>
#include <sys/mman.h>
#include "vider.h"
int main(int argc, char **argv)
{
	int fd = open("/sys/bus/pci/devices/0000:01:00.0/resource0", O_RDWR | O_SYNC);
	volatile uint32_t *r = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	FILE *f = argc > 1 ? fopen(argv[1], "w") : stdout;
	if (fd < 0 || r == MAP_FAILED || !f) return 1;
	athos_vider(r, f);
	return 0;
}
