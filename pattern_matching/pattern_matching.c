#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pattern_matching.h"

/* Initializes the fsm parameters (the fsm itself sould be allocated).  Returns 0 on success, -1 on failure. 
*  this function should init zero state
*/
int pm_init(pm_t *fsm){
	if(fsm == NULL)
	{
		printf("ERROR: The FSM is NULL\n");
		return -1;
	}
	fsm->zerostate = (pm_state_t*)malloc(sizeof(pm_state_t)); // create the zerostate
	if(fsm->zerostate == NULL)
	{
		printf("ERROR: Cannot alocate memory\n");
		exit(-1);
	}
	fsm->zerostate->output = (slist_t*)malloc(sizeof(slist_t));
	if(fsm->zerostate->output == NULL)
	{
		printf("ERROR: Cannot alocate memory\n");
		exit(-1);
	}
	fsm->zerostate->_transitions = (slist_t*)malloc(sizeof(slist_t));
	if(fsm->zerostate->_transitions == NULL)
	{
		printf("ERROR: Cannot alocate memory\n");
		exit(-1);
	}
	slist_init(fsm->zerostate->output);
	slist_init(fsm->zerostate->_transitions);
	fsm->newstate = 1;
	fsm->zerostate->fail = NULL;
	fsm->zerostate->id = 0; 
	fsm->zerostate->depth = 0;
	return 0 ;
}

/* Adds a new string to the fsm, given that the string is of length n. 
   Returns 0 on success, -1 on failure.*/
int pm_addstring(pm_t *fsm,unsigned char *word, size_t n){
	if(fsm == NULL)
	{
		printf("ERROR: The FSM is NULL\n");
		return -1;
	}
	if(word == NULL)
	{
		printf("ERROR: The word is NULL\n");
		return -1;
	}
	if(strlen(word) != n)
	{
		printf("ERROR: The word length is not equal to pram n\n");
		return -1;
	}	
	int i; unsigned char letter;
	pm_state_t *cState = fsm->zerostate;
	pm_state_t *nState = NULL;
	for(i = 0; i < n; i++)
	{
		letter = word[i];
		nState = pm_goto_get(cState , letter);
		if(nState == NULL)
		{	
				nState = (pm_state_t*)malloc(sizeof(pm_state_t));
				if(nState == NULL)
				{
					printf("ERROR: Cannot alocate memory\n");
					exit(-1);
				}
				printf("Allocating state %d\n" , fsm->newstate);
				nState->output = (slist_t*)malloc(sizeof(slist_t));
				if(nState->output == NULL)
				{
					printf("ERROR: Cannot alocate memory\n");
					exit(-1);
				}
				nState->_transitions = (slist_t*)malloc(sizeof(slist_t));
				if(nState->_transitions == NULL)
				{
					printf("ERROR: Cannot alocate memory\n");
					exit(-1);
				}
				slist_init(nState->output);
				slist_init(nState->_transitions);
				nState->id = fsm->newstate;
				fsm->newstate++;
				nState->depth = cState->depth + 1;
				nState->fail = NULL;
				if(pm_goto_set(cState , letter , nState) == -1)
					return -1;
		}
		cState = nState;
	}
	if(slist_append(cState->output , word) == -1)
		return -1;
	return 0;
}


/* Finalizes construction by setting up the failrue transitions, as
   well as the goto transitions of the zerostate. 
   Returns 0 on success, -1 on failure.*/
int pm_makeFSM(pm_t *fsm){
	if(fsm == NULL)
	{
		printf("ERROR: The FSM is NULL\n");
		return -1;
	}
	pm_labeled_edge_t *edge;
	pm_state_t *cState , *fState;
	slist_node_t *p;
	slist_t *qState = (slist_t*)malloc(sizeof(slist_t)); // our queue states
	if(qState == NULL)
	{
		printf("ERROR: Cannot alocate memory\n");
		exit(-1);
	}
	slist_init(qState);
	// p = pointer to the head of zero state edges list.
	p = slist_head(fsm->zerostate->_transitions); 
	// insert to the queue all the state in depth 1;
	while(p != NULL)
	{
		edge = (pm_labeled_edge_t*)slist_data(p);
		edge->state->fail = fsm->zerostate;
		if(slist_append(qState , edge->state))
			return -1;
		p = slist_next(p);
	}
	while(slist_size(qState) > 0)
	{
		//Let cState be the next state in queue
		cState = slist_pop_first(qState);
		// p = pointer to the head of next state in queue - edges list.
		p = slist_head(cState->_transitions);
		while(p != NULL)
		{
			edge = (pm_labeled_edge_t*)slist_data(p);
			if(slist_append(qState , edge->state))
				return -1;
			fState = cState->fail;
			while(fState != NULL && pm_goto_get(fState , edge->label) == NULL)
				fState = fState->fail;
			if(fState == NULL)
				edge->state->fail = fsm->zerostate;
			else
			{
				printf("Setting f(%d) = %d\n" , edge->state->id ,pm_goto_get(fState , edge->label)->id);
				edge->state->fail = pm_goto_get(fState , edge->label); 
			}
			if(slist_append_list(edge->state->output , edge->state->fail->output))
				return -1;
			p = slist_next(p);
		}
	}
	slist_destroy_t qStateData = SLIST_LEAVE_DATA;
	slist_destroy(qState, qStateData);
}


