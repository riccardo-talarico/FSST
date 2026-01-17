#ifndef HEAP_H
#define HEAP_H

#include <stdio.h>
#include <stdint.h>


#define MAX_HEAP_SIZE 256
#define PARENT(i) (i-1)/2
#define LEFT(i) (2*i+1)
#define RIGHT(i) (2*i+1)

typedef struct{
	uint8_t symbol[8];
	uint32_t gain;
	unsigned char len;
}candidate;

typedef struct{
	size_t size;
	candidate *entry;
}heap;

heap *hinit(void);
void hswap(heap *h, size_t i, size_t j);
void heapify(heap *h, size_t i);
void hpush(heap *h, candidate cand); 
candidate hgetmin(heap *h);


#endif
