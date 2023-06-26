#ifndef MAIN_H
#define MAIN_H

#define NQBITS 10
#define FMAXOPS 128
#define BUFSIZE 64
#define MAXGATES 128
#define VALID_GATES "X, H, Z, W (SWAP), U (boolean function), M (measure)"
#define NFUNCS 8
#define FRACBUFSIZ 32
#define DENOMINATOR_BITS 30
#define DENOMINATOR (1 << DENOMINATOR_BITS)
#define PRIMAXCOLS MAXGATES
#define NAMPS (1 << NQBITS)

#ifdef _MSC_VER
#include <intrin.h>
#endif

#define error(MSG, ...) (fprintf(stderr, MSG "\n", ##__VA_ARGS__), exit(EXIT_FAILURE))

struct func {
	int name;
	int map[NAMPS];
	int argc;
};

extern struct func funcs[NFUNCS];

void parse_func(const char *, int);
void print_func(const struct func *);

enum gatetype {
	GATE_NONE,
	GATE_X,
	GATE_H,
	GATE_Uf,
	GATE_Z,
	GATE_SWAP,
	GATE_MEASURE,
	GATE_BARRIER_BEGIN,
	GATE_BARRIER_END,
	GATE_PAUSE,
	GATE_STATE,
	GATE_PROBS,
	GATE_DRAW,
	GATE_PFUNC
};

enum mstate {
	MSTATE_UNKNOWN,
	MSTATE_0,
	MSTATE_1
};

struct gate {
	enum gatetype type;
	int ctrl;
	union {
		int bits[NQBITS];
		struct barrier {
			int name;
			int end;
			int repeat;
			struct gate * prev;
		} barrier;
	};
	union {
		struct func * func;
		enum mstate mstate;
	};
	int cnt;
};

static const enum gatetype gatemap[0xff] =
{
	['X'] = GATE_X,
	['H'] = GATE_H,
	['U'] = GATE_Uf,
	['Z'] = GATE_Z,
	['W'] = GATE_SWAP,
	['M'] = GATE_MEASURE,
};

#define rgatemap "\0XHUZWM"

struct amp {
	int ones;
	int root2s;
};

int parse_circuit(struct gate *, FILE *);
void print_circuit(const struct gate *, int ngates);
void print_state(int, struct amp *);
void print_probs(int, struct amp *);

extern struct amp state[NAMPS];

static const struct amp iroot2 = {0, DENOMINATOR >> 1};

static inline void mult(struct amp * a, const struct amp * b)
{
	// Assume >> is arithmetic shift. Works with gcc.
	int a1 = a->ones >> 15;
	int a2 = a->root2s >> 15;
	int b1 = b->ones >> 15;
	int b2 = b->root2s >> 15;
	a->ones = a1 * b1 + a2 * b2 * 2;
	a->root2s = a1 * b2 + a2 * b1;
}

static inline void add(struct amp * a, const struct amp * b)
{
	//printf("a: (%d, %d)\n", a->ones, a->root2s);
	a->ones += b->ones;
	a->root2s += b->root2s;
}

static inline void neg(struct amp * a)
{
	a->ones = -a->ones;
	a->root2s = -a->root2s;
}

static inline int ctrlbit(int idx)
{
	return 1 << NQBITS - 1 >> idx;
}

static inline int popcount(int x)
{
	#ifdef __GNUC__
	return __builtin_popcount((unsigned)x);
	#elif _MSC_VER
	return __popcnt((unsigned)x);
	#else
	int cnt = 0;
	for (int i = 1; i; i <<= 1)
		cnt += !!(x & i);
	return cnt;
	#endif
}

static inline int clz(int x)
{
	#ifdef __GNUC__
	return x? __builtin_clz(x): sizeof(int)*8;
	#elif _MSC_VER
	unsigned long idx;
	return _BitScanReverse(&idx, (unsigned long)x)? (int)idx: sizeof(int)*8;
	#else
	for (int i = 0; i < sizeof(int)*8; i++)
		if (x >> sizeof(int)*8 - 1 - i & 1)
			return i;
	return sizeof(int)*8;
	#endif
}

static inline int ctz(int x)
{
	#ifdef __GNUC__
	return x? __builtin_ctz(x): sizeof(int)*8;
	#elif _MSC_VER
	unsigned long idx;
	return _BitScanForward(&idx, (unsigned long)x)? (int)idx: sizeof(int)*8;
	#else
	for (int i = 0; i < sizeof(int)*8; i++)
		if (x >> i & 1)
			return i;
	return sizeof(int)*8;
	#endif
}

#endif
