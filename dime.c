/*******************************
 * dime.c
 *
 * Source code for DIstributed MakE
 *
 ******************************/

#include "util.h"
#include "dime.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/*********
 * Simple usage instructions
 *********/
void dime_usage(char* progname) {
	fprintf(stderr, "Usage: %s [options] [target]\n", progname);
	fprintf(stderr, "-f FILE\t\tRead FILE as a dimefile.\n");
	fprintf(stderr, "-h\t\tPrint this message and exit.\n");
	fprintf(stderr, "-n\t\tDon't actually execute commands, just print them.\n");
	exit(0);
}

/****************************** 
 * this is the function that, when given a proper filename, will
 * parse the dimefile and read in the rules
 ***************/
rule_node_t* parse_file(char* filename) {
	char* buffer = malloc(160*sizeof(char));
	char* line;
	FILE* fp = file_open(filename);
	rule_node_t* rlist = NULL;
	rule_t* current_rule = NULL;
	while((line = file_getline(buffer, fp)) != NULL) {
		if(line[0] == '#') {
			// do nothing because this line is a comment
		} else {
			// not a comment
			if(current_rule != NULL) {
				if(line[0] == '}') {
					// then this is the closing } of a rule
					rule_node_t* node = rule_node_create(current_rule);
					node->next = rlist;
					rlist = node;
					current_rule = NULL;
				} else {
					// this is just another command line in the rule
					char* trim_line = trim(line);
					rule_add_commandline(current_rule, trim_line);
					free(trim_line);
				}
			} else {
				if(strchr(line, ':') != NULL) {
					// this is the start of a rule
					char* trim_targ = trim(strtok(line, ":"));
					current_rule = rule_create(trim_targ);
					free(trim_targ);
					char* alldeps = strtok(NULL, ":");
					char* dep = strtok(alldeps, " ");
					if(dep != NULL) {
						do {
							if(*dep != '{') {
								rule_add_dep(current_rule, dep);
							}
						} while((dep = strtok(NULL, " ")) != NULL);
					}
				}
			}
		}
	}
	fclose(fp);
	free(buffer);
	return rlist;
}

/**************
 * Exec targets with recursive calls
 **************/
void exec_target_rec(rule_t* rule, rule_node_t* list) {
	str_node_t* sptr;
	rule_node_t* rptr;
	for(sptr = rule->deps; sptr != NULL; sptr = sptr->next) {
		// for each dependency, see if there's a rule, then exec that rule
		for(rptr = list; rptr != NULL; rptr = rptr->next) {
			if(strcmp(sptr->str, rptr->rule->target) == 0) {
				exec_target_rec(rptr->rule, list);
			}
		}
	}

	fake_exec(rule);
}

/***********
 * Function for 'fake execing' for HW4.
 * Don't exec, just print commandlines and wait 1 sec per commandline
 ***********/
void fake_exec(rule_t* rule) {
	str_node_t* sptr;
	for(sptr = rule->commandlines; sptr != NULL; sptr = sptr->next) {
		printf("%s\n",sptr->str);
		usleep(500000);
	}
}

/*********
 * Given a target list and the list of rules, execute the targets.
 *********/
void execute_targets(int targetc, char* targetv[], rule_node_t* list) {
	rule_node_t* ptr = list;
	int i;
	if(targetc == 0) {
		// no target specified on command line!
		for(ptr = list; ptr->next != NULL; ptr = ptr->next);
		if(ptr == NULL) {
			fprintf(stderr, "Error, no targets in dimefile.\n");
		} else {
			exec_target_rec(ptr->rule, list);
		}
	} else {
		for(i = 0; i < targetc; i++) {
			for(ptr = list; ptr != NULL; ptr = ptr->next) {
				if(strcmp(targetv[i], ptr->rule->target) == 0) {
					exec_target_rec(ptr->rule, list);
					break;
				}
			}
			if(ptr == NULL) {
				fprintf(stderr, "Error, target '%s' not found.\n",targetv[i]);
				exit(1);
			}
		}
	}
}

int main(int argc, char* argv[]) {
	// Declarations for getopt
	extern int optind;
	extern char* optarg;
	int ch;
	char* format = "f:h";
	
	// Variables you'll want to use
	char* filename = "Dimefile";

	// Part 2.2.1: Use getopt code to take input appropriately.
	while((ch = getopt(argc, argv, format)) != -1) {
		switch(ch) {
			case 'f':
				filename = strdup(optarg);
				break;
			case 'h':
				dime_usage(argv[0]);
				break;
		}
	}
	argc -= optind;
	argv += optind;

	// parse the given file, then execute targets
	rule_node_t* list = parse_file(filename);
	execute_targets(argc, argv, list);
	rule_node_free(list);
	return 0;
}
