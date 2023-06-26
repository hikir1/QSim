#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include "main.h"

enum optype {
	OP_VAR,
	OP_CONST,
	OP_POW,
	OP_NEG,
	OP_BNOT,
	OP_NOT,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_ADD,
	OP_SUB,
	OP_LT,
	OP_LTE,
	OP_GT,
	OP_GTE,
	OP_EQ,
	OP_NEQ,
	OP_BAND,
	OP_BXOR,
	OP_BOR,
	OP_AND,
	OP_OR,
	OP_TERN
};

struct op {
	enum optype type;
	int args[3];
};

enum isempty {
	EMPTY,
	NOTEMPTY
};

struct func funcs[NFUNCS];

static void parse_tern(const char *, int *, struct op *, int *, struct func *, int);

static enum isempty parse_term(const char * s, int * sidx, struct op * ops,
		int * opidx, bool error_if_empty, struct func * func, int lineno)
{
	while (isspace((int)s[*sidx]))
		++*sidx;

	if (islower((int)s[*sidx]))
	{
		int arg = s[*sidx] - 'a';
		
		if (arg >= NQBITS - 1)
			error("Line %d: Invalid variable in function %c\n%s\n%*s~~~ Here\n"
					"Valid variables are 'a'-'%c'", lineno, func->name, s, *sidx + 1, "^", 'a' + NQBITS - 2);
		if (*opidx >= FMAXOPS)
			error("Line %d: Function %c is too big :(", lineno, func->name);

		ops[*opidx].type = OP_VAR;
		ops[*opidx].args[0] = arg;
		++*opidx;
		++*sidx;

		if (arg + 1 > func->argc)
			func->argc = arg + 1;
	}
	else if (isdigit((int)s[*sidx]))
	{
		char * endptr;
		long num;

		errno = 0;
		num = strtol(s + *sidx, &endptr, 0);
		if (errno || num < INT_MIN || num > INT_MAX)
			error("Line %d: Invalid integer in function %c\n%s\n%*s~~~ Here\n"
					"Must be in range [%d, %d]", lineno, func->name, s, *sidx + 1, "^", INT_MIN, INT_MAX);

		if (*opidx >= FMAXOPS)
			error("Line %d: Function %c is too big :(", lineno, func->name);

		ops[*opidx].type = OP_CONST;
		ops[*opidx].args[0] = (int)num;
		++*opidx;
		*sidx = endptr - s;
	}
	else if (s[*sidx] == '(')
	{
		++*sidx;
		parse_tern(s, sidx, ops, opidx, func, lineno);
		if (s[*sidx] != ')')
			error("Line: %d: Missing closing parenthesis in "
					"function %c\n%s\n%*s~~~ Here", lineno, func->name, s, *sidx + 1, "^");
		++*sidx;
	}
	else if (error_if_empty)
		error("Line %d: Expected term in function %c\n%s\n%*s~~~ Here",
				lineno, func->name, s, *sidx + 1, "^");
	else
		return EMPTY;
	return NOTEMPTY;
}

static enum isempty parse_pow(const char * s, int * sidx, struct op * ops,
		int * opidx, bool error_if_empty, struct func * func, int lineno)
{
	if (parse_term(s, sidx, ops, opidx, error_if_empty, func, lineno) == EMPTY)
		return EMPTY;

	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (s[*sidx] == '*' && s[*sidx + 1] == '*')
		{
			int arg1 = *opidx - 1;

			*sidx += 2;
			parse_term(s, sidx, ops, opidx, true, func, lineno);

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			ops[*opidx].type = OP_POW;
			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			++*opidx;
		}
		else break;
	}
	return NOTEMPTY;
}

