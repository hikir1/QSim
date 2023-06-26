#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "main.h"

#ifndef _WIN32
#include <unistd.h>
#endif

enum nodetype {
	NODE_NONE,
	NODE_CTRL,
	NODE_X,
	NODE_H,
	NODE_Z,
	NODE_Uf,
	NODE_BOX,
	NODE_SWAP,
	NODE_BARRIER,
	NODE_MEASURE_UNKNOWN,
	NODE_MEASURE_0,
	NODE_MEASURE_1
};

enum wire {
	WIRE_NONE = 0,
	WIRE_TOP = 1,
	WIRE_BOT = 2,
	WIRE_BOTH = WIRE_TOP | WIRE_BOT,
	WIRE_BOXTOP = 4,
	WIRE_BOXBOT = 8,
	WIRE_BOXBOTH = WIRE_BOXTOP | WIRE_BOXBOT,
	WIRE_UfTOP = 0x10,
	WIRE_UfBOT = 0x20
} wire;

struct node {
	enum nodetype type;
	enum wire wire;
	int fname;
	bool past;
};

#define SPACEPAD "        "
#define LINEPAD "--------"
#define BORDERPAD "========"

bool docolor()
{
#ifdef _WIN32
	return false;
#else
	return isatty(fileno(stdout));
#endif
}

static void add_topctrl(struct node nodes[NQBITS][PRIMAXCOLS], int * idx, const struct gate * gate, int mincol, bool past)
{
	int start;

	if (!gate->ctrl)
		return;

	start = clz(gate->ctrl) - (sizeof(gate->ctrl)*8 - NQBITS);
	if (start > gate->bits[0])
		return;
	
	nodes[start][mincol].type = NODE_CTRL;
	nodes[start][mincol].wire = WIRE_BOT;
	nodes[start][mincol].past = past;
	idx[start] = mincol + 1;
	for (int i = start + 1; i < gate->bits[0]; i++)
	{
		if (gate->ctrl & ctrlbit(i))
			nodes[i][mincol].type = NODE_CTRL;
		nodes[i][mincol].wire = WIRE_BOTH;
		nodes[i][mincol].past = past;
		idx[i] = mincol + 1;
	}

	nodes[gate->bits[0]][mincol].wire |= WIRE_TOP;
	nodes[gate->bits[0]][mincol].past = past;
}

static void add_botctrl(struct node nodes[NQBITS][PRIMAXCOLS], int * idx, const struct gate * gate, int mincol, bool past)
{
	int end;

	if (!gate->ctrl)
		return;

	end = NQBITS - 1 - ctz(gate->ctrl);
	if (end < gate->bits[0])
		return;

	nodes[gate->bits[0]][mincol].wire |= WIRE_BOT;
	nodes[gate->bits[0]][mincol].past = past;
	
	for (int i = gate->bits[0] + 1; i < end; i++)
	{
		if (gate->ctrl & ctrlbit(i))
			nodes[i][mincol].type = NODE_CTRL;
		nodes[i][mincol].wire = WIRE_BOTH;
		nodes[i][mincol].past = past;
		idx[i] = mincol + 1;
	}
	nodes[end][mincol].type = NODE_CTRL;
	nodes[end][mincol].wire = WIRE_TOP;
	nodes[end][mincol].past = past;
	idx[end] = mincol + 1;
}

static void add_swap(struct node nodes[NQBITS][PRIMAXCOLS], int * idx, const struct gate * gate, bool past)
{
	int mincol = 0;
	int start, end;

	if (gate->bits[0] < gate->bits[1])
	{
		start = gate->bits[0];
		end = gate->bits[1];
	}
	else
	{
		start = gate->bits[1];
		end = gate->bits[0];
	}

	for (int i = 0; i < NQBITS; i++)
		if (gate->ctrl & ctrlbit(i) && idx[i] > mincol)
			mincol = idx[i];
	for (int i = start; i <= end; i++)
		if (idx[i] > mincol)
			mincol = idx[i];
	if (mincol >= PRIMAXCOLS)
		error("Circuit is too big to print!");

	add_topctrl(nodes, idx, gate, mincol, past);

	nodes[start][mincol].type = NODE_SWAP;
	nodes[start][mincol].wire |= WIRE_BOT;
	nodes[start][mincol].past = past;
	idx[start] = mincol + 1;
	for (int i = start + 1; i < end; i++)
	{
		nodes[i][mincol].wire = WIRE_BOTH;
		nodes[i][mincol].past = past;
		idx[i] = mincol + 1;
	}
	nodes[end][mincol].type = NODE_SWAP;
	nodes[end][mincol].wire |= WIRE_TOP;
	nodes[end][mincol].past = past;
	idx[end] = mincol + 1;

	add_botctrl(nodes, idx, gate, mincol, past);
}

