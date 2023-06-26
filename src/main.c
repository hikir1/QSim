#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include "main.h"

struct amp state[NAMPS] = {{DENOMINATOR}};
struct amp temp[NAMPS];

// ctrl bits must come before bit
void X2(int bit, int ctrl)
{
	int size = NAMPS >> bit;
	int half = size >> 1;
	for (int i = 0; i < NAMPS; i += size)
	{
		if ((i & ctrl) != ctrl)
			continue;

		memcpy(temp, &state[i], half * sizeof(struct amp));
		memcpy(&state[i], &state[i + half], half * sizeof(struct amp));
		memcpy(&state[i + half], temp, half * sizeof(struct amp));
	}
}

void X(int bit, int ctrl)
{
	int jctrl = ctrl & ((1 << NQBITS - 1 - bit) - 1);
	if (!jctrl)
	{
		X2(bit, ctrl);
		return;
	}

	int ictrl = ctrl & ~jctrl;
	int size = NAMPS >> bit;
	int half = size >> 1;
	for (int i = 0; i < NAMPS; i += size)
	{
		if ((i & ictrl) != ictrl)
			continue;

		for (int j = 0; j < half; j++)
		{
			struct amp temp;

			if ((j & jctrl) != jctrl)
				continue;

			temp = state[i + j];
			state[i + j] = state[i + half + j];
			state[i + half + j] = temp;
		}
	}
}

void H(int bit, int ctrl)
{
	int jctrl = ctrl & ((1 << NQBITS - 1 - bit) - 1);
	int ictrl = ctrl & ~jctrl;
	int size = NAMPS >> bit;
	int half = size >> 1;
	for (int i = 0; i < NAMPS; i += size)
	{
		if ((i & ictrl) != ictrl)
			continue;

		for (int j = 0; j < half; j++)
		{
			struct amp temp;

			if ((j & jctrl) != jctrl)
				continue;

			mult(&state[i + j], &iroot2);
			mult(&state[i + half + j], &iroot2);

			temp = state[i + j];
			add(&state[i + j], &state[i + half + j]);
			neg(&state[i + half + j]);
			add(&state[i + half + j], &temp);
		}
	}
}

void Z(int bit, int ctrl)
{
	int jctrl = ctrl & ((1 << NQBITS - 1 - bit) - 1);
	int ictrl = ctrl & ~jctrl;
	int size = NAMPS >> bit;
	int half = size >> 1;
	for (int i = 0; i < NAMPS; i += size)
	{
		if ((i & ictrl) != ictrl)
			continue;

		for (int j = 0; j < half; j++)
		{
			if ((j & jctrl) != jctrl)
				continue;

			neg(&state[i + half + j]);
		}
	}
}

// a < b
// ctrl bits must come before b
void SWAP2(int a, int b, int ctrl)
{
	int jctrl = ctrl & ((1 << NQBITS - 1 - a) - 1);
	int ictrl = ctrl & ~jctrl;
	int asize = NAMPS >> a;
	int ahalf = asize >> 1;
	int bsize = NAMPS >> b;
	int bhalf = bsize >> 1;
	for (int i = 0; i < NAMPS; i += asize)
	{
		if ((i & ictrl) != ictrl)
			continue;

		for (int j = bhalf; j < ahalf; j += bsize)
		{
			if ((j & jctrl) != jctrl)
				continue;

			memcpy(temp, &state[i + j], bhalf * sizeof(struct amp));
			memcpy(&state[i + j], &state[i + ahalf + j - bhalf], bhalf * sizeof(struct amp));
			memcpy(&state[i + ahalf + j - bhalf], temp, bhalf * sizeof(struct amp));
		}
	}

}

// like an X, but half one bit and half another
void SWAP(int a, int b, int ctrl)
{
	if (b < a)
	{
		a ^= b;
		b ^= a;
		a ^= b;
	}

	int kctrl = ctrl & ((1 << NQBITS - 1 - b) - 1);
	if (!kctrl)
	{
		SWAP2(a, b, ctrl);
		return;
	}

	int jctrl = ctrl & ~kctrl & ((1 << NQBITS - 1 - a) - 1);
	int ictrl = ctrl & ~(jctrl & kctrl);
	int asize = NAMPS >> a;
	int ahalf = asize >> 1;
	int bsize = NAMPS >> b;
	int bhalf = bsize >> 1;
	for (int i = 0; i < NAMPS; i += asize)
	{
		if ((i & ictrl) != ictrl)
			continue;

		for (int j = bhalf; j < ahalf; j += bsize)
		{
			if ((j & jctrl) != jctrl)
				continue;

			for (int k = 0; k < bhalf; k++)
			{
				struct amp temp;

				if ((k & kctrl) != kctrl)
					continue;

				temp = state[i + j + k];
				state[i + j + k] = state[i + ahalf + j - bhalf + k];
				state[i + ahalf + j - bhalf + k] = temp;
			}
		}
	}
}

