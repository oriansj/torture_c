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
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include"cc.h"

/* The core functions */
void initialize_types();
struct token_list* read_all_tokens(FILE* a, struct token_list* current, char* filename);
struct token_list* reverse_list(struct token_list* head);
struct token_list* program();
void recursive_output(struct token_list* i, FILE* out);
int match(char* a, char* b);
void file_print(char* s, FILE* f);
char* parse_string(char* string);

int main()
{
	hold_string = calloc(MAX_STRING, sizeof(char));
	FILE* in = fopen("tape_01", "r");
	FILE* destination_file = fopen("tape_02", "w");

	global_token = read_all_tokens(in, global_token, "tape_01");

	if(NULL == global_token)
	{
		file_print("Either no input files were given or they were empty\n", stderr);
		exit(EXIT_FAILURE);
	}
	global_token = reverse_list(global_token);

	initialize_types();
	struct token_list* output_list = program();

	recursive_output(output_list, destination_file);
	file_print("\n# Program global variables\n", destination_file);
	recursive_output(globals_list, destination_file);
	file_print("\n# Program strings\n", destination_file);
	recursive_output(strings_list, destination_file);
	file_print("\n:ELF_end\n", destination_file);
	return EXIT_SUCCESS;
}
