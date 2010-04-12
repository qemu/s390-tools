/*
 * Copyright IBM Corp 2007
 * Author: Hans-Joachim Picht <hans@linux.vnet.ibm.com>
 *
 * The original parser contained in this file was written by
 * Martin Schwidefsky <schwidefsky@de.ibm.com>
 *
 * Linux for System Hotplug Daemon
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#include <unistd.h>
#include "cpuplugd.h"

static enum op_prio op_prio_table[] =
{
	[OP_NEG] = OP_PRIO_ADD,
	[OP_GREATER] = OP_PRIO_CMP,
	[OP_LESSER] = OP_PRIO_CMP,
	[OP_PLUS] = OP_PRIO_ADD,
	[OP_MINUS] = OP_PRIO_ADD,
	[OP_MULT] = OP_PRIO_MULT,
	[OP_DIV] = OP_PRIO_MULT,
	[OP_AND] = OP_PRIO_AND,
	[OP_OR] = OP_PRIO_OR,
};

void free_term(struct term *fn)
{
	if (!fn)
		return;
	switch (fn->op) {
	case OP_SYMBOL_LOADAVG:
	case OP_SYMBOL_RUNABLE:
	case OP_SYMBOL_CPUS:
	case OP_SYMBOL_IDLE:
	case OP_CONST:
		free(fn);
		break;
	case OP_NEG:
	case OP_NOT:
		free_term(fn->left);
		free(fn);
		break;
	case OP_GREATER:
	case OP_LESSER:
	case OP_PLUS:
	case OP_MINUS:
	case OP_MULT:
	case OP_DIV:
	case OP_AND:
		free_term(fn->left);
		free_term(fn->right);
		free(fn);
		break;
	case OP_OR:
		free_term(fn->left);
		free_term(fn->right);
		free(fn);
		break;
	default:
		break;
	}
}

void print_term(struct term *fn)
{
	switch (fn->op) {
	case OP_SYMBOL_LOADAVG:
		printf("loadavg");
		break;
	case OP_SYMBOL_RUNABLE:
		printf("runable_proc");
		break;
	case OP_SYMBOL_CPUS:
		printf("onumcpus");
		break;
	case OP_SYMBOL_IDLE:
		printf("idle");
		break;
	case OP_SYMBOL_SWAPRATE:
		printf("swaprate");
		break;
	case OP_SYMBOL_FREEMEM:
		printf("freemem");
		break;
	case OP_SYMBOL_APCR:
		printf("apcr");
		break;
	case OP_CONST:
		printf("%f", fn->value);
		break;
	case OP_NEG:
		printf("-(");
		print_term(fn->left);
		printf(")");
		break;
	case OP_NOT:
		printf("!(");
		print_term(fn->left);
		printf(")");
		break;
	case OP_PLUS:
	case OP_MINUS:
	case OP_MULT:
	case OP_DIV:
	case OP_AND:
	case OP_OR:
	case OP_GREATER:
	case OP_LESSER:
		printf("(");
		print_term(fn->left);
		switch (fn->op) {
		case OP_AND:
			printf(") & (");
			break;
		case OP_OR:
			printf(") | (");
			break;
		case OP_GREATER:
			printf(") > (");
			break;
		case OP_LESSER:
			printf(") < (");
			break;
		case OP_PLUS:
			printf(") + (");
			break;
		case OP_MINUS:
			printf(") - (");
			break;
		case OP_MULT:
			printf(") * (");
			break;
		case OP_DIV:
			printf(") / (");
			break;
		case OP_CONST:
			printf("%f", fn->value);
			break;
		case OP_SYMBOL_LOADAVG:
		case OP_SYMBOL_RUNABLE:
		case OP_SYMBOL_CPUS:
		case OP_SYMBOL_IDLE:
		case OP_SYMBOL_APCR:
		case OP_SYMBOL_SWAPRATE:
		case OP_SYMBOL_FREEMEM:
		case OP_NEG:
		case OP_NOT:
		case VAR_LOAD:
		case VAR_RUN:
		case VAR_ONLINE:
			break;
		}
		print_term(fn->right);
		printf(")");
		break;
	case VAR_LOAD:
	case VAR_RUN:
	case VAR_ONLINE:
		break;
	}
}

struct term *parse_term(char **p, enum op_prio prio)
{
	static struct {
		char *name;
		enum operation symop;
	} sym_names [] = {
		{ "loadavg", OP_SYMBOL_LOADAVG },
		{ "runable_proc", OP_SYMBOL_RUNABLE },
		{ "onumcpus", OP_SYMBOL_CPUS },
		{ "idle", OP_SYMBOL_IDLE },
		{ "swaprate", OP_SYMBOL_SWAPRATE },
		{ "apcr", OP_SYMBOL_APCR },
		{ "freemem", OP_SYMBOL_FREEMEM },
	};
	struct term *fn, *new;
	enum operation op;
	char *s;
	double value;
	unsigned int i, length;

	s = *p;
	fn = NULL;
	if (*s == '-') {
		s++;
		fn = malloc(sizeof(struct term));
		if (fn == NULL)
			goto out_error;
		if  (isdigit(*s)) {
			value = 0;
			length = 0;
			sscanf(s, "%lf %n", &value, &length);
			fn->op = OP_CONST;
			fn->value = -value;
			s += length;
		} else {
			fn->op = OP_NEG;
			fn->left = parse_term(&s, prio);
			if (fn->left == NULL)
				goto out_error;
		}
	} else if (*s == '!') {
		s++;
		fn = malloc(sizeof(struct term));
		if (fn == NULL)
			goto out_error;
		fn->op = OP_NOT;
		fn->left = parse_term(&s, prio);
		if (fn->left == NULL)
			goto out_error;
	} else if (isdigit(*s)) {
		value = 0;
		length = 0;
		sscanf(s, "%lf %n", &value, &length);
		for (i = 0; i < length; i++)
			s++;
		fn = malloc(sizeof(struct term));
		if (fn == NULL)
			goto out_error;
		fn->op = OP_CONST;
		fn->value = value;
	} else if (*s == '(') {
		s++;
		fn = parse_term(&s, OP_PRIO_NONE);
		if (fn == NULL || *s != ')')
			goto out_error;
		s++;
	} else {
		for (i = 0; i < sizeof(sym_names)/sizeof(sym_names[0]); i++)
			if (strncmp(s, sym_names[i].name,
				    strlen(sym_names[i].name)) == 0)
			    break;
		if (i >= sizeof(sym_names)/sizeof(sym_names[0]))
			/* Term doesn't make sense. */
			goto out_error;
		fn = malloc(sizeof(struct term));
		if (fn == NULL)
			goto out_error;
		fn->op = sym_names[i].symop;
		s += strlen(sym_names[i].name);
	}
	while (1) {
		switch (*s) {
		case '>':
			op = OP_GREATER;
			break;
		case '<':
			op = OP_LESSER;
			break;
		case '+':
			op = OP_PLUS;
			break;
		case '-':
			op = OP_MINUS;
			break;
		case '*':
			op = OP_MULT;
			break;
		case '/':
			op = OP_DIV;
			break;
		case '|':
			op = OP_OR;
			break;
		case '&':
			op = OP_AND;
			break;
		default:
			goto out;
		}
		if (prio >= op_prio_table[op])
			break;
		s++;
		new = malloc(sizeof(struct term));
		new->op = op;
		new->left = fn;
		if (new == NULL)
			goto out_error;
		new->right = parse_term(&s, op_prio_table[op]);
		if (new->right == NULL) {
			free(new);
			goto out_error;
		}
		fn = new;
	}
