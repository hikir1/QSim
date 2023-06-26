#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include "main.h"

static void parse_range(const char * s, int * sidx, int lineno,
		int * start, int * start_idx, int * stop, int * stop_idx)
{
	*start = s[*sidx] & 0xf;
	*start_idx = *sidx;

	++*sidx;
	while (isspace(s[*sidx]))
		++*sidx;

	if (s[*sidx] == '.' && s[*sidx + 1] == '.')
	{
		*sidx += 2;
		while (isspace(s[*sidx]))
			++*sidx;
		if (!isdigit(s[*sidx]))
			error("Line %d: Missing index at end of range\n%s\n%*s~~~ Here",
				lineno, s, *sidx + 1, "^");

		*stop = (s[*sidx] & 0xf) + 1;
		*stop_idx = *sidx;
		if (*stop < *start)
			error("Line %d: End index cannot be smaller than start index\n%s\n%*s~~~ Here",
				lineno, s, *sidx + 1, "^");

		++*sidx;
	}
	else
	{
		*stop = *start + 1;
		*stop_idx = *start_idx;
	}
}

static int parse_bits(const char * s, int * sidx, struct gate * gates, int * gidx, int lineno)
{
	int nbits = 0;
	int bits = 0;
	while (1)
	{
		int start, stop;
		int start_idx, stop_idx;

		while (isspace((int)s[*sidx]))
			++*sidx;
		if (!isdigit((int)s[*sidx]))
			break;

		parse_range(s, sidx, lineno, &start, &start_idx, &stop, &stop_idx);

		for (int i = start; i < stop; i++)
		{
			if (nbits >= sizeof(gates->bits)/sizeof(*gates->bits))
				error("Line %d: Too many input bits for gate %c.\n"
						"%s\n%*s~~~ Here",
						lineno, rgatemap[gates[*gidx].type], s, stop_idx + 1, "^");
			if (bits & ctrlbit(i))
				error("Line %d: Duplicate input bit '%d'\n%s\n%*s~~~ Here",
						lineno, i, s, start_idx + 1, "^");
			bits |= ctrlbit(i);

			gates[*gidx].bits[nbits] = i;
			nbits++;
		}
	}
	switch (gates[*gidx].type)
	{
		case GATE_Uf:
			if (nbits != gates[*gidx].func->argc + 1)
				error("Line %d: Gate %c takes %d input bits. "
						"%d given.\n%s\n%*s~~~ Here", lineno, rgatemap[gates[*gidx].type],
						gates[*gidx].func->argc + 1, nbits, s, *sidx + 1, "^");
			break;
		case GATE_SWAP:
			if (nbits != 2)
				error("Line %d: SWAP gate takes 2 input bits. "
						"%d given.\n%s\n%*s~~~ Here", lineno, nbits, s, *sidx + 1, "^");
			break;
		default:
			break;
	}
	return bits;
}

static void parse_ctrl(const char * s, int * sidx, struct gate * gates, int * gidx, int lineno, int bits)
{
	gates[*gidx].ctrl = 0;

	while (isspace((int)s[*sidx]))
		++*sidx;
	if (s[*sidx] != ':')
		return;
	++*sidx;

	while (1)
	{
		int start, stop;
		int start_idx, stop_idx;

		while (isspace((int)s[*sidx]))
			++*sidx;
		if (!isdigit((int)s[*sidx]))
			break;

		parse_range(s, sidx, lineno, &start, &start_idx, &stop, &stop_idx);

		for (int i = start; i < stop; i++)
		{
			if (gates[*gidx].ctrl & ctrlbit(i))
				error("Line %d: Duplicate control bit '%d'\n%s\n%*s~~~ Here",
						lineno, i, s, start_idx + 1, "^");
			if (bits & ctrlbit(i))
				error("Line %d: Input bit '%d' used as control bit\n%s\n%*s~~~ Here",
						lineno, i, s, start_idx + 1, "^");

			gates[*gidx].ctrl |= ctrlbit(i);
		}
	}

	if (gates[*gidx].type == GATE_MEASURE && gates[*gidx].ctrl)
		error("Line %d: Can't control measure operator", lineno);
}

