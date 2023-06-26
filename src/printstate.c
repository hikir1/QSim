#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "main.h"

static int gcd(int a, int b)
{
	int sign = a < 0 && b < 0? -1: 1;
	if (a < 0)
		a = -a;
	if (b < 0)
		b = -b;
	if (a == b)
		return a;
	if (a < b)
	{
		a ^= b;
		b ^= a;
		a ^= b;
	}
	while (b != 0)
	{
		a = a % b;
		a ^= b;
		b ^= a;
		a ^= b;
	}
	return a * sign;
}

static void extract_ggcd(int fracs[NAMPS][2][2], int ggcd[2][2])
{
	bool allroot2 = true;

	ggcd[0][0] = 1 << 30;
	ggcd[0][1] = 1 << 30;
	ggcd[1][0] = 0;
	ggcd[1][1] = 1;

	for (int i = 0; i < NAMPS; i++)
	{
		if (fracs[i][0][0] != 0)
		{
			allroot2 = false;
			ggcd[0][0] = gcd(ggcd[0][0], fracs[i][0][0]);
			ggcd[0][1] = gcd(ggcd[0][1], fracs[i][0][1]);
		}
		if (fracs[i][1][0] != 0)
		{
			ggcd[0][0] = gcd(ggcd[0][0], fracs[i][1][0]);
			ggcd[0][1] = gcd(ggcd[0][1], fracs[i][1][1]);
		}
	}

	for (int i = 0; i < NAMPS; i++)
	{
		fracs[i][0][0] /= ggcd[0][0];
		fracs[i][1][0] /= ggcd[0][0];
		fracs[i][0][1] /= ggcd[0][1];
		fracs[i][1][1] /= ggcd[0][1];
	}

	if (allroot2)
	{
		ggcd[1][0] = ggcd[0][0];
		ggcd[1][1] = ggcd[0][1];
		ggcd[0][0] = 0;
		ggcd[0][1] = 1;
		for (int i = 0; i < NAMPS; i++)
		{
			fracs[i][0][0] = fracs[i][1][0];
			fracs[i][0][1] = fracs[i][1][1];
			fracs[i][1][0] = 0;
			fracs[i][1][1] = 1;
		}
	}
}

static void get_fracs(struct amp state[NAMPS], int fracs[NAMPS][2][2])
{
	for (int i = 0; i < NAMPS; i++)
	{
		if (state[i].ones == 0)
		{
			fracs[i][0][0] = 0;
			fracs[i][0][1] = 1;
		}
		else
		{
			int d = gcd(state[i].ones, 1 << 30);
			fracs[i][0][0] = state[i].ones / d;
			fracs[i][0][1] = (1 << 30) / d;
		}
		if (state[i].root2s == 0)
		{
			fracs[i][1][0] = 0;
			fracs[i][1][1] = 1;
		}
		else
		{
			int d = gcd(state[i].root2s, 1 << 30);
			fracs[i][1][0] = state[i].root2s / d;
			fracs[i][1][1] = (1 << 30) / d;
		}
	}
}

// returns length of string printed
static int print_frac(int frac[2][2], char buf[FRACBUFSIZ])
{
	char bufs[4][16] = {0};
	const char * sep = "";
	int len;

	if (frac[0][0] != 0)
	{
		snprintf(bufs[0], sizeof(bufs[0]), "%d", frac[0][0]);
		if (frac[0][1] != 1)
			snprintf(bufs[1], sizeof(bufs[1]), "/%d", frac[0][1]);
		if (frac[1][0] > 0)
			sep = " + ";
		else if (frac[1][0] < 0)
		{
			sep = " - ";
			frac[1][0] *= -1;
		}
	}
	if (frac[1][0] != 0)
	{
		if (frac[1][0] == -1)
			strcpy(bufs[2], "-s");
		else if (frac[1][0] == 1)
			bufs[2][0] = 's';
		else snprintf(bufs[2], sizeof(bufs[2]), "%ds", frac[1][0]);

		if (frac[1][1] != 1)
			snprintf(bufs[3], sizeof(bufs[3]), "/%d", frac[1][1]);
		if (sep == " - ")
			frac[1][0] *= -1;
	}
	else if (frac[0][0] == 0)
		bufs[0][0] = '0';
	len = (int)snprintf(buf, FRACBUFSIZ, "%s%s%s%s%s", bufs[0], bufs[1], sep, bufs[2], bufs[3]);
	if (len >= FRACBUFSIZ)
		error("Value is too big: %s%s%s%s%s", bufs[0], bufs[1], sep, bufs[2], bufs[3]);
	return len;
}

static void print_fracs(int fracs[NAMPS][2][2])
{
	char buf[FRACBUFSIZ];
	int ggcd[2][2];

	extract_ggcd(fracs, ggcd);

	if (ggcd[0][1] > 1 || ggcd[1][1] > 1)
	{
		print_frac(ggcd, buf);
		printf("%s", buf);
	}

	printf("[");
	for (int i = 0; i < NAMPS; i++)
	{
		print_frac(fracs[i], buf);
		printf("%s%s", buf, i == NAMPS - 1? "": ", ");
	}
	printf("]^T\n");
}

double todouble(int frac[2][2])
{
#define SQRT2 1.4142135623730951L
	double d = (double)frac[0][0]/(double)frac[0][1];
	d += (double)frac[1][0]*SQRT2/(double)frac[1][1];
	return d;
}

static void print_indexed(int fracs[NAMPS][2][2], int bits)
{
	char bufs[NAMPS][FRACBUFSIZ];
	int maxlen = 0;

	for (int i = 0; i < NQBITS; i++)
	{
		if (bits & 1 << NQBITS - 1 >> i)
			printf(" q%d", i);
	}
	printf("\n");

	for (int i = 0; i < NAMPS; i++)
	{
		int len;

		if (i & ~bits)
		{
			i += (1 << ctz(i & ~bits)) - 1;
			continue;
		}

		if((len = print_frac(fracs[i], bufs[i])) > maxlen)
			maxlen = len;
	}

	for (int i = 0; i < NAMPS; i++)
	{
		if (i & ~bits)
		{
			i += (1 << ctz(i & ~bits)) - 1;
			continue;
		}

		if (fracs[i][0][0] != 0 || fracs[i][1][0] != 0)
		{
			for (int j = NQBITS - 1; j >= 0; j--)
			{
				if (~bits & 1 << j)
					continue;
				if (i & (1 << j))
					printf("1");
				else
					printf("0");
			}
			printf(": %*s (% lf)\n", maxlen, bufs[i], todouble(fracs[i]));
		}
	}
}

void print_state(int bits, struct amp * s)
{
	int fracs[NAMPS][2][2];

	get_fracs(s, fracs);
//	print_fracs(fracs);
	printf("State:");
	print_indexed(fracs, bits);
}

void print_probs(int bits, struct amp * s)
{
	int fracs[NAMPS][2][2];
	//struct amp copy[NAMPS];

	//memcpy(copy, state, NAMPS * sizeof(struct amp));
	//for (int i = 0; i < NAMPS; i++)
	//	mult(&copy[i], &state[i]);

	//get_fracs(copy, fracs);
	get_fracs(s, fracs);
	printf("Probabilities:");
	print_indexed(fracs, bits);
}