static enum isempty parse_not(const char * s, int * sidx, struct op * ops,
		int * opidx, bool error_if_empty, struct func * func, int lineno)
{
	while (isspace((int)s[*sidx]))
		++*sidx;

	if (s[*sidx] == '-' || s[*sidx] == '~' || s[*sidx] == '!')
	{
		int symb = s[*sidx];
		++*sidx;
		parse_not(s, sidx, ops, opidx, true, func, lineno);

		if (*opidx >= FMAXOPS)
			error("Line %d: Function %c is too big :(", lineno, func->name);

		if (symb == '-')
			ops[*opidx].type = OP_NEG;
		else if (symb == '~')
			ops[*opidx].type = OP_BNOT;
		else ops[*opidx].type = OP_NOT;

		ops[*opidx].args[0] = *opidx - 1;
		++*opidx;
		return NOTEMPTY;
	}

	return parse_pow(s, sidx, ops, opidx, error_if_empty, func, lineno);
}

static void parse_mult(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	int arg1;

	parse_not(s, sidx, ops, opidx, true, func, lineno);
	arg1 = *opidx - 1;

	while (1)
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (parse_not(s, sidx, ops, opidx, false, func, lineno) != EMPTY)
		{
			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			ops[*opidx].type = OP_MUL;
			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			arg1 = *opidx;
			++*opidx;
			continue;
		}
		if (s[*sidx] == '*' && s[*sidx + 1] != '*' || s[*sidx] == '/' || s[*sidx] == '%')
		{
			int symb = s[*sidx];

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			++*sidx;
			parse_not(s, sidx, ops, opidx, true, func, lineno);
			
			if (symb == '*')
				ops[*opidx].type = OP_MUL;
			else if (symb == '/')
				ops[*opidx].type = OP_DIV;
			else ops[*opidx].type = OP_MOD;

			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			arg1 = *opidx;
			++*opidx;
			continue;
		}
		break;
	}
}

static void parse_add(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	parse_mult(s, sidx, ops, opidx, func, lineno);
	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (s[*sidx] == '+' || s[*sidx] == '-')
		{
			int arg1 = *opidx - 1;
			int symb = s[*sidx];

			++*sidx;
			parse_mult(s, sidx, ops, opidx, func, lineno);

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			if (symb == '+')
				ops[*opidx].type = OP_ADD;
			else ops[*opidx].type = OP_SUB;

			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			++*opidx;
		}
		else break;
	}
}

static void parse_rel(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	parse_add(s, sidx, ops, opidx, func, lineno);
	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (s[*sidx] == '<' || s[*sidx] == '>')
		{
			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			if (s[*sidx] == '<')
			{
				if (s[*sidx + 1] == '=')
				{
					ops[*opidx].type = OP_LTE;
					*sidx += 2;
				}
				else {
					ops[*opidx].type = OP_LT;
					++*sidx;
				}
			}
			else
			{
				if (s[*sidx + 1] == '=')
				{
					ops[*opidx].type = OP_GTE;
					*sidx += 2;
				}
				else {
					ops[*opidx].type = OP_GT;
					++*sidx;
				}
			}
			ops[*opidx].args[0] = *opidx - 1;
			
			parse_add(s, sidx, ops, opidx, func, lineno);
			ops[*opidx].args[1] = *opidx - 1;

			++*opidx;
		}
		else break;
	}
}

static void parse_eq(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	parse_rel(s, sidx, ops, opidx, func, lineno);
	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if ((s[*sidx] == '=' || s[*sidx] == '!') && s[*sidx + 1] == '=')
		{
			int arg1 = *opidx - 1;
			int sym = s[*sidx];

			++*sidx;
			parse_rel(s, sidx, ops, opidx, func, lineno);

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			ops[*opidx].type = sym == '='? OP_EQ: OP_NEQ;
			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			++*opidx;
		}
		else break;
	}
}

static void parse_band(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	parse_eq(s, sidx, ops, opidx, func, lineno);
	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (s[*sidx] == '&')
		{
			int arg1 = *opidx - 1;

			++*sidx;
			parse_eq(s, sidx, ops, opidx, func, lineno);

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			ops[*opidx].type = OP_BAND;
			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			++*opidx;
		}
		else break;
	}
}

static void parse_bxor(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	parse_band(s, sidx, ops, opidx, func, lineno);
	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (s[*sidx] == '^')
		{
			int arg1 = *opidx - 1;

			++*sidx;
			parse_band(s, sidx, ops, opidx, func, lineno);

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			ops[*opidx].type = OP_BXOR;
			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			++*opidx;
		}
		else break;
	}
}

