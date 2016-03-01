// ---------------------------
#include <stdio.h>
#include <stdlib.h>
#include "pattern_matching.h"
// ---------------------------

/* PRIVATE FUNCTIONS DECLARATION: */
void printFSM(slist_t* list, int state_id);
void printFailure(slist_t* list);
void printOutput(slist_t* list);



  // ******************************************************************* //
 // ----------------------------- TESTER: ----------------------------- //
// ******************************************************************* //



int main()
{	
	int i;
	pm_t* fsm = (pm_t*)malloc(sizeof(pm_t));
	if (!fsm)
		printf("FSM Allocation Failed!\n");
	// Checking the pm_init (init of the FSM):
	int pmInit = pm_init(fsm);

	// Adding the patterns from the example:
	pm_addstring(fsm, "a" , 1);
	pm_addstring(fsm, "abc" , 3);
	pm_addstring(fsm, "bca" , 3);
	pm_addstring(fsm, "cab" , 3);
	pm_addstring(fsm, "acb" , 3);
	/*printf("adding =  %d\n" ,pm_addstring(fsm , "e" , 1));
	printf("adding =  %d\n" ,pm_addstring(fsm , "be" , 2));
	printf("adding =  %d\n" ,pm_addstring(fsm , "bd" , 2));
	printf("adding =  %d\n" ,pm_addstring(fsm , "bcd" , 3));
	printf("adding =  %d\n" ,pm_addstring(fsm , "cdbcab" , 6));
	printf("adding =  %d\n" ,pm_addstring(fsm , "bcaa" , 4));*/
	pm_makeFSM(fsm);
	slist_destroy(pm_fsm_search(fsm->zerostate , "xyzabcabde" , 10) , 1);
	pm_destroy(fsm);


	// Printing the FSM Tree - Starting from s(0):
	//printFSM(fsm->zerostate->_transitions, fsm->zerostate->id);

	/* Printing the created states: 
	printf("States:\n");
	for (i=0; i<fsm->newstate; i++)
		printf("S(%d),", i);
	if (fsm->newstate == 15)
		printf("\nNumber of states: 15 (working)\n\n");
	else
		printf("Wrong number of states!\n");
	
	// Printing the Failures - Starting from s(1):
	pm_makeFSM(fsm);
	printFailure(fsm->zerostate->_transitions);

	printOutput(fsm->zerostate->_transitions);//xyzabcabde
	slist_t *mlist = pm_fsm_search(fsm->zerostate , "xyzabcabde" , 10);
	printf("size = %d\n" , slist_size(mlist));
	slist_node_t *p = slist_head(mlist);
	while(p != NULL)
	{
		pm_match_t *q = (pm_match_t*)slist_data(p);
		printf("Pattern: %s, start at: %d, ends at: %d, last state = %d\n" , q->pattern , q->start_pos , q->end_pos , q->fstate->id);
		p = slist_next(p);
	}*/
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* PRIVATE FUNCTIONS: */

void printFSM(slist_t* list, int state_id)
{
	if(slist_size(list) == 0)
		return;

	slist_node_t* current = slist_head(list);
	
	while (current)
	{
		pm_labeled_edge_t*currentEdge = (pm_labeled_edge_t*)slist_data(current);  // current edge.
		pm_state_t* nextState = currentEdge->state;                              // next state.
		slist_t* nextList = nextState->_transitions;                            // transitions list of the next state.
		int sizeof_nextList = slist_size(nextList);                            // size of the next transitions list.

		if (sizeof_nextList == 0)
		{
			printf("S(%d) -> (%c) -> S(%d)\n", state_id, currentEdge->label, nextState->id);
			printf("\n");
		}
		else
		{
			printf("S(%d) -> (%c) -> S(%d)\n", state_id, currentEdge->label, nextState->id);
			printFSM(nextList, nextState->id);
		}

		current = slist_next(current);
	}

	return;
}

void printFailure(slist_t* list)
{
	if(slist_size(list) == 0)
		return;

	slist_node_t* current = slist_head(list);
	
	while (current)
	{
		pm_labeled_edge_t* currentEdge = (pm_labeled_edge_t*)slist_data(current);  // current edge.
		pm_state_t* nextState = currentEdge->state;                              // next state.
		slist_t* nextList = nextState->_transitions;                            // transitions list of the next state.

		printf("S(%d) -> failure: S(%d)\n", nextState->id, nextState->fail->id);
		printFailure(nextList);

		current = slist_next(current);
	}

	return;
}

void printOutput(slist_t* list)
{
	if(slist_size(list) == 0)
		return;

	slist_node_t* current = slist_head(list);
	
	while (current)
	{
		pm_labeled_edge_t* currentEdge = (pm_labeled_edge_t*)slist_data(current);  // current edge.
		pm_state_t* nextState = currentEdge->state;                              // next state.
		slist_t* nextList = nextState->_transitions;                            // transitions list of the next state.

		printf("S(%d) - Output: ", nextState->id);

		slist_node_t* c = slist_head(nextState->output);
		while (c)
		{
			char* pattern = (char*)slist_data(c);
			if (pattern)
				printf("%s, ", pattern);
			printf("\n");

			c = slist_next(c);
		}
		printf("\n");
		
		printOutput(nextList);

		current = slist_next(current);
	}

	return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////