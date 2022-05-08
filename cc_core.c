/* Copyright (C) 2016 Jeremiah Orians
 * Copyright (C) 2018 Jan (janneke) Nieuwenhuizen <janneke@gnu.org>
 * This file is part of stage0.
 *
 * stage0 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * stage0 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with stage0.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc_globals.h"
#include "gcc_req.h"
#include <stdint.h>

/* Global lists */
struct token_list* global_symbol_list;
struct token_list* global_function_list;
struct token_list* global_constant_list;

/* Core lists for this file */
struct token_list* function;
struct token_list* out;

/* What we are currently working on */
struct type* current_target;
char* break_target_head;
char* break_target_func;
char* break_target_num;
struct token_list* break_frame;
int current_count;
struct type* last_type;
int Address_of;

/* Imported functions */
char* parse_string(char* string);
int escape_lookup(char* c);
char* int2str(int x, int base, int signed_p);


struct token_list* emit(char *s, struct token_list* head)
{
	struct token_list* t = calloc(1, sizeof(struct token_list));
	t->next = head;
	t->s = s;
	return t;
}

void emit_out(char* s)
{
	out = emit(s, out);
}

struct token_list* uniqueID(char* s, struct token_list* l, char* num)
{
	l = emit(s, l);
	l = emit("_", l);
	l = emit(num, l);
	l = emit("\n", l);
	return l;
}

void uniqueID_out(char* s, char* num)
{
	out = uniqueID(s, out, num);
}

struct token_list* sym_declare(char *s, struct type* t, struct token_list* list)
{
	struct token_list* a = calloc(1, sizeof(struct token_list));
	a->next = list;
	a->s = s;
	a->type = t;
	return a;
}

struct token_list* sym_lookup(char *s, struct token_list* symbol_list)
{
	struct token_list* i;
	for(i = symbol_list; NULL != i; i = i->next)
	{
		if(match(i->s, s)) return i;
	}
	return NULL;
}

void line_error()
{
	fputs(global_token->filename, stderr);
	fputs(":", stderr);
	fputs(int2str(global_token->linenumber, 10, TRUE), stderr);
	fputs(":", stderr);
}

void require_match(char* message, char* required)
{
	if(!match(global_token->s, required))
	{
		line_error();
		fputs(message, stderr);
		exit(EXIT_FAILURE);
	}
	global_token = global_token->next;
}

void expression();
void function_call(char* s, int bool)
{
	require_match("ERROR in process_expression_list\nNo ( was found\n", "(");
	int passed = 0;
	emit_out("PUSH_edi\t# Prevent overwriting in recursion\n");
	emit_out("PUSH_ebp\t# Protect the old base pointer\n");
	emit_out("COPY_esp_to_edi\t# Copy new base pointer\n");

	if(global_token->s[0] != ')')
	{
		expression();
		emit_out("PUSH_eax\t#_process_expression1\n");
		passed = 1;

		while(global_token->s[0] == ',')
		{
			global_token = global_token->next;
			expression();
			emit_out("PUSH_eax\t#_process_expression2\n");
			passed = passed + 1;
		}
	}

	require_match("ERROR in process_expression_list\nNo ) was found\n", ")");

	if(TRUE == bool)
	{
		emit_out("LOAD_BASE_ADDRESS_eax %");
		emit_out(s);
		emit_out("\nLOAD_INTEGER\n");
		emit_out("COPY_edi_to_ebp\n");
		emit_out("CALL_eax\n");
	}
	else
	{
		emit_out("COPY_edi_to_ebp\n");
		emit_out("CALL_IMMEDIATE %FUNCTION_");
		emit_out(s);
		emit_out("\n");
	}

	for(; passed > 0; passed = passed - 1)
	{
		emit_out("POP_ebx\t# _process_expression_locals\n");
	}
	emit_out("POP_ebp\t# Restore old base pointer\n");
	emit_out("POP_edi\t# Prevent overwrite\n");
}

void constant_load(struct token_list* a)
{
	emit_out("LOAD_IMMEDIATE_eax %");
	emit_out(a->arguments->s);
	emit_out("\n");
}

void variable_load(struct token_list* a)
{
	if(match("FUNCTION", a->type->name) && match("(", global_token->s))
	{
		function_call(int2str(a->depth, 10, TRUE), TRUE);
		return;
	}
	current_target = a->type;
	emit_out("LOAD_BASE_ADDRESS_eax %");
	emit_out(int2str(a->depth, 10, TRUE));
	emit_out("\n");

	if(TRUE == Address_of) return;
	if(match("=", global_token->s)) return;

	emit_out("LOAD_INTEGER\n");
}

