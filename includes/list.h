#if !defined(LIST)
#define LIST

#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

typedef struct _list{
	int fd;
	struct _list *next;
} list;

typedef struct _list2{
	int fd;
	fileT *fileopened;
	struct _list2 *next;
} listfiles;

int insert_node(int node, list **last, list **l){
	list *new = malloc(sizeof(list));
	if(!new){
		perror("malloc");
		return -1;
	}
	new->fd = node;
	new->next = NULL; 
	
	if(*last == NULL){
		*last = new;
		*l = new;
		return 0;
	}

	(*last)->next = new;
	return 0;
}

int remove_node(list **l, list **last){
	if(*l == NULL)
		return -1;
	list *tmp = *l;
	*l = (*l)->next;
	int ret = tmp->fd;
	if(*l == NULL)
		*last = NULL;
	free(tmp);
	return ret;
}

int insert_listfiles(int node, fileT **file, listfiles **l){
	listfiles *new = malloc(sizeof(listfiles));
	if(!new){
		perror("malloc");
		return -1;
	}
	new->fd = node;
	new->fileopened = *file;
	new->next = *l;
	*l = new;
	return 0;
}

fileT* remove_if_equal(int node, listfiles **l){
	listfiles *cur = *l;
	listfiles *prev = NULL;
	listfiles *tmp = NULL;
	fileT *ret = NULL;    

	while((cur != NULL) && (node != cur->fd)){
		prev = cur;
		cur = cur->next;

	}
	if(cur == NULL)
		return NULL;
	ret = cur->fileopened;
	if(prev != NULL)
		prev->next = cur->next;
	else
		*l = cur->next;
	free(cur);
	return ret;

}

fileT* remove_if_equalpath(char *path, listfiles **l){
	listfiles *cur = *l;
	listfiles *prev = NULL;
	listfiles *tmp = NULL;
	fileT *ret = NULL;    

	while((cur != NULL) && (path != cur->fileopened->key)){
		prev = cur;
		cur = cur->next;

	}
	if(cur == NULL)
		return NULL;
	ret = cur->fileopened;
	if(prev != NULL)
		prev->next = cur->next;
	else
		*l = cur->next;
	free(cur);
	return ret;

}

#endif
