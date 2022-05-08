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

#include "cc_globals.h"

FILE* input;
struct token_list* token;
int line;
char* file;

int clearWhiteSpace(int c)
{
	if(in_set(c ," \t\n")) return clearWhiteSpace(fgetc(input));
	return c;
}

void push_byte(int c)
{
	hold_string[string_index] = c;
	string_index = string_index + 1;
}

int consume_byte(int c)
{
	push_byte(c);
	return fgetc(input);
}

int consume_word(int c, int frequent)
{
	int escape = FALSE;
	do
	{
		if(!escape && '\\' == c ) escape = TRUE;
		else escape = FALSE;
		c = consume_byte(c);
	} while(escape || (c != frequent));
	return fgetc(input);
}


void fixup_label()
{
	int hold = ':';
	int prev;
	int i = 0;
	do
	{
		prev = hold;
		hold = hold_string[i];
		hold_string[i] = prev;
		i = i + 1;
	} while(0 != hold);
}

int preserve_keyword(int c)
{
	while(!in_set(c, "(){}[],: \t\n;"))
	{
		c = consume_byte(c);
	}
	if(':' == c)
	{
		fixup_label();
		return ' ';
	}
	return c;
}

void reset_hold_string()
{
	int i = string_index + 2;
	while(0 != i)
	{
		hold_string[i] = 0;
		i = i - 1;
	}
}

int get_token(int c)
{
	struct token_list* current = calloc(1, sizeof(struct token_list));

	reset_hold_string();
	string_index = 0;

	c = clearWhiteSpace(c);
	if(in_set(c, "(){},"))
	{
		push_byte(c);
		c = ' ';
	}
	else if(in_set(c, "\'\""))
	{
		c = consume_word(c, c);
	}
	else if(c == '#')
	{
		while(c != '\n') c = fgetc(input);
	}
	else if(c == EOF)
	{
		free(current);
		return c;
	}
	else
	{
		c = preserve_keyword(c);
	}

	/* More efficiently allocate memory for string */
	current->s = calloc(string_index + 2, sizeof(char));
	strcpy(current->s, hold_string);

	current->prev = token;
	current->next = token;
	current->linenumber = line;
	current->filename = file;
	token = current;
	return c;
}

struct token_list* reverse_list(struct token_list* head)
{
	struct token_list* root = NULL;
	while(NULL != head)
	{
		struct token_list* next = head->next;
		head->next = root;
		root = head;
		head = next;
	}
	return root;
}

struct token_list* read_all_tokens(FILE* a, struct token_list* current, char* filename)
{
	input  = a;
	line = 1;
	file = filename;
	token = current;
	int ch =fgetc(input);
	while(EOF != ch) ch = get_token(ch);

	return token;
}