void function_load(struct token_list* a)
{
	if(match("(", global_token->s))
	{
		function_call(a->s, FALSE);
		return;
	}

	emit_out("LOAD_IMMEDIATE_eax &FUNCTION_");
	emit_out(a->s);
	emit_out("\n");
}

void global_load(struct token_list* a)
{
	current_target = a->type;
	emit_out("LOAD_IMMEDIATE_eax &GLOBAL_");
	emit_out(a->s);
	emit_out("\n");
	if(!match("=", global_token->s)) emit_out("LOAD_INTEGER\n");
}

/*
 * primary-expr:
 * FAILURE
 * "String"
 * 'Char'
 * [0-9]*
 * [a-z,A-Z]*
 * ( expression )
 */

void primary_expr_failure()
{
	line_error();
	fputs("Recieved ", stderr);
	fputs(global_token->s, stderr);
	fputs(" in primary_expr\n", stderr);
	exit(EXIT_FAILURE);
}

void primary_expr_string()
{
	char* number_string = int2str(current_count, 10, TRUE);
	current_count = current_count + 1;
	emit_out("LOAD_IMMEDIATE_eax &STRING_");
	uniqueID_out(function->s, number_string);

	/* The target */
	strings_list = emit(":STRING_", strings_list);
	strings_list = uniqueID(function->s, strings_list, number_string);

	/* Parse the string */
	strings_list = emit(parse_string(global_token->s), strings_list);
	global_token = global_token->next;
}

void primary_expr_char()
{
	emit_out("LOAD_IMMEDIATE_eax %");
	emit_out(int2str(escape_lookup(global_token->s + 1), 10, TRUE));
	emit_out("\n");
	global_token = global_token->next;
}

void primary_expr_number()
{
	emit_out("LOAD_IMMEDIATE_eax %");
	emit_out(global_token->s);
	emit_out("\n");
	global_token = global_token->next;
}

void primary_expr_variable()
{
	char* s = global_token->s;
	global_token = global_token->next;
	struct token_list* a = sym_lookup(s, global_constant_list);
	if(NULL != a)
	{
		constant_load(a);
		return;
	}

	a= sym_lookup(s, function->locals);
	if(NULL != a)
	{
		variable_load(a);
		return;
	}

	a = sym_lookup(s, function->arguments);
	if(NULL != a)
	{
		variable_load(a);
		return;
	}

	a= sym_lookup(s, global_function_list);
	if(NULL != a)
	{
		function_load(a);
		return;
	}

	a = sym_lookup(s, global_symbol_list);
	if(NULL != a)
	{
		global_load(a);
		return;
	}

	line_error();
	fputs(s ,stderr);
	fputs(" is not a defined symbol\n", stderr);
	exit(EXIT_FAILURE);
}

void primary_expr();
struct type* promote_type(struct type* a, struct type* b)
{
	if(NULL == b)
	{
		return a;
	}
	if(NULL == a)
	{
		return b;
	}

	struct type* i;
	for(i = global_types; NULL != i; i = i->next)
	{
		if(a->name == i->name) break;
		if(b->name == i->name) break;
		if(a->name == i->indirect->name) break;
		if(b->name == i->indirect->name) break;
	}
	return i;
}

void common_recursion(FUNCTION f)
{
	last_type = current_target;
	global_token = global_token->next;
	emit_out("PUSH_eax\t#_common_recursion\n");
	f();
	current_target = promote_type(current_target, last_type);
	emit_out("POP_ebx\t# _common_recursion\n");
}

void general_recursion( FUNCTION f, char* s, char* name, FUNCTION iterate)
{
	if(match(name, global_token->s))
	{
		common_recursion(f);
		emit_out(s);
		iterate();
	}
}

int ceil_log2(int a)
{
	int result = 0;
	if((a & (a - 1)) == 0)
	{
		result = -1;
	}

	while(a > 0)
	{
		result = result + 1;
		a = a >> 1;
	}

	return result;
}

/*
 * postfix-expr:
 *         primary-expr
 *         postfix-expr [ expression ]
 *         postfix-expr ( expression-list-opt )
 *         postfix-expr -> member
 */