out:
	*p = s;
	return fn;
out_error:
	if (fn)
		free_term(fn);
	return NULL;
}

static double eval_double(struct term *fn, struct symbols *symbols)
{

		double a;
		double b;
		double sum;
	switch (fn->op) {
	case OP_SYMBOL_LOADAVG:
		return symbols->loadavg;
	case OP_SYMBOL_RUNABLE:
		return symbols->runable_proc;
	case OP_SYMBOL_CPUS:
		return symbols->onumcpus;
	case OP_SYMBOL_IDLE:
		return symbols->idle;
	case OP_SYMBOL_FREEMEM:
		return symbols->freemem;
	case OP_SYMBOL_APCR:
		return symbols->apcr;
	case OP_SYMBOL_SWAPRATE:
		return symbols->swaprate;
	case OP_CONST:
		return fn->value;
	case OP_NEG:
		return -eval_double(fn->left, symbols);
	case OP_PLUS:
		return eval_double(fn->left, symbols) +
			eval_double(fn->right, symbols);
	case OP_MINUS:
		return eval_double(fn->left, symbols) -
			eval_double(fn->right, symbols);
	case OP_MULT:
		a = eval_double(fn->left, symbols);
		b = eval_double(fn->right, symbols);
		sum = a*b;
		/*return eval_double(fn->left, symbols) *
			eval_double(fn->right, symbols);*/
	case OP_DIV:
		a = eval_double(fn->left, symbols);
		b = eval_double(fn->right, symbols);
		sum = a/b;
		return sum;
		/*return eval_double(fn->left, symbols) /
			eval_double(fn->right, symbols); */
	case OP_NOT:
	case OP_AND:
	case OP_OR:
	case OP_GREATER:
	case OP_LESSER:
	case VAR_LOAD:
	case VAR_RUN:
	case VAR_ONLINE:
		fprintf(stderr, "Invalid term specified: %i\n", fn->op);
		clean_up();
	}
	return 0;
}

int eval_term(struct term *fn, struct symbols *symbols)
{
	if (fn == NULL || symbols == NULL)
		return 0.0;
	switch (fn->op) {
	case OP_NOT:
		return !eval_term(fn->left, symbols);
	case OP_OR:
		return eval_term(fn->left, symbols) == 1 ||
			eval_term(fn->right, symbols) == 1;
	case OP_AND:
		return eval_term(fn->left, symbols) == 1 &&
			eval_term(fn->right, symbols) == 1;
	case OP_GREATER:
		return eval_double(fn->left, symbols) >
			eval_double(fn->right, symbols);
	case OP_LESSER:
		return eval_double(fn->left, symbols) <
			eval_double(fn->right, symbols);
	default:
		return eval_double(fn, symbols) != 0.0;
	}
}
