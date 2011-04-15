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
#include <semaphore.h>

pthread_mutex_t mutex;
sem_t sem_lock;
pthread_cond_t queue_full;
pthread_cond_t queue_empty;
pthread_cond_t finished_execution;

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
void exec_target_rec(rule_t* rule, rule_node_t* list, ARG_HOLDER* argholder) {
	str_node_t* sptr;
	rule_node_t* rptr;
	for(sptr = rule->deps; sptr != NULL; sptr = sptr->next) {
		// for each dependency, see if there's a rule, then exec that rule
		for(rptr = list; rptr != NULL; rptr = rptr->next) {
			if(strcmp(sptr->str, rptr->rule->target) == 0) {
				exec_target_rec(rptr->rule, list, argholder);
			}
		}
	}
    
    //Should add the target to the queue instead so helper threads can exec
	//fake_exec(rule);
	rule_node_t* queue = argholder->rule_queue;
	int max_queue_length = argholder->max_queue_length;
	
	int ql = queue_length(queue) - 1;
	int empty_queue = 0;
	if (ql == max_queue_length)
	{
	    //Wait for the queue length to go down
	    cond_wait(&queue_full, &mutex);
	}
	else if (ql == 0)
	{
	    empty_queue = 1;
	}
    //Attach the new target to the back of the queue
    sem_wait(&sem_lock);
    rule_node_t* qnode = queue;
    rule_node_t* qnext = qnode->next;
    while (qnext != NULL)
    {
        qnode = qnext;
        qnext = qnode->next;
    }
    rule_node_t* new_node = (rule_node_t*)(malloc(sizeof(rule_node_t)));
    new_node->rule = rule;
    new_node->next = NULL;
    qnode->next = new_node;
    sem_post(&sem_lock);
    //If the queue was empty, signal the helper threads that it is not
    if (empty_queue)
    {
        pthread_cond_signal(&queue_empty);
    }
}

/***********
 * Function for 'fake execing' for HW4.
 * Don't exec, just print commandlines and wait TIME_DELAY usecs per commandline
 ***********/
void fake_exec(rule_t* rule) {
	str_node_t* sptr;
	for(sptr = rule->commandlines; sptr != NULL; sptr = sptr->next) {
		printf("%s\n",sptr->str);
		usleep(LINE_DELAY);
	}
}

/*********
 * Given a target list and the list of rules, execute the targets.
 *********/
void execute_targets(int targetc, char* targetv[], rule_node_t* list,
                     ARG_HOLDER* argholder) {
	rule_node_t* ptr = list;
	int i;
	if(targetc == 0) {
		// no target specified on command line!
		//Go to the last rule (which was first in the file) and run it
		for(ptr = list; ptr->next != NULL; ptr = ptr->next);
		{
		    //Do nothing, we're just getting to the last rule
		}
	    if(ptr == NULL) {
		    fprintf(stderr, "Error, no targets in dimefile.\n");
	    } else {
		    exec_target_rec(ptr->rule, list, argholder);
	    }
	} else {
		for(i = 0; i < targetc; i++) {
			for(ptr = list; ptr != NULL; ptr = ptr->next) {
				if(strcmp(targetv[i], ptr->rule->target) == 0) {
					exec_target_rec(ptr->rule, list, argholder);
					break;
				}
			}
			if(ptr == NULL) {
				fprintf(stderr, "Error, target '%s' not found.\n",targetv[i]);
				exit(1);
			}
		}
	}
	argholder->finished_adding = 1;
	while (argholder->threads_not_done > 0)
	{
	    cond_wait(&finished_execution, &mutex);
	}
}

void* helper_thread(void* args)
{
    //Unpack args
    ARG_HOLDER* argholder = (ARG_HOLDER*)(args);
    rule_node_t* queue = argholder->rule_queue;
    int queue_size = argholder->max_queue_length;
    argholder->threads_not_done++;
    int threadno = argholder->threads_not_done;
	int ql = queue_length(queue) - 1;
    while (!argholder->finished_adding || ql > 0)
    {
        //Check queue and "execute" targets on it
	    int full_queue = 0;
	    while (!argholder->done && ql == 0)
	    {
	        //Wait for the queue length to go up
	        cond_wait(&queue_empty, &mutex);
	        ql = queue_length(queue) - 1;
	    }
	    if (ql == queue_size)
	    {
	        full_queue = 1;
	    }
	    if (ql > 0 && !argholder->done)
        {
            //Remove the first target from the queue and "execute" it
            sem_wait(&sem_lock);
            rule_node_t* first_node = queue->next;
            rule_node_t* qnode = first_node->next;
            rule_t* cur_rule = first_node->rule;
            queue->next = qnode;
            free(first_node);
            sem_post(&sem_lock);
            //If the queue was full, signal the main thread that it is not
            if (full_queue)
            {
                pthread_cond_broadcast(&queue_full);
            }
            
            fake_exec(cur_rule);
            
            //Put rule on output queue
            rule_node_t* out_first = argholder->output_queue;
            rule_node_t* output = (rule_node_t*)malloc(sizeof(rule_node_t));
            output->rule = cur_rule;
            output->next = out_first;
            argholder->output_queue = output;
        }
        ql = queue_length(queue) - 1;
    }
    argholder->done = 1;
    argholder->threads_not_done--;
    pthread_cond_broadcast(&queue_empty);
    pthread_cond_signal(&finished_execution);
    //printf("Thread %d exiting.\n", threadno);
    return NULL;
}