struct type* lookup_member(struct type* parent, char* name);
void postfix_expr_arrow()
{
	emit_out("# looking up offset\n");
	global_token = global_token->next;

	struct type* i = lookup_member(current_target, global_token->s);
	current_target = i->type;
	global_token = global_token->next;

	if(0 != i->offset)
	{
		emit_out("# -> offset calculation\n");
		emit_out("LOAD_IMMEDIATE_ebx %");
		emit_out(int2str(i->offset, 10, TRUE));
		emit_out("\nADD_ebx_to_eax\n");
	}

	if(!match("=", global_token->s) && (4 == i->size))
	{
		emit_out("LOAD_INTEGER\n");
	}
}

void postfix_expr_array()
{
	struct type* array = current_target;
	common_recursion(expression);
	current_target = array;
	char* assign = "LOAD_INTEGER\n";

	/* Add support for Ints */
	if(match("char*",  current_target->name))
	{
		assign = "LOAD_BYTE\n";
	}
	else
	{
		emit_out("SAL_eax_Immediate8 !");
		emit_out(int2str(ceil_log2(current_target->indirect->size), 10, TRUE));
		emit_out("\n");
	}

	emit_out("ADD_ebx_to_eax\n");
	require_match("ERROR in postfix_expr\nMissing ]\n", "]");

	if(match("=", global_token->s))
	{
		assign = "";
	}

	emit_out(assign);
}

/*
 * unary-expr:
 *         postfix-expr
 *         - postfix-expr
 *         !postfix-expr
 *         sizeof ( type )
 */
struct type* type_name();
void unary_expr_sizeof()
{
	global_token = global_token->next;
	require_match("ERROR in unary_expr\nMissing (\n", "(");
	struct type* a = type_name();
	require_match("ERROR in unary_expr\nMissing )\n", ")");

	emit_out("LOAD_IMMEDIATE_eax %");
	emit_out(int2str(a->size, 10, TRUE));
	emit_out("\n");
}

void postfix_expr_stub()
{
	if(match("[", global_token->s))
	{
		postfix_expr_array();
		postfix_expr_stub();
	}

	if(match("->", global_token->s))
	{
		postfix_expr_arrow();
		postfix_expr_stub();
	}
}

void postfix_expr()
{
	primary_expr();
	postfix_expr_stub();
}

/*
 * expression:
 *         bitwise-or-expr
 *         bitwise-or-expr = expression
 */

void primary_expr()
{
	if(match("&", global_token->s))
	{
		Address_of = TRUE;
		global_token = global_token->next;
	}
	else
	{
		Address_of = FALSE;
	}

	if(match("sizeof", global_token->s)) unary_expr_sizeof();
	else if(global_token->s[0] == '(')
	{
		global_token = global_token->next;
		expression();
		require_match("Error in Primary expression\nDidn't get )\n", ")");
	}
	else if(global_token->s[0] == '\'') primary_expr_char();
	else if(global_token->s[0] == '"') primary_expr_string();
	else if(in_set(global_token->s[0], "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_")) primary_expr_variable();
	else if(in_set(global_token->s[0], "0123456789")) primary_expr_number();
	else primary_expr_failure();
}

void expression()
{
	postfix_expr();
	if(match("=", global_token->s))
	{
		char* store;
		if(!match("]", global_token->prev->s) || !match("char*", current_target->name))
		{
			store = "STORE_INTEGER\n";
		}
		else
		{
			store = "STORE_CHAR\n";
		}

		common_recursion(expression);
		emit_out(store);
		current_target = NULL;
	}
}


/* Process local variable */
void collect_local()
{
	struct type* type_size = type_name();
	struct token_list* a = sym_declare(global_token->s, type_size, function->locals);
	if(match("main", function->s) && (NULL == function->locals))
	{
		a->depth = -20;
	}
	else if((NULL == function->arguments) && (NULL == function->locals))
	{
		a->depth = -8;
	}
	else if(NULL == function->locals)
	{
		a->depth = function->arguments->depth - 8;
	}
	else
	{
		a->depth = function->locals->depth - 4;
	}

	function->locals = a;

	emit_out("# Defining local ");
	emit_out(global_token->s);
	emit_out("\n");

	global_token = global_token->next;

	if(match("=", global_token->s))
	{
		global_token = global_token->next;
		expression();
	}

	require_match("ERROR in collect_local\nMissing ;\n", ";");

	emit_out("PUSH_eax\t#");
	emit_out(a->s);
	emit_out("\n");
}

void statement();

