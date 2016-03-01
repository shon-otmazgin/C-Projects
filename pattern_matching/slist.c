#include "slist.h"
#include <stdlib.h>
#include <stdio.h>

void slist_init(slist_t *list){
	if(list != NULL){
		slist_head(list) = NULL;
		slist_tail(list) = NULL;
		slist_size(list) = 0;
	}
}

/** Destroy and de-allocate the memory hold by a list
	\param list - a pointer to an existing list
	\param dealloc flag that indicates whether stored data should also be de-allocated */
void slist_destroy(slist_t *list ,slist_destroy_t dealloc){
	if(list == NULL)
		return ;
	slist_node_t *p;
	while(slist_head(list) != NULL)
	{
		p = slist_head(list);
		slist_head(list) = slist_next(slist_head(list));
		if(dealloc)
			free(slist_data(p));
		free(p);	
	}
	free(list);
}

/** Pop the first element in the list
	\param list - a pointer to a list
	\return a pointer to the data of the element, or NULL if the list is empty */
void *slist_pop_first(slist_t *list){
	if(list == NULL)
	{
		printf("parm List is NULL\n");
		return NULL;
	}
	if(!slist_size(list))
	{
		printf("parm List is empty\n");
		return NULL;
	}
	slist_node_t *p = slist_head(list);
	void* data = slist_data(p);
	slist_head(list) = slist_next(slist_head(list));
	free(p);
	slist_size(list)--;
	if(!slist_size(list))
		slist_tail(list) = NULL;
	return data;
}

/** Append data to list (add as last node of the list)
	\param list - a pointer to a list
	\param data - the data to place in the list
	\return 0 on success, or -1 on failure */
int slist_append(slist_t *list,void *data){
	if(list == NULL)
	{
		printf("parm List is NULL\n");
		return -1;
	}
	slist_node_t *p = (slist_node_t*)malloc(sizeof(slist_node_t));
	if(p == NULL)
	{
		printf("ERROR: Cannot alocate memory\n");
		exit(-1);
	}
	slist_data(p) = data; slist_next(p) = NULL;
	if(!slist_size(list))
		slist_head(list) = p;
	else
		slist_next(slist_tail(list)) = p;
	slist_tail(list) = p;
	slist_size(list)++; 
	return 0;
}

/** Prepend data to list (add as first node of the list)
	\param list - a pointer to list
	\param data - the data to place in the list
	\return 0 on success, or -1 on failure
*/
int slist_prepend(slist_t *list,void *data){
	if(list == NULL)
	{
		printf("parm List is NULL\n");
		return -1;
	}
	slist_node_t *p = (slist_node_t* )malloc(sizeof(slist_node_t));
	if(p == NULL)
	{
		printf("ERROR: Cannot alocate memory\n");
		exit(-1);
	}
	slist_data(p) = data; slist_next(p) = NULL;
	if(!slist_size(list))
		slist_tail(list) = p;
	else
		slist_next(p) = slist_head(list);
	slist_head(list) = p;
	slist_size(list)++; 
	return 0;
}

/** \brief Append elements from the second list to the first list, use the slist_append function.
	you can assume that the data of the lists were not allocated and thus should not be deallocated in destroy 
	(the destroy for these lists will use the SLIST_LEAVE_DATA flag)
	\param to a pointer to the destination list
	\param from a pointer to the source list
	\return 0 on success, or -1 on failure
*/
int slist_append_list(slist_t* to, slist_t* from){
	if(to == NULL || from == NULL)
	{
		printf("parm List ot or List to or both is NULL\n");
		return -1;
	}
	slist_node_t *p = slist_head(from);
	while(p != NULL)
	{
		if(slist_append(to , slist_data(p)) == -1)
			return -1;
		p = slist_next(p);
	}
	return 0;
}