/* lire.c : lit quelques registres de la puce Atheros (BAR0 par sysfs), sans rien écrire. */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define RES "/sys/bus/pci/devices/0000:01:00.0/resource0"

int main(int argc, char **argv)
{
    int fd = open(RES, O_RDWR | O_SYNC);
    if (fd < 0) { perror(RES); return 1; }
    volatile uint32_t *r = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (r == MAP_FAILED) { perror("mmap"); return 1; }
    for (int i = 1; i < argc; i++) {
        unsigned long off = strtoul(argv[i], NULL, 0);
        printf("0x%04lx = 0x%08x\n", off, r[off / 4]);
    }
    return 0;
}
