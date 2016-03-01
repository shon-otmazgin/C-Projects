#include "pattern_matching.h"
#include <stdio.h>
#include <stdlib.h>


void print(slist_t *list)
{	
	if(slist_size(list) == 0)
		return;

	slist_node_t *p = slist_head(list);
	while( p != NULL)
	{
		pm_labeled_edge_t* edge = (pm_labeled_edge_t*)slist_data(p);
		pm_state_t* nextState = edge->state;

		if (slist_size(nextState->_transitions) == 0)
			printf("s: %d , l: %c -> " , nextState->id , edge->label);
		else
		{	
			printf("s: %d , l: %c -> " , nextState->id , edge->label);
			print(nextState->_transitions);
		}
		p = slist_next(p);
		
	}

}

int main(){
	pm_t *fsm = (pm_t*)malloc(sizeof(pm_t));

	printf("init = %d\n" , pm_init(fsm));
	printf("adding =  %d\n" ,pm_addstring(fsm , "e" , 1));
	printf("adding =  %d\n" ,pm_addstring(fsm , "be" , 2));
	printf("adding =  %d\n" ,pm_addstring(fsm , "bd" , 2));
	printf("adding =  %d\n" ,pm_addstring(fsm , "bcd" , 3));
	printf("adding =  %d\n" ,pm_addstring(fsm , "cdbcab" , 6));
	printf("adding =  %d\n" ,pm_addstring(fsm , "bcaa" , 4));

	printf("num os states : %d\n" , fsm->newstate);


	print(fsm->zerostate->_transitions);
	pm_makeFSM(fsm);

}


