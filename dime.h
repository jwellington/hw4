/**************************
 * dime.h -- the header file for dime.c, a 
 * distributed make
 *
 *
 *
 *************************/

#ifndef _DIME_H_
#define _DIME_H_

#define true 1
#define false 0
typedef int bool;

#define LINE_DELAY 200000

void dime_usage(char*);
rule_node_t* parse_file(char*);
void fake_exec(rule_t*);
void exec_target_rec(rule_t*, rule_node_t*);
void execute_targets(int, char**, rule_node_t*, rule_node_t*, int);
void* helper_thread(void*);

#endif