int measure(int bit)
{
	static int seeded = false;
	int size = NAMPS >> bit;
	int half = size >> 1;
	int newval;

	struct amp prob = {0};
	for (int i = 0; i < NAMPS; i += size)
	{
		for (int j = half; j < size; j++)
		{
			struct amp temp = state[i + j];

			mult(&temp, &temp);
			add(&prob, &temp);
		}
	}

	if (!seeded)
		srand(time(NULL));

	// measure
	int isone = (double)rand()/RAND_MAX < (double)prob.ones/DENOMINATOR;

	// sqrt log2, assume prob is power of 2
	int tz = isone? ctz(prob.ones): ctz(DENOMINATOR - prob.ones);
	int scale = DENOMINATOR_BITS/2 - tz/2;
	int scalestart = isone * half;
	for (int i = 0; i < NAMPS; i += size)
	{
		for (int j = scalestart; j < scalestart + half; j++)
		{
			state[i + j].ones <<= scale;
			state[i + j].root2s <<= scale;

			if (tz & 1)
				mult(&state[i + j], &iroot2);
		}
		for (int j = half - scalestart; j < half*2 - scalestart; j++)
		{
			state[i + j].ones = 0;
			state[i + j].root2s = 0;
		}
	}
			
	return isone;
}

int hash_args(int bits, const int * args, int argc)
{
	int hash = 0;
	for (int i = 0; i < argc; i++)
	{
		hash <<= 1;
		hash |= !!(bits & (1 << NQBITS - 1 >> args[i]));
	}
	return hash;
}

// this is very similar to CX, except uses f in addition to ctrl
void Uf(int bit, struct func * func, const int * args, int ctrl)
{
	int jctrl = ctrl & ((1 << NQBITS - 1 - bit) - 1);
	int ictrl = ctrl & ~jctrl;
	int size = NAMPS >> bit;
	int half = size >> 1;
	for (int i = 0; i < NAMPS; i += size)
	{
		if ((i & ictrl) != ictrl)
			continue;

		for (int j = 0; j < half; j++)
		{
			struct amp temp;

			if ((j & jctrl) != jctrl)
				continue;
			if (!func->map[hash_args(i + j, args, func->argc)])
				continue;

			temp = state[i + j];
			state[i + j] = state[i + half + j];
			state[i + half + j] = temp;
		}
	}
}

static void copy_state(struct amp a[NAMPS], const struct amp b[NAMPS])
{
	memcpy(a, b, NAMPS * sizeof(struct amp));
}

static void to_probs(struct amp * s)
{
	for (int i = 0; i < NAMPS; i++)
		mult(&s[i], &s[i]);
}

// TODO Not sure if this is most efficient or convenient
static void merge_bits(int bits, struct amp * s)
{
	int size = NAMPS;

	for (int i = 0; i < NQBITS; i++)
	{
		int half = size >> 1;

		if (bits & half)
		{
			for (int j = 0; j < NAMPS; j += size)
				for (int k = 0; k < half; k++)
					add(&s[j + k], &s[j + k + half]);

		}
		size = half;
	}
}

void run(struct gate * gates, int ngates, int start)
{
	for (int i = start; i < ngates; i++)
	{
		gates[i].cnt++;
		switch (gates[i].type)
		{
			case GATE_X:
				X(gates[i].bits[0], gates[i].ctrl);
				break;
			case GATE_H:
				H(gates[i].bits[0], gates[i].ctrl);
				break;
			case GATE_Uf:
				Uf(gates[i].bits[gates[i].func->argc], gates[i].func, gates[i].bits, gates[i].ctrl);
				break;
			case GATE_Z:
				Z(gates[i].bits[0], gates[i].ctrl);
				break;
			case GATE_SWAP:
				SWAP(gates[i].bits[0], gates[i].bits[1], gates[i].ctrl);
				break;
			case GATE_MEASURE:
				gates[i].mstate = measure(gates[i].bits[0])? MSTATE_1: MSTATE_0;
				break;
			case GATE_BARRIER_BEGIN:
				while (gates[i].barrier.end)
				{
					for (int j = 0; j < gates[i].barrier.repeat; j++)
						run(gates, ngates, i + 1);
					i = gates[i].barrier.end;
				}
				break;
			case GATE_BARRIER_END:
				return;
			case GATE_PAUSE:
				getc(stdin);
				break;
			case GATE_DRAW:
				print_circuit(gates, ngates);
				puts("");
				break;
			case GATE_STATE:
				copy_state(temp, state);
				merge_bits(~gates[i].ctrl, temp);
				print_state(gates[i].ctrl, temp);
				puts("");
				break;
			case GATE_PROBS:
				copy_state(temp, state);
				to_probs(temp);
				merge_bits(~gates[i].ctrl, temp);
				print_probs(gates[i].ctrl, temp);
				puts("");
				break;
			case GATE_PFUNC:
				print_func(gates[i].func);
				puts("");
				break;
			default:
				error("Strange gate type: %d", gates[i].type);
		}
	}
}

int main(int argc, char ** argv)
{
	FILE * in;
	struct gate gates[MAXGATES] = {0};
	int ngates;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <file>\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (!(in = fopen(argv[1], "r")))
		error("Failed to read input file");
	
	ngates = parse_circuit(gates, in);
	fclose(in);

	puts("");
	run(gates, ngates, 0);
}
