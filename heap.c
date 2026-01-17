#include "heap.h"
#include <stdlib.h>
#include <assert.h>


heap *hinit(void){
	heap *h = malloc(sizeof(*h));
	h->entry = malloc(sizeof(candidate)*MAX_HEAP_SIZE);
	h->size = 0;
	return h;
}

void hswap(heap *h, size_t i, size_t j){
	candidate tmp = h->entry[i];
	h->entry[i] = h->entry[j];
	h->entry[j] = tmp;
}

void heapify(heap *h, size_t i){
	size_t left = LEFT(i);
	size_t right = RIGHT(i);
	size_t smallest = i;

	if(left < MAX_HEAP_SIZE && h->entry[left].gain < h->entry[i].gain) smallest = left;
	if(right < MAX_HEAP_SIZE && h->entry[right].gain < h->entry[smallest].gain) smallest = right;

	if(smallest != i){
		hswap(h, i, smallest);
		heapify(h, smallest);
	}
}	

void hpush(heap *h, candidate *cand){
	size_t idx = h->size;
	h->size++;
	h->entry[idx] = *cand;

	while(idx!=0 && h->entry[PARENT(idx)].gain > h->entry[idx].gain){
		size_t parent = PARENT(idx);
		hswap(h, parent, idx);
		idx = parent;
	}
}


candidate hgetmin(heap *h){
	candidate top = h->entry[0];
	
	(h->size)--;
	
	h->entry[0] = h->entry[h->size];
	heapify(h, 0);
	
	return top;
}




