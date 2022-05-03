/* Copyright (C) 2016 Jeremiah Orians
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

#include "cc.h"
#include <stdint.h>

/* Initialize default types */
void initialize_types()
{
	/* Define void */
	global_types = calloc(1, sizeof(struct type));
	global_types->name = "int";
	global_types->size = 4;
	global_types->type = global_types;
	/* int* has the same properties as int */
	global_types->indirect = global_types;

	/* Define char* */
	struct type* b = calloc(1, sizeof(struct type));
	b->name = "char*";
	b->size = 4;
	b->type = b;

	/* Define char */
	struct type* c = calloc(1, sizeof(struct type));
	c->name = "char";
	c->size = 1;
	c->type = c;

	/*fix up indrects for chars */
	c->indirect = b;
	b->indirect = c;

	/* Define FILE */
	struct type* e = calloc(1, sizeof(struct type));
	e->name = "FILE";
	e->size = 4;
	e->type = e;
	/* FILE* has the same properties as FILE */
	e->indirect = e;

	/* Define FUNCTION */
	struct type* f = calloc(1, sizeof(struct type));
	f->name = "FUNCTION";
	f->size = 4;
	f->type = f;
	/* FUNCTION* has the same properties as FUNCTION */
	f->indirect = f;

	/* Finalize type list */
	e->next = f;
	c->next = e;
	global_types->next = c;
	prim_types = global_types;
}

struct type* lookup_type(char* s, struct type* start)
{
	struct type* i;
	for(i = start; NULL != i; i = i->next)
	{
		if(match(i->name, s))
		{
			return i;
		}
	}
	return NULL;
}

struct type* lookup_member(struct type* parent, char* name)
{
	struct type* i;
	for(i = parent->members; NULL != i; i = i->members)
	{
		if(match(i->name, name)) return i;
	}
	exit(EXIT_FAILURE);
}

struct type* type_name();
void require_match(char* message, char* required);

int member_size;
struct type* build_member(struct type* last, int offset)
{
	struct type* member_type = type_name();
	struct type* i = calloc(1, sizeof(struct type));
	i->name = global_token->s;
	global_token = global_token->next;
	i->members = last;

	i->size = member_type->size;
	member_size = i->size;
	i->type = member_type;
	i->offset = offset;
	return i;
}

void create_struct()
{
	int offset = 0;
	member_size = 0;
	struct type* head = calloc(1, sizeof(struct type));
	struct type* i = calloc(1, sizeof(struct type));
	head->name = global_token->s;
	i->name = global_token->s;
	head->indirect = i;
	i->indirect = head;
	head->next = global_types;
	global_types = head;
	global_token = global_token->next;
	i->size = 4;
	struct type* last = NULL;
	while('}' != global_token->s[0])
	{
		last = build_member(last, offset);
		offset = offset + member_size;
	}

	global_token = global_token->next;

	head->size = offset;
	head->members = last;
	i->members = last;
}


/*
 * type-name:
 *     char *
 *     int
 *     struct
 *     FILE
 *     void
 */
struct type* type_name()
{
	int structure = match("struct", global_token->s);

	if(structure)
	{
		global_token = global_token->next;
	}

	struct type* ret = lookup_type(global_token->s, global_types);

	if(NULL == ret && !structure)
	{
		exit(EXIT_FAILURE);
	}
	else if(NULL == ret)
	{
		create_struct();
		return NULL;
	}

	global_token = global_token->next;

	while(global_token->s[0] == '*')
	{
		ret = ret->indirect;
		global_token = global_token->next;
	}

	return ret;
}