/* Evaluate if statements */
void process_if()
{
	char* number_string = int2str(current_count, 10, TRUE);
	current_count = current_count + 1;

	emit_out("# IF_");
	uniqueID_out(function->s, number_string);

	global_token = global_token->next;
	require_match("ERROR in process_if\nMISSING (\n", "(");
	expression();

	emit_out("TEST\nJUMP_EQ %ELSE_");
	uniqueID_out(function->s, number_string);

	require_match("ERROR in process_if\nMISSING )\n", ")");
	statement();

	emit_out("JUMP %_END_IF_");
	uniqueID_out(function->s, number_string);
	emit_out(":ELSE_");
	uniqueID_out(function->s, number_string);

	if(match("else", global_token->s))
	{
		global_token = global_token->next;
		statement();
	}
	emit_out(":_END_IF_");
	uniqueID_out(function->s, number_string);
}

/* Process Assembly statements */
void process_asm()
{
	global_token = global_token->next;
	require_match("ERROR in process_asm\nMISSING (\n", "(");
	while(34 == global_token->s[0])
	{/* 34 == " */
		emit_out((global_token->s + 1));
		emit_out("\n");
		global_token = global_token->next;
	}
	require_match("ERROR in process_asm\nMISSING )\n", ")");
	require_match("ERROR in process_asm\nMISSING ;\n", ";");
}

/* Process while loops */
void process_while()
{
	struct token_list* nested_locals = break_frame;
	char* nested_break_head = break_target_head;
	char* nested_break_func = break_target_func;
	char* nested_break_num = break_target_num;

	char* number_string = int2str(current_count, 10, TRUE);
	current_count = current_count + 1;

	break_target_head = "END_WHILE_";
	break_target_num = number_string;
	break_frame = function->locals;
	break_target_func = function->s;

	emit_out(":WHILE_");
	uniqueID_out(function->s, number_string);

	global_token = global_token->next;
	require_match("ERROR in process_while\nMISSING (\n", "(");
	expression();

	emit_out("TEST\nJUMP_EQ %END_WHILE_");
	uniqueID_out(function->s, number_string);
	emit_out("# THEN_while_");
	uniqueID_out(function->s, number_string);

	require_match("ERROR in process_while\nMISSING )\n", ")");
	statement();

	emit_out("JUMP %WHILE_");
	uniqueID_out(function->s, number_string);
	emit_out(":END_WHILE_");
	uniqueID_out(function->s, number_string);

	break_target_head = nested_break_head;
	break_target_func = nested_break_func;
	break_target_num = nested_break_num;
	break_frame = nested_locals;
}

/* Ensure that functions return */
void return_result()
{
	global_token = global_token->next;
	if(global_token->s[0] != ';') expression();

	require_match("ERROR in return_result\nMISSING ;\n", ";");

	struct token_list* i;
	for(i = function->locals; NULL != i; i = i->next)
	{
		emit_out("POP_ebx\t# _return_result_locals\n");
	}
	emit_out("RETURN\n");
}

void recursive_statement()
{
	global_token = global_token->next;
	struct token_list* frame = function->locals;

	while(!match("}", global_token->s))
	{
		statement();
	}
	global_token = global_token->next;

	/* Clean up any locals added */
	if(!match("RETURN\n", out->s))
	{
		struct token_list* i;
		for(i = function->locals; frame != i; i = i->next)
		{
			emit_out( "POP_ebx\t# _recursive_statement_locals\n");
		}
	}
	function->locals = frame;
}

/*
 * statement:
 *     { statement-list-opt }
 *     type-name identifier ;
 *     type-name identifier = expression;
 *     if ( expression ) statement
 *     if ( expression ) statement else statement
 *     do statement while ( expression ) ;
 *     while ( expression ) statement
 *     for ( expression ; expression ; expression ) statement
 *     asm ( "assembly" ... "assembly" ) ;
 *     goto label ;
 *     label:
 *     return ;
 *     break ;
 *     expr ;
 */

struct type* lookup_type(char* s, struct type* start);
void statement()
{
	if(global_token->s[0] == '{')
	{
		recursive_statement();
	}
	else if(':' == global_token->s[0])
	{
		emit_out(global_token->s);
		emit_out("\t#C goto label\n");
		global_token = global_token->next;
	}
	else if((NULL != lookup_type(global_token->s, prim_types)) ||
	          match("struct", global_token->s))
	{
		collect_local();
	}
	else if(match("if", global_token->s))
	{
		process_if();
	}
	else if(match("while", global_token->s))
	{
		process_while();
	}
	else if(match("asm", global_token->s))
	{
		process_asm();
	}
	else if(match("goto", global_token->s))
	{
		global_token = global_token->next;
		emit_out("JUMP %");
		emit_out(global_token->s);
		emit_out("\n");
		global_token = global_token->next;
		require_match("ERROR in statement\nMissing ;\n", ";");
	}
	else if(match("return", global_token->s))
	{
		return_result();
	}
	else
	{
		expression();
		require_match("ERROR in statement\nMISSING ;\n", ";");
	}
}

