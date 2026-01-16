#include "heap.h"
#include <stdlib.h>
#include <assert.h>


heap *hinit(void){
	heap *h = malloc(sizeof(*h));
	h->entry = malloc(sizeof(candidate)*MAX_HEAP_SIZE);
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
		heapify(h, i);
	}
}	

void hpush(heap *h, candidate *cand){
	size_t idx = h->size++;
	h->entry[idx] = *cand;

	while(idx!=0 && h->entry[PARENT(idx)].gain > h->entry[idx].gain){
		size_t parent = PARENT(idx);
		hswap(h, parent, idx);
		idx = parent;
	}
}


char *hgetmin(heap *h){
	char *symbol = h->entry[0].symbol;
	
	(h->size)--;
	
	h->entry[0] = h->entry[h->size];
	heapify(h, 0);
	
	return symbol;
}


int main(void){
	heap *h = hinit();
	candidate c1 = {1,"ab"};
	candidate c2 = {10, "hello"};
	candidate c3 = {5, "yes"};
	candidate c4 = {2, "no"};
	
	hpush(h, &c1);
	hpush(h, &c2);
	hpush(h, &c3);
	hpush(h, &c4);

	char *s = hgetmin(h);
	printf("Min is %s and in the heap there is %s at position 0\n", s, h->entry[0].symbol);
	char *s1 = hgetmin(h);
	printf("New min is %s and hsize is %lu\n",s1,h->size);
	char *s2 = hgetmin(h);
	char *s3 = hgetmin(h);
	printf("Other strings in order: %s %s\n", s2, s3);
	free(h);
	return 0;
}