static void parse_gate(const char * s, int * sidx, struct gate * gates, int * gidx, int lineno)
{
	int bits;

	while (isspace((int)s[*sidx]))
		++*sidx;
	if (!s[*sidx] || s[*sidx] == '#')
		return;

	if (*gidx >= MAXGATES)
		error("Line %d: Circuit is too large. The limit is %d gates", lineno, MAXGATES);
	
	if (!gatemap[s[*sidx]])
		error("Line %d: Unknown gate\n%s\n%*s~~~ Here\n"
				"Valid gates are " VALID_GATES, lineno, s, *sidx + 1, "^");

	gates[*gidx].type = gatemap[s[*sidx]];
	++*sidx;

	if (gates[*gidx].type == GATE_Uf)
	{
		if (s[*sidx] < 'a' || s[*sidx] >= 'a' + NFUNCS)
			error("Line %d: Expected function name\n%s\n%*s~~~ Here",
					lineno, s, *sidx + 1, "^");
		if (!funcs[s[*sidx] - 'a'].name)
			error("Line %d: Function '%c' not defined\n%s\n%*s~~~ Here",
					lineno, s[*sidx], s, *sidx + 1, "^");
		gates[*gidx].func = &funcs[s[*sidx] - 'a'];
		++*sidx;
	}

	bits = parse_bits(s, sidx, gates, gidx, lineno);
	parse_ctrl(s, sidx, gates, gidx, lineno, bits);

	if (s[*sidx] && s[*sidx] != '#')
		error("Line %d: Unexpected symbol\n%s\n%*s~~~ What's that?",
				lineno, s, *sidx + 1, "^");

	++*gidx;

	if (gates[*gidx - 1].type == GATE_H
			|| gates[*gidx - 1].type == GATE_X
			|| gates[*gidx - 1].type == GATE_Z)
	{
		for (int i = 1; i < popcount(bits); i++)
		{
			if (*gidx >= MAXGATES)
				error("Line %d: Reached circuit limit while expanding gate %c",
						lineno, rgatemap[gates[*gidx - 1].type]);

			gates[*gidx].type = gates[*gidx - i].type;
			gates[*gidx].bits[0] = gates[*gidx - i].bits[i];
			gates[*gidx].ctrl = gates[*gidx - i].ctrl;

			++*gidx;
		}
	}
}

static void parse_command(const char * s, int sidx, struct gate * gates, int * gidx, int lineno)
{
	if (strncmp(s + sidx, "draw", 4) == 0
			&& (!s[sidx + 4] || isspace(s[sidx + 4])))
	{
		gates[*gidx].type = GATE_DRAW;
		sidx += 4;
		++*gidx;
	}
	else if (strncmp(s + sidx, "pause", 5) == 0
			&& (!s[sidx + 5] || isspace(s[sidx + 5])))
	{
		gates[*gidx].type = GATE_PAUSE;
		sidx += 5;
		++*gidx;
	}
	else if (strncmp(s + sidx, "pfunc", 5) == 0
			&& (!s[sidx + 5] || isspace(s[sidx + 5])))
	{
		gates[*gidx].type = GATE_PFUNC;
		sidx += 5;

		while (isspace(s[sidx]))
			sidx++;
		if (!islower(s[sidx]))
			error("Line %d: Expected function name\n%s\n%*s~~~ Here",
					lineno, s, sidx + 1, "^");
		if (s[sidx] < 'a' || s[sidx] >= 'a' + NFUNCS || !funcs[s[sidx] - 'a'].name)
			error("Line %d: Unknown function '%c'\n%s\n%*s~~~ What's that?",
					lineno, s[sidx], s, sidx + 1, "^");

		gates[*gidx].func = &funcs[s[sidx] - 'a'];
		sidx++;

		while (isspace(s[sidx]))
			sidx++;
		if (s[sidx] && s[sidx] != '#')
			error("Line %d: Too many arguments given\n%s\n%*s~~~ Here",
					lineno, s, sidx + 1, "^");

		++*gidx;
	}
	else {
		if (strncmp(s + sidx, "state", 5) == 0
				&& (!s[sidx + 5] || isspace(s[sidx + 5])))
		{
			gates[*gidx].type = GATE_STATE;
			sidx += 5;
		}
		else if (strncmp(s + sidx, "prob", 4) == 0
				&& (!s[sidx + 4] || isspace(s[sidx + 4])))
		{
			gates[*gidx].type = GATE_PROBS;
			sidx += 4;
		}
		else error("Line %d: Unknown command\n%s\n%*s~~~ What's that?\n"
					"Available commands are: pause, draw, state, prob",
					lineno, s, sidx + 1, "^");
		while (1)
		{
			int start, stop;
			int start_idx, stop_idx;

			while (isspace((int)s[sidx]))
				sidx++;
			if (!s[sidx] || s[sidx] == '#')
				break;

			if (!isdigit((int)s[sidx]))
				error("Line %d: Expected qubit index\n%s\n%*s~~~ Here",
						lineno, s, sidx + 1, "^");

			parse_range(s, &sidx, lineno, &start, &start_idx, &stop, &stop_idx);
			
			for (int i = start; i < stop; i++)
			{
				if (gates[*gidx].ctrl & ctrlbit(i))
					error("Line %d: Duplicate qubit '%d'\n%s\n%*s~~~ Here",
							lineno, i, s, start_idx + 1, "^");
				gates[*gidx].ctrl |= ctrlbit(i);
			}
		}

		if (!gates[*gidx].ctrl)
			gates[*gidx].ctrl = -1;

		++*gidx;
	}

	while (isspace(s[sidx]))
		sidx++;
	if (s[sidx])
		error("Line %d: Unexpected token\n%s\n%*s~~~ What's that?",
				lineno, s, sidx + 1, "^");
}