static void parse_bor(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	parse_bxor(s, sidx, ops, opidx, func, lineno);
	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (s[*sidx] == '|')
		{
			int arg1 = *opidx - 1;

			++*sidx;
			parse_bxor(s, sidx, ops, opidx, func, lineno);

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			ops[*opidx].type = OP_BOR;
			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			++*opidx;
		}
		else break;
	}
}

static void parse_and(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	parse_bor(s, sidx, ops, opidx, func, lineno);
	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (s[*sidx] == '&' && s[*sidx + 1] == '&')
		{
			int arg1 = *opidx - 1;

			*sidx += 2;
			parse_bor(s, sidx, ops, opidx, func, lineno);

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			ops[*opidx].type = OP_AND;
			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			++*opidx;
		}
		else break;
	}
}

static void parse_or(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	parse_and(s, sidx, ops, opidx, func, lineno);
	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (s[*sidx] == '|' && s[*sidx + 1] == '|')
		{
			int arg1 = *opidx - 1;

			*sidx += 2;
			parse_and(s, sidx, ops, opidx, func, lineno);

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			ops[*opidx].type = OP_OR;
			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = *opidx - 1;
			++*opidx;
		}
		else break;
	}
}

static void parse_tern(const char * s, int * sidx, struct op * ops, int * opidx, struct func * func, int lineno)
{
	parse_or(s, sidx, ops, opidx, func, lineno);
	while (s[*sidx] && s[*sidx] != ')')
	{
		if (isspace((int)s[*sidx]))
		{
			++*sidx;
			continue;
		}
		if (s[*sidx] == '?')
		{
			int arg1 = *opidx - 1;
			int arg2;

			++*sidx;
			parse_or(s, sidx, ops, opidx, func, lineno);
			arg2 = *opidx - 1;

			while (isspace((int)s[*sidx]))
				++*sidx;

			if (s[*sidx] != ':')
				error("Line %d: Ternary operator missing ':'"
						"in function %c\n%s\n%*s~~~ Here\n", lineno, func->name, s, *sidx + 1, "^");

			++*sidx;
			parse_or(s, sidx, ops, opidx, func, lineno);

			if (*opidx >= FMAXOPS)
				error("Line %d: Function %c is too big :(", lineno, func->name);

			ops[*opidx].type = OP_TERN;
			ops[*opidx].args[0] = arg1;
			ops[*opidx].args[1] = arg2;
			ops[*opidx].args[2] = *opidx - 1;
			++*opidx;
		}
		else break;
	}
}

// only works for b >= 0
static int intpow(int a, int b)
{
	int c = 1;
	while (b > 0)
	{
		if (b & 1)
			c *= a;
		a *= a;
		b >>= 1;
	}
	return c;
}