static int add_Ufswaps(struct node nodes[NQBITS][PRIMAXCOLS], int * idx, const struct gate * gate, int * start, bool past)
{
	int sorted[NQBITS];
	int newctrl = gate->ctrl;

	for (int i = 0; i < NQBITS; i++)
		sorted[i] = i;

	if (gate->bits[0] + gate->func->argc < NQBITS)
		*start = gate->bits[0];
	else
		*start = 0;

	for (int i = 0; i < gate->func->argc + 1; i++)
	{
		int sorted_i = *start + i;
		if (sorted[sorted_i] == gate->bits[i])
			continue;
		for (int j = 0; j < NQBITS; j++)
		{
			if (j == sorted_i)
				continue;

			if (sorted[j] == gate->bits[i])
			{
				struct gate swap = {.type=GATE_SWAP, .ctrl=0, .bits={sorted_i, j}};
				add_swap(nodes, idx, &swap, past);

				sorted[sorted_i] ^= sorted[j];
				sorted[j] ^= sorted[i];
				sorted[sorted_i] ^= sorted[j];

				newctrl ^= (newctrl & ctrlbit(sorted_i)) >> j - sorted_i;
				newctrl ^= (newctrl & ctrlbit(j)) << j - sorted_i;
				newctrl ^= (newctrl & ctrlbit(sorted_i)) >> j - sorted_i;
				break;
			}
		}
	}
	return newctrl;
}

static void add_Uf(struct node nodes[NQBITS][PRIMAXCOLS], int * idx, const struct gate * gate, int * pad, bool past)
{
	int mincol = 0;
	int start = 0;
	struct gate copy = *gate;

	copy.ctrl = add_Ufswaps(nodes, idx, gate, &start, past);

	for (int i = 0; i < gate->func->argc + 1; i++)
		if (idx[start + i] > mincol)
			mincol = idx[start + i];
	for (int i = gate->func->argc + 1; i < NQBITS; i++)
		if (copy.ctrl & ctrlbit(i) && idx[i] > mincol)
			mincol = idx[i];
	if (mincol >= PRIMAXCOLS)
		error("Circuit is too big to print!");

	if (pad[mincol] < 2)
		pad[mincol] = 2;

	copy.bits[0] = start;
	add_topctrl(nodes, idx, &copy, mincol, past);

	nodes[start][mincol].type = NODE_BOX;
	nodes[start][mincol].wire |= WIRE_BOXBOT;
	nodes[start][mincol].past = past;
	idx[start] = mincol + 1;
	for (int i = start + 1; i < start + gate->func->argc; i++)
	{
		nodes[i][mincol].type = NODE_BOX;
		nodes[i][mincol].wire = WIRE_BOXBOTH;
		nodes[i][mincol].past = past;
		idx[i] = mincol + 1;
	}
	nodes[start + gate->func->argc][mincol].type = NODE_BOX;
	nodes[start + gate->func->argc][mincol].wire |= WIRE_BOXTOP;
	nodes[start + gate->func->argc][mincol].past = past;
	idx[start + gate->func->argc] = mincol + 1;

	if (gate->func->argc & 1)
	{
		nodes[start + gate->func->argc/2][mincol].wire |= WIRE_UfBOT;
		nodes[start + gate->func->argc/2][mincol].fname = gate->func->name;
		nodes[start + gate->func->argc/2 + 1][mincol].wire |= WIRE_UfTOP;
		nodes[start + gate->func->argc/2 + 1][mincol].fname = gate->func->name;
	}
	else
	{
		nodes[start + gate->func->argc/2][mincol].type = NODE_Uf;
		nodes[start + gate->func->argc/2][mincol].fname = gate->func->name;
	}

	copy.bits[0] = start + gate->func->argc;
	add_botctrl(nodes, idx, &copy, mincol, past);

	add_Ufswaps(nodes, idx, gate, &start, past);
}