void parse_barrier(const char * s, int sidx,
	struct gate * gates, int * gidx, int lineno, struct gate ** barrier_stack)
{
	int repeat = 0;
	int start = sidx;

	while (s[sidx] == '-')
		sidx++;

	if (islower(s[sidx]))
	{
		gates[*gidx].barrier.name = s[sidx];
		sidx++;
	}
	else gates[*gidx].barrier.name = 0;

	if (s[sidx] && !isspace(s[sidx]) && s[sidx] != '#')
			error("Line %d: Barrier name must be 1 letter\n%s\n%*s~~~ Here",
					lineno, s, sidx + 1, "^");

	while (isspace(s[sidx]))
		sidx++;
	
	if (isdigit(s[sidx])) {
		char * endptr;
		long l;

		errno = 0;
		l = strtol(s + sidx, &endptr, 0);
		if (errno || l > INT_MAX)
			error("Line %d: Barrier repeat is too big. "
					"Maximum value is %d\n%s\n%*s~~~ Here",
					lineno, INT_MAX, s, sidx + 1, "^");
		sidx = endptr - s;
		repeat = (int) l;

		if (s[sidx] && !isspace(s[sidx]) && s[sidx] != '#')
			error("Line %d: Barrier repeat must be an integer.\n"
					"%s\n%*s~~~ Here", lineno, s, sidx + 1, "^");

		while (isspace(s[sidx]))
			sidx++;
		if (s[sidx])
			error("Line %d: Unexpected token following barrier repeat.\n"
					"%s\n%*s~~~ Here", lineno, s, sidx + 1, "^");
	}
	else if (s[sidx])
		error("Line %d: Barrier repeat must be an integer.\n"
				"%s\n%*s~~~ Here", lineno, s, sidx + 1, "^");

	// unwind
	for (struct gate * barrier = *barrier_stack; barrier; barrier = barrier->barrier.prev)
	{
		if (barrier->barrier.name == gates[*gidx].barrier.name)
		{
			while ((*barrier_stack)->barrier.name != gates[*gidx].barrier.name)
				*barrier_stack = (*barrier_stack)->barrier.prev;
			break;
		}
	}

	if (*barrier_stack && (*barrier_stack)->barrier.name == gates[*gidx].barrier.name)
	{
		(*barrier_stack)->barrier.end = *gidx;
		(*barrier_stack)->barrier.repeat = repeat? repeat: 1;
		gates[*gidx].barrier.prev = (*barrier_stack)->barrier.prev;
		*barrier_stack = &gates[*gidx];
		
		gates[*gidx].type = GATE_BARRIER_END;
	}
	else if (repeat)
		error("Line %d: Missing start of repeat block.\n"
				"%s\n%*s~~~ Ends here", lineno, s, start + 1, "^");
	else {
		gates[*gidx].barrier.prev = *barrier_stack;
		*barrier_stack = &gates[*gidx];

		gates[*gidx].type = GATE_BARRIER_BEGIN;
	}
	
	++*gidx;
}

static inline void strip_newline(char * s)
{
	size_t len = strlen(s);
	if (len > 0 && s[len - 1] == '\n')
	{
		s[len - 1] = 0;
		if (len > 1 && s[len - 2] == '\r')
			s[len - 2] = 0;
	}
}

// returns number of gates
int parse_circuit(struct gate * gates, FILE * in)
{
	char s[BUFSIZE];
	int gidx = 0;
	int lineno = 0;
	struct gate * barrier_stack = NULL;

	while (fgets(s, BUFSIZE, in))
	{
		int sidx = 0;
		lineno++;
		strip_newline(s);

		while (isspace(s[sidx]))
			sidx++;
		if (!s[sidx] || s[sidx] == '#')
			continue;

		if (islower(s[sidx]))
		{
			if (islower(s[sidx + 1]))
				parse_command(s, sidx, gates, &gidx, lineno);
			else
				parse_func(s, lineno);
		}
		else if (s[sidx] == '-')
			parse_barrier(s, sidx, gates, &gidx, lineno, &barrier_stack);
		else
			parse_gate(s, &sidx, gates, &gidx, lineno);
	}
	return gidx;
}

int path_parse_circuit(struct gate * gates, const char * path)
{
	int ngates;
	FILE * in;

	if (!(in = fopen(path, "r")))
		error("Failed to open %s: %s", path, strerror(errno));
	ngates = parse_circuit(gates, in);
	fclose(in);

	return ngates;
}