int main(int argc, char* argv[]) {
	// Declarations for getopt
	extern int optind;
	extern char* optarg;
	int ch;
	char* format = "f:hq:t:";
	
	// Variables you'll want to use
	char* filename = "Dimefile";
	
	int num_threads = 3;
	char* queue_size = "5";

	// Part 2.2.1: Use getopt code to take input appropriately.
	while((ch = getopt(argc, argv, format)) != -1) {
		switch(ch) {
			case 'f':
				filename = strdup(optarg);
				break;
			case 'h':
				dime_usage(argv[0]);
				break;
	        case 't':
	            num_threads = atoi(optarg);
	            if (num_threads < 1)
	            {
	                error("The number of helper threads must be at least 1.");
	            }
	            break;
	        case 'q':
	            queue_size = optarg;
	            if (atoi(queue_size) < 1)
	            {
	                error("The queue size must be at least 1.");
	            }
	            break;
		}
	}
	argc -= optind;
	argv += optind;
	
	//Set up queue for targets
	rule_node_t* rule_queue = (rule_node_t*)(malloc(sizeof(rule_node_t)));
	rule_node_t* output_queue = (rule_node_t*)(malloc(sizeof(rule_node_t)));
	//The first "real" entry of rule_queue is the second, so we can change it
	//while keeping the address of rule_queue constant
	rule_queue->rule = NULL;
	rule_queue->next = NULL;
	
	ARG_HOLDER argholder;
	argholder.rule_queue = rule_queue;
	argholder.output_queue = output_queue;
	argholder.max_queue_length = atoi(queue_size);
	argholder.threads_not_done = 0;
	argholder.finished_adding = 0;
	argholder.done = 0;
	
	pthread_mutex_init(&mutex, NULL);
	sem_init(&sem_lock, 0, 1);
	pthread_cond_init(&queue_full, NULL);
	pthread_cond_init(&queue_empty, NULL);
	pthread_cond_init(&finished_execution, NULL);
	
	//Set up threads
	PTHREAD_NODE* threads = NULL;
	int i;
	for (i = 0; i < num_threads; i++)
	{
	    pthread_t* thread = (pthread_t*)(malloc(sizeof(pthread_t)));
	    if (pthread_create(thread, NULL, helper_thread, (void*)(&argholder)) != 0)
	    {
	        error("Failed to create helper thread.");
	    }
	    else
	    {
	        PTHREAD_NODE* cur_node = (PTHREAD_NODE*)(malloc(sizeof(PTHREAD_NODE)));
	        cur_node->thread = thread;
	        cur_node->next = threads;
	        threads = cur_node;
	    }
	}

	// parse the given file, then execute targets
	rule_node_t* list = parse_file(filename);
	execute_targets(argc, argv, list, &argholder);
	rule_node_free(list);
	rule_queue_free(rule_queue);
	rule_queue_free(output_queue);
	
	//Rejoin threads
	PTHREAD_NODE* pthread_ptr = threads;
	while (pthread_ptr != NULL)
	{
        if (pthread_join(*(pthread_ptr->thread),NULL) != 0)
        {
            error("Couldn't join helper thread.");
        }
        else
        {
            printf("Joined helper thread.\n");
        }
        PTHREAD_NODE* temp = pthread_ptr;
        pthread_ptr = pthread_ptr->next;
        free(temp->thread);
        free(temp);
	}
	
	pthread_mutex_destroy(&mutex);
	sem_destroy(&sem_lock);
	pthread_cond_destroy(&queue_full);
	pthread_cond_destroy(&queue_empty);
	pthread_cond_destroy(&finished_execution);
	
	return 0;
}