static void add_gate(struct node nodes[NQBITS][PRIMAXCOLS], int * idx, const struct gate * gate, int * pad, bool past)
{
	int mincol = idx[gate->bits[0]];
	for (int i = 0; i < NQBITS; i++)
		if (gate->ctrl & ctrlbit(i) && idx[i] > mincol)
			mincol = idx[i];
	if (mincol >= PRIMAXCOLS)
		error("Circuit is too big to print!");

	switch (gate->type)
	{
		case GATE_X:
			nodes[gate->bits[0]][mincol].type = NODE_X;
			break;
		case GATE_H:
			nodes[gate->bits[0]][mincol].type = NODE_H;
			break;
		case GATE_Z:
			nodes[gate->bits[0]][mincol].type = NODE_Z;
			break;
		case GATE_MEASURE:
			switch (gate->mstate)
			{
				case MSTATE_UNKNOWN:
					nodes[gate->bits[0]][mincol].type = NODE_MEASURE_UNKNOWN;
					break;
				case MSTATE_0:
					nodes[gate->bits[0]][mincol].type = NODE_MEASURE_0;
					break;
				case MSTATE_1:
					nodes[gate->bits[0]][mincol].type = NODE_MEASURE_1;
					break;
				default:
					error("Unkown measure state: %d", gate->mstate);
			}
			break;
		case GATE_PAUSE:
		case GATE_DRAW:
		case GATE_STATE:
		case GATE_PROBS:
		case GATE_PFUNC:
			return;
		default:
			error("Unknown gate type: %d", gate->type);
	}

	add_topctrl(nodes, idx, gate, mincol, past);
	idx[gate->bits[0]] = mincol + 1;
	nodes[gate->bits[0]][mincol].past = past;
	add_botctrl(nodes, idx, gate, mincol, past);
}

static void add_barrier(struct node nodes[NQBITS][PRIMAXCOLS], int * idx, const struct gate * gate, int * pad, bool past)
{
	int mincol = 0;
	for (int i = 0; i < NQBITS; i++)
		if (idx[i] > mincol)
			mincol = idx[i];
	if (mincol >= PRIMAXCOLS)
		error("Circuit is too big to print!");

	for (int i = 0; i < NQBITS; i++)
	{
		idx[i] = mincol + 1;
		nodes[i][mincol].type = NODE_BARRIER;
		nodes[i][mincol].past = past;
	}
}

static void add_nodes(const struct gate * gates, int ngates, int i,
	struct node nodes[NQBITS][PRIMAXCOLS], int * idx, int * pad, int * cnt)
{
	for (; i < ngates; i++)
	{
		cnt[i] += 1;
		if (gates[i].type == GATE_Uf)
			add_Uf(nodes, idx, &gates[i], pad, gates[i].cnt >= cnt[i]);
		else if (gates[i].type == GATE_SWAP)
			add_swap(nodes, idx, &gates[i], gates[i].cnt >= cnt[i]);
		else if (gates[i].type == GATE_BARRIER_BEGIN)
		{
			add_barrier(nodes, idx, &gates[i], pad, gates[i].cnt >= cnt[i]);
			while (gates[i].barrier.end)
			{
				for (int j = 0; j < gates[i].barrier.repeat; j++)
				{
					add_nodes(gates, ngates, i + 1, nodes, idx, pad, cnt);
					add_barrier(nodes, idx, &gates[i], pad, gates[i].cnt >= cnt[i]);
				}
				i = gates[i].barrier.end;
			}
		}
		else if (gates[i].type == GATE_BARRIER_END)
			return;
		else
			add_gate(nodes, idx, &gates[i], pad, gates[i].cnt >= cnt[i]);
	}
}