static int run(const struct op * ops, int nops, int args, int argc, int lineno, int name)
{
	int vals[FMAXOPS];
	for (int i = 0; i < nops; i++)
	{
		switch (ops[i].type)
		{
			case OP_VAR:
				vals[i] = !!(args & (1 << argc - 1 >> ops[i].args[0]));
				continue;
			case OP_CONST:
				vals[i] = ops[i].args[0];
				continue;
			case OP_POW:
				vals[i] = intpow(vals[ops[i].args[0]], vals[ops[i].args[1]]);
				continue;
			case OP_NEG:
				vals[i] = -vals[ops[i].args[0]];
				continue;
			case OP_NOT:
				vals[i] = !vals[ops[i].args[0]];
				continue;
			case OP_BNOT:
				vals[i] = ~vals[ops[i].args[0]];
				continue;
			case OP_MUL:
				vals[i] = vals[ops[i].args[0]] * vals[ops[i].args[1]];
				continue;
			case OP_DIV:
				if (vals[ops[i].args[1]] == 0)
					error("Line %d: Division by zero in function %c", lineno, name);
				vals[i] = vals[ops[i].args[0]] / vals[ops[i].args[1]];
				continue;
			case OP_MOD:
				if (vals[ops[i].args[1]] == 0)
					error("Line %d: Division by zero in function %c", lineno, name);
				vals[i] = vals[ops[i].args[0]] % vals[ops[i].args[1]];
				continue;
			case OP_ADD:
				vals[i] = vals[ops[i].args[0]] + vals[ops[i].args[1]];
				continue;
			case OP_SUB:
				vals[i] = vals[ops[i].args[0]] - vals[ops[i].args[1]];
				continue;
			case OP_LT:
				vals[i] = vals[ops[i].args[0]] < vals[ops[i].args[1]];
				continue;
			case OP_LTE:
				vals[i] = vals[ops[i].args[0]] <= vals[ops[i].args[1]];
				continue;
			case OP_GT:
				vals[i] = vals[ops[i].args[0]] > vals[ops[i].args[1]];
				continue;
			case OP_GTE:
				vals[i] = vals[ops[i].args[0]] >= vals[ops[i].args[1]];
				continue;
			case OP_EQ:
				vals[i] = vals[ops[i].args[0]] == vals[ops[i].args[1]];
				continue;
			case OP_NEQ:
				vals[i] = vals[ops[i].args[0]] != vals[ops[i].args[1]];
				continue;
			case OP_BAND:
				vals[i] = vals[ops[i].args[0]] & vals[ops[i].args[1]];
				continue;
			case OP_BXOR:
				vals[i] = vals[ops[i].args[0]] ^ vals[ops[i].args[1]];
				continue;
			case OP_BOR:
				vals[i] = vals[ops[i].args[0]] | vals[ops[i].args[1]];
				continue;
			case OP_AND:
				vals[i] = vals[ops[i].args[0]] && vals[ops[i].args[1]];
				continue;
			case OP_OR:
				vals[i] = vals[ops[i].args[0]] || vals[ops[i].args[1]];
				continue;
			case OP_TERN:
				vals[i] = vals[ops[i].args[0]]? vals[ops[i].args[1]]: vals[ops[i].args[2]];
				continue;
			default:
				error("Unknown operator type in "
						"representation of func: %d", ops[i].type);
		}
	}
	return vals[nops - 1];
}

struct func * parse_name(const char * s, int * sidx, int lineno)
{
	char name = s[*sidx];

	if (name < 'a' || name >= 'a' + NFUNCS)
		error("Line %d: Bad function name '%c'. Choose from "
				"'a'-'%c'\n%s\n%*s~~~ Here", lineno, name, 'a' + NFUNCS, s, *sidx + 1, "^");
	if (funcs[name - 'a'].name)
		error("Line %d: Function '%c' already exists\n"
				"%s\n%*s~~~ Here", lineno, name, s, *sidx + 1, "^");
	++*sidx;
	while (isspace((int)s[*sidx]))
		++*sidx;

	if (s[*sidx] != '=')
		error("Line %d: Expected '=' in function declaration "
				"following %c\n%s\n%*s~~~ Here", lineno, name, s, *sidx + 1, "^");
	++*sidx;

	funcs[name - 'a'].name = name;
	return &funcs[name - 'a'];
}

void print_func(const struct func * func)
{
	printf("Function: %c\n", func->name);
	for (int i = 0; i < (1 << func->argc); i++)
	{
		for (int j = (1 << func->argc - 1); j > 0; j >>= 1)
			putc(0x30 | !!(i & j), stdout);
		printf(" -> %d\n", func->map[i]);
	}
}

void parse_func(const char * s, int lineno)
{
	struct op ops[FMAXOPS];

	int sidx = 0, opidx = 0;
	struct func * func = parse_name(s, &sidx, lineno);

	parse_tern(s, &sidx, ops, &opidx, func, lineno);

	if (s[sidx] == ')')
		error("Line %d: Stray closing parenthesis in function "
				"%c\n%s\n%*s~~~ Here", lineno, func->name, s, sidx + 1, "^");
	if (s[sidx])
		error("Line %d: Unknown symbol in function "
				"%c\n%s\n%*s~~~ Here", lineno, func->name, s, sidx + 1, "^");

	for (int i = 0; i < (1 << func->argc); i++)
		func->map[i] = run(ops, opidx, i, func->argc, lineno, func->name);
}
