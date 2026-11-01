/* vider.h : relevé des registres de l'AR5212/AR2425 (sans les registres qui
 * s'effacent à la lecture), une ligne « adresse valeur » par registre. */
#include <stdio.h>
#include <stdint.h>
static const struct { uint32_t de, a; } athos_plages[] = {
	{ 0x0000, 0x00bc }, { 0x0800, 0x0a00 }, { 0x1000, 0x1300 }, { 0x4000, 0x4100 },
	{ 0x8000, 0x8200 }, { 0x9800, 0xa000 }, { 0xa000, 0xa400 },
};
static void athos_vider(volatile uint32_t *r, FILE *f)
{
	for (unsigned p = 0; p < sizeof(athos_plages) / sizeof(athos_plages[0]); p++)
		for (uint32_t a = athos_plages[p].de; a < athos_plages[p].a; a += 4)
			fprintf(f, "%04x %08x\n", a, r[a / 4]);
}