/* Collect function arguments */
void collect_arguments()
{
	global_token = global_token->next;

	while(!match(")", global_token->s))
	{
		struct type* type_size = type_name();
		if(global_token->s[0] == ')')
		{
			/* foo(int,char,void) doesn't need anything done */
			continue;
		}
		else if(global_token->s[0] != ',')
		{
			/* deal with foo(int a, char b) */
			struct token_list* a = sym_declare(global_token->s, type_size, function->arguments);
			if(NULL == function->arguments)
			{
				a->depth = -4;
			}
			else
			{
				a->depth = function->arguments->depth - 4;
			}

			global_token = global_token->next;
			function->arguments = a;
		}

		/* ignore trailing comma (needed for foo(bar(), 1); expressions*/
		if(global_token->s[0] == ',') global_token = global_token->next;
	}
	global_token = global_token->next;
}

void declare_function()
{
	current_count = 0;
	function = sym_declare(global_token->prev->s, NULL, global_function_list);

	/* allow previously defined functions to be looked up */
	global_function_list = function;
	collect_arguments();

	/* If just a prototype don't waste time */
	if(global_token->s[0] == ';') global_token = global_token->next;
	else
	{
		emit_out("# Defining function ");
		emit_out(function->s);
		emit_out("\n");
		emit_out(":FUNCTION_");
		emit_out(function->s);
		emit_out("\n");
		statement();

		/* Prevent duplicate RETURNS */
		if(!match("RETURN\n", out->s))
		{
			emit_out("RETURN\n");
		}
	}
}

/*
 * program:
 *     declaration
 *     declaration program
 *
 * declaration:
 *     CONSTANT identifer value
 *     type-name identifier ;
 *     type-name identifier ( parameter-list ) ;
 *     type-name identifier ( parameter-list ) statement
 *
 * parameter-list:
 *     parameter-declaration
 *     parameter-list, parameter-declaration
 *
 * parameter-declaration:
 *     type-name identifier-opt
 */
struct token_list* program()
{
	Address_of = FALSE;
	out = NULL;
	function = NULL;
	struct type* type_size;

new_type:
	if (NULL == global_token) return out;
	if(match("CONSTANT", global_token->s))
	{
		global_token = global_token->next;
		global_constant_list = sym_declare(global_token->s, NULL, global_constant_list);
		global_constant_list->arguments = global_token->next;
		global_token = global_token->next->next;
	}
	else
	{
		type_size = type_name();
		if(NULL == type_size)
		{
			goto new_type;
		}
		/* Add to global symbol table */
		global_symbol_list = sym_declare(global_token->s, type_size, global_symbol_list);
		global_token = global_token->next;
		if(match(";", global_token->s))
		{
			/* Ensure 4 bytes are allocated for the global */
			globals_list = emit(":GLOBAL_", globals_list);
			globals_list = emit(global_token->prev->s, globals_list);
			globals_list = emit("\nNULL\n", globals_list);

			global_token = global_token->next;
		}
		else if(match("(", global_token->s)) declare_function();
		else if(match("=",global_token->s))
		{
			/* Store the global's value*/
			globals_list = emit(":GLOBAL_", globals_list);
			globals_list = emit(global_token->prev->s, globals_list);
			globals_list = emit("\n", globals_list);
			global_token = global_token->next;
			if(in_set(global_token->s[0], "0123456789"))
			{ /* Assume Int */
				globals_list = emit("%", globals_list);
				globals_list = emit(global_token->s, globals_list);
				globals_list = emit("\n", globals_list);
			}
			else if(('"' == global_token->s[0]))
			{ /* Assume a string*/
				globals_list = emit(parse_string(global_token->s), globals_list);
			}
			else
			{
				line_error();
				fputs("Recieved ", stderr);
				fputs(global_token->s, stderr);
				fputs(" in program\n", stderr);
				exit(EXIT_FAILURE);
			}

			global_token = global_token->next;
			require_match("ERROR in Program\nMissing ;\n", ";");
		}
		else
		{
			line_error();
			fputs("Recieved ", stderr);
			fputs(global_token->s, stderr);
			fputs(" in program\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
	goto new_type;
}

void recursive_output(struct token_list* i, FILE* out)
{
	if(NULL == i) return;
	recursive_output(i->next, out);
	fputs(i->s, out);
}
