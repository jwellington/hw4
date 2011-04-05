/********************
 * util.h
 *
 * You may put your utility function definitions here
 * also your structs, if you create any
 *********************/

#include <stdio.h>
#include <unistd.h>

// the following ifdef/def pair prevents us from having problems if 
// we've included util.h in multiple places... it's a handy trick
#ifndef _UTIL_H_
#define _UTIL_H_

// This stuff is for easy file reading
FILE* file_open(char*);
char* file_getline(char*, FILE*);

// struct and functions for a linked list of strings
typedef struct str_node_s str_node_t;

struct str_node_s {
	char* str;
	str_node_t* next;
};

str_node_t* str_node_create(char*);
void str_node_free(str_node_t*);

// struct for rule info
typedef struct rule_s rule_t;
struct rule_s {
	char* target;
	str_node_t* deps;
	str_node_t* commandlines;
};

// functions for rules
rule_t* rule_create(char*);
void rule_add_dep(rule_t*, char*);
void rule_add_commandline(rule_t*, char*);
void rule_free(rule_t*);

// struct for list of rules
typedef struct rule_node_s rule_node_t;
struct rule_node_s {
	rule_t* rule;
	rule_node_t* next;
};

rule_node_t* rule_node_create(rule_t*);
void rule_node_free(rule_node_t*);

// struct for pid list
typedef struct pid_node_s pid_node_t;
struct pid_node_s {
	pid_t pid;
	pid_node_t* next;
};

pid_node_t* pid_node_create(pid_t);
void pid_node_free(pid_node_t*);

// string munging functions
char* trim(char*);
char** get_argvec(char*);

#endif