/* Set a transition arrow from this from_state, via a symbol, to a
   to_state. will be used in the pm_addstring and pm_makeFSM functions.
   Returns 0 on success, -1 on failure.*/   
int pm_goto_set(pm_state_t *from_state,
			   unsigned char symbol,
			   pm_state_t *to_state){
	if(from_state == NULL || to_state == NULL)
	{
		printf("One or both parm state is NULL\n");
		return -1;;
	}
	pm_labeled_edge_t *edge = (pm_labeled_edge_t*)malloc(sizeof(pm_labeled_edge_t));
	if(edge == NULL)
	{
		printf("ERROR: Cannot alocate memory\n");
		exit(-1);
	}
	edge->label = symbol; edge->state = to_state;
	if(slist_append(from_state->_transitions , edge) == -1)
		return -1;
	printf("%d -> %c -> %d\n", from_state->id , symbol , to_state->id);
	return 0;
}

/* Returns the transition state.  If no such state exists, returns NULL. 
   will be used in pm_addstring, pm_makeFSM, pm_fsm_search, pm_destroy functions. */
pm_state_t* pm_goto_get(pm_state_t *state,
					    unsigned char symbol){
	if(state == NULL)
	{
		printf("Parm state is NULL\n");
		return NULL;
	}
	slist_node_t *p = slist_head(state->_transitions);
	pm_labeled_edge_t* edge;
	while(p != NULL)
	{
		edge = (pm_labeled_edge_t*)slist_data(p);
		if(edge->label == symbol)
			return edge->state;
		p = slist_next(p);
	}
	return NULL;
}



/* Search for matches in a string of size n in the FSM. 
   if there are no matches return empty list */
slist_t* pm_fsm_search(pm_state_t *root,unsigned char *text,size_t n){
	slist_t *matched_list = (slist_t*)malloc(sizeof(slist_t));
	slist_destroy_t freeData = SLIST_LEAVE_DATA;	
	if(matched_list == NULL)
	{
		printf("ERROR: Cannot alocate memory\n");
		exit(-1);
	}
	slist_init(matched_list);
	if(root == NULL)
	{
		printf("Parm root is NULL\n");
		slist_destroy(matched_list , freeData);
		return NULL;
	}
	if(text == NULL)
	{
		printf("ERROR: Text is not define\n");
		slist_destroy(matched_list , freeData);
		return NULL;
	}
	if(strlen(text) != n)
	{
		printf("ERROR: The text length is not equal to pram n\n");
		slist_destroy(matched_list, freeData);
		return NULL;
	}	
	int i;
	slist_node_t *p;
	pm_state_t *state = root;
	for(i=0; i < n; i++)
	{
		while(state != NULL && pm_goto_get(state, text[i]) == NULL)
			state = state->fail;
		if(state != NULL)
		{
			state = pm_goto_get(state , text[i]); 
			int j;
			p = slist_head(state->output);
			for(j = 0; j < slist_size(state->output); j++)
			{
				pm_match_t *newMatch = (pm_match_t*)malloc(sizeof(pm_match_t));
				if(newMatch == NULL)
				{
					printf("ERROR: Cannot alocate memory\n");
					slist_destroy(matched_list , freeData);
					exit(-1);
				}
				newMatch->pattern = slist_data(p);
				newMatch->start_pos = i - strlen(newMatch->pattern) + 1;
				newMatch->end_pos = i;
				newMatch->fstate = state;
				printf("Pattern: %s, start at: %d, ends at: %d, last state = %d\n" , 
					   newMatch->pattern , newMatch->start_pos , newMatch->end_pos , newMatch->fstate->id);
				if(slist_append(matched_list , newMatch))
				{
						slist_destroy(matched_list ,freeData);
						return NULL;
				}
				p = slist_next(p);
			}	
		}
		else
			state = root;
	}
	return matched_list;
}

// my private function that implement destroy of the FSM in recusion.
void my_destroy(pm_state_t* state){
	slist_t *list = state->_transitions;
	slist_node_t *p = slist_head(list);
	while(p != NULL){
		my_destroy(((pm_labeled_edge_t*)slist_data(p))->state);
		p = slist_next(p);
	}
	if(p == NULL ){
		slist_destroy_t output = SLIST_LEAVE_DATA;
		slist_destroy(state->output , output);
		slist_destroy_t _transitions = SLIST_FREE_DATA;
		slist_destroy(state->_transitions , _transitions);
		free(state);
		return;
	}
}

/* Destroys the fsm, deallocating memory. */
void pm_destroy(pm_t *fsm){
	my_destroy(fsm->zerostate);
	free(fsm);
}