void print_circuit(const struct gate * gates, int ngates)
{
	struct node nodes[NQBITS][PRIMAXCOLS] = {0};
	int idx[NQBITS] = {0};
	int pad[MAXGATES] = {0};
	int len = 0;
	int nqbits = 0;
	int cnt[MAXGATES] = {0};

	const char * pastc = "", * setcmes = "", * resetc = "";
	if (docolor())
	{
		pastc = "\x1b[34;1m"; // blue
		setcmes = "\x1b[32;1m"; // green
		resetc = "\x1b[0m";
	}

	add_nodes(gates, ngates, 0, nodes, idx, pad, cnt);

	for (int i = 0; i < NQBITS; i++)
	{
		if (idx[i] > 0)
		{
			if (nodes[i][idx[i] - 1].type == NODE_BARRIER)
			{
				bool empty = true;
				for (int j = 0; j < idx[i]; j++)
				{
					if (nodes[i][j].type && nodes[i][j].type != NODE_BARRIER)
					{
						empty = false;
						break;
					}
				}
				if (empty)
					continue;
			}
			nqbits = i + 1;
			if (idx[i] > len)
				len = idx[i];
		}
	}

	for (int i = 0; i < nqbits; i++)
	{
		if (i == 0)
		{
			printf("   =");
			for (int j = 0; j < len; j++)
				printf("%.*s====%.*s", pad[j]/2, BORDERPAD, (pad[j] + 1)/2, BORDERPAD);
			printf("\n");
		}
		else
		{
			printf("    ");
			for (int j = 0; j < len; j++)
			{
				const char * setc = nodes[i][j].past? pastc: "";

				if (nodes[i][j].wire & WIRE_TOP)
					printf("%.*s %s|%s  %.*s", pad[j]/2, SPACEPAD, setc, resetc, (pad[j] + 1)/2, SPACEPAD);
				else if (nodes[i][j].wire & WIRE_UfTOP)
					printf("%.*s%s[ %c ]%s %.*s", pad[j]/2 - 1, SPACEPAD,
							setc, nodes[i][j].fname, resetc, (pad[j] + 1)/2 - 1, SPACEPAD);
				else if (nodes[i][j].wire & WIRE_BOXTOP)
					printf("%.*s%s[   ]%s %.*s", pad[j]/2 - 1, SPACEPAD,
							setc, resetc, (pad[j] + 1)/2 - 1, SPACEPAD);
				else if (nodes[i][j].type == NODE_BARRIER)
					printf(" |  ");
				else
					printf("%.*s    %.*s", pad[j]/2, SPACEPAD, (pad[j] + 1)/2, SPACEPAD);
			}
			printf("\n");
		}

		printf("q%d -", i);
		for (int j = 0; j < len; j++)
		{
			const char * setc = nodes[i][j].past? pastc: "";

			switch (nodes[i][j].type)
			{
				case NODE_NONE:
					if (nodes[i][j].wire == WIRE_BOTH)
						printf("%.*s-%s|%s--%.*s", pad[j]/2, LINEPAD,
								setc, resetc, (pad[j] + 1)/2, LINEPAD);
					else
						printf("%.*s----%.*s", pad[j]/2, LINEPAD, (pad[j] + 1)/2, LINEPAD);
					continue;
				case NODE_CTRL:
					printf("%.*s-%so%s--%.*s", pad[j]/2, LINEPAD,
							setc, resetc, (pad[j] + 1)/2, LINEPAD);
					continue;
				case NODE_X:
					printf("%.*s%s(+)%s-%.*s", pad[j]/2, LINEPAD,
							setc, resetc, (pad[j] + 1)/2, LINEPAD);
					continue;
				case NODE_H:
					printf("%.*s%s[H]%s-%.*s", pad[j]/2, LINEPAD,
							setc, resetc, (pad[j] + 1)/2, LINEPAD);
					continue;
				case NODE_Z:
					printf("%.*s%s[Z]%s-%.*s", pad[j]/2, LINEPAD,
							setc, resetc, (pad[j] + 1)/2, LINEPAD);
					continue;
				case NODE_SWAP:
					printf("%.*s-%sX%s--%.*s", pad[j]/2, LINEPAD,
							setc, resetc, (pad[j] + 1)/2, LINEPAD);
					continue;
				case NODE_Uf:
					printf("%.*s%s[ %c ]%s-%.*s", pad[j]/2 - 1, LINEPAD,
							setc, nodes[i][j].fname, resetc, (pad[j] + 1)/2 - 1, LINEPAD);
					continue;
				case NODE_BOX:
					printf("%.*s%s[   ]%s-%.*s", pad[j]/2 - 1, LINEPAD,
							setc, resetc, (pad[j] + 1)/2 - 1, LINEPAD);
					continue;
				case NODE_MEASURE_UNKNOWN:
					printf("%.*s%s[?]%s-%.*s", pad[j]/2, LINEPAD,
							setc, resetc, (pad[j] + 1)/2, LINEPAD);
					continue;
				case NODE_MEASURE_0:
					printf("%.*s%s[%s%s0%s%s]%s-%.*s", pad[j]/2, LINEPAD,
							setc, resetc, setcmes, resetc, setc, resetc, (pad[j] + 1)/2, LINEPAD);
					continue;
				case NODE_MEASURE_1:
					printf("%.*s%s[%s%s1%s%s]%s-%.*s", pad[j]/2, LINEPAD,
							setc, resetc, setcmes, resetc, setc, resetc, (pad[j] + 1)/2, LINEPAD);
					continue;
				case NODE_BARRIER:
					printf("-|--");
					continue;
				default:
					error("Uknown node type: %d", nodes[i][j].type);
			}
		}
		printf("\n");
	}
	printf("   =");
	for (int j = 0; j < len; j++)
		printf("%.*s====%.*s", pad[j]/2, BORDERPAD, (pad[j] + 1)/2, BORDERPAD);
	printf("\n");
}
