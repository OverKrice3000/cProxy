#include <stdio.h>
#include <stdbool.h>

int parse_query(char** query_init, int* query_length);
int find_header_by_name(char* query, char* name, char** value, int* val_length);
bool starts_with_name(char* name, char* string);
int urlcpy(char* query, char** url_addr);
