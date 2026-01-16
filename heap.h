#pragma once

#include <stdio.h>
#include <stdint.h>


#define MAX_HEAP_SIZE 255
#define PARENT(i) (i-1)/2
#define LEFT(i) (2*i+1)
#define RIGHT(i) (2*i+1)

typedef struct{
	uint32_t gain;
	char *symbol;
}candidate;

typedef struct{
	size_t size;
	candidate *entry;
}heap;


size_t hparent(size_t i);
size_t hleft(size_t i);
size_t hright(size_t i);

void hswap(heap *h, size_t i, size_t j);
void heapify(heap *h, size_t i);

char *hgetmin(heap *h);
