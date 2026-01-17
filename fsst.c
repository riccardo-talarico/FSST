#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "heap.h"

#define DEBUG 0

typedef uint8_t byte;

typedef struct symbolEntry{
	// can range from 1 to 8 byte
	char *symbol;
	//max length in byte is 8
	byte len;
	byte code;
}symbolEntry;


/* Symbol Table data structure. It contains an array of 255 symbols plus a special entry for the escape byte
 * The symbols are stored in lexicographical order but when one string prefixes the other, the longer is first.
 * The firsIdx array stores for every byte the code of the longest symbol that start with that byte
 */
typedef struct symbolTable{
	symbolEntry entry[256];
	byte nSymbols;
	byte firstIdx[257]; 
}symbolTable;

symbolTable *stInit(void){
	symbolTable *st = malloc(sizeof(*st));
	for(int i = 0; i < 256; i++){
		st->entry[i].symbol = NULL;
	}
	for(int i = 0; i < 257; i++){
		st->firstIdx[i] = 0;
	}
	st->nSymbols = 0;
	return st;
}

void insertSymbol(symbolTable *st, symbolEntry e){
	st->entry[st->nSymbols++] = e;
}

byte findLongestSymbol(symbolTable *st, byte *text){
	byte first = text[0];

	byte startCode = st->firstIdx[first];
	byte endCode = st->firstIdx[first+1];
	for(byte code=startCode; code<endCode; code++){
		char *symbol = st->entry[code].symbol;
		byte len = st->entry[code].len;
		if(memcmp(text, symbol, len)==0) return code;
	}
	// Return the escape byte
	return 255;
}


void encode(symbolTable *st, byte **in, byte **out){
	byte code = findLongestSymbol(st, *in);
#if DEBUG
	printf("Code found: %u\n",code);
#endif
	if(code == 255){
		// Adding escape sequence
		*((*out)++) = 255;	
		// Copying the byte
		*((*out)++) = *((*in)++);
	}
	else{
		*((*out)++) = code;
		*in += st->entry[code].len;
	}
}	


void decode(symbolTable *st, byte **in, byte **out){
	byte code = *((*in)++);
	if(code != 255){
		// No escape code so it substitutes the code with the corresponding symbol
		*((uint64_t*)(*out)) = (uint64_t)(*(st->entry[code].symbol));
		// Increases the pointer by the length of the symbol
		*out += st->entry[code].len;
	}
	else{
		// Copying the byte
		*((*out)++) = *((*in)++);
	}
}


void compressCount(symbolTable *st, uint32_t *count1, uint32_t count2[][512], char *text, size_t text_len){
	size_t pos = 0;
	byte *t = (byte*)text;
	byte code = findLongestSymbol(st,t);

	size_t symbolLen = (code==255)? 1 : st->entry[code].len; 
	if(code==255) code=t[pos];
	else code+=255;
	count1[code]++;
#if DEBUG
	printf("Symbol len %lu\n", symbolLen);
#endif
	byte prev = code;
	while((pos+=symbolLen)<text_len){
		code = findLongestSymbol(st, t+pos);
#if DEBUG
		printf("Code found: %u, current prev: %u, current pos: %lu\n", code, prev, pos);
#endif
		if(code == 255){
			byte next = t[pos];
#if DEBUG
			printf("Byte: %u\n", next);
#endif
			count1[next]++;
			count2[prev][next]++;
			prev=next;
		}
		else{
			// Symbol frequency count is stored from 255 on
			code += 255;
			// Count frequencies of symbols
			count1[code]++;
			// Count frequencies of concat(prev,code)
			count2[prev][code]++;
			prev=code;
		}	
		symbolLen = (code==255)? 1 : st->entry[code].len; 
	}
}

void makeIndex(symbolTable *st){
	
}


void updateTable(symbolTable *st, uint32_t *count1, uint32_t count2[][512]){
	heap *h = hinit();
	for(uint32_t i = 0; i < 256+((uint32_t)st->nSymbols); i++){
		uint32_t gain=count1[i];
		if(gain == 0) continue;
		candidate c;
		c.gain = gain;
		if(i<256){

			c.len = 1;
			memset(c.symbol, 0,8);
			c.symbol[0] = i;
		} else{
			uint32_t j = i- 256;
		   	c.len = st->entry[j].len;
			c.gain *= ((uint32_t)c.len);
			memcpy(c.symbol, st->entry[j].symbol,c.len);
		}
		hpush(h, c);
		byte remaining_len = 8-c.len;
		for(uint32_t k =0; k<256+((uint32_t)st->nSymbols); k++){
			if(remaining_len == 0) break;
			uint32_t freq = count2[i][k];
			if(freq == 0) continue;
			
			candidate c2;

			if(k<256){
				memcpy(&(c2.symbol[c.len]), &k, 1);
				c2.len = 1 + c.len;
			}
			else{
				uint32_t j = k-256;
				byte copy_len = (remaining_len>st->entry[j].len)? st->entry[j].len : remaining_len;
				memcpy(&(c2.symbol[c.len]), &st->entry[j], copy_len);
				c2.len = c.len + copy_len;
			}
			gain = c2.len * freq;
			hpush(h, c2);
		}
	}
	st->nSymbols = 0;
	while(st->nSymbols < 255 && h->size > 0){

		candidate c = hgetmin(h);
		
		symbolEntry e={0};
		e.len = c.len;
		e.code = st->nSymbols;
		memcpy(&e.symbol, &c.symbol, e.len);	
		insertSymbol(st,e);
	}
	free(h);
	makeIndex(st);
}


/*
symbolTable *buildSymbolTableFromText(char *text){
	symbolTable *st = stInit();
	for(uint8_t i=0; i < 5; i++){
		//count1
		//count2
		compressCount(st, count1, count2, text);
		st = makeTable(st, count1, count2);
	}
}*/

int main(void){
	symbolTable *st = stInit();
	// Repetition of tum, len=13
	char *text = "tumcwitumvldb";
	
	byte *p = (byte*)text;
	byte *outbuf = malloc(strlen(text)*2 + 1);
	printf("Size of outbuf:%lu\n",strlen(text)*2+1);
	byte *outp = outbuf;
	
	while(p[0]!=0){
		encode(st, &p, &outp);	
	}
	size_t len = (outp-outbuf);
	// Adding null terminatation byte
	outbuf[len] = 0;
	printf("Successfully Encoded\n");
	for(size_t i = 0; i < len; i++){
		if(outbuf[i] !=255){
			unsigned char c = (outbuf[i]);
			if(isprint(c)) printf("%c ",c);
			else printf("Unprintable, value %u\n",c);
		}
		else printf("Escape-");
	}
	printf("\n");
	
	byte *decoded = malloc(sizeof(*text)+1);
	p = decoded;
	byte *encoded = outbuf;
	while(outp != encoded){
		decode(st, &encoded, &p); 
	}
	printf("Successfully Decoded\n");
	

	len = p-decoded;
	decoded[len]=0;
	for(size_t i = 0; i<len; i++){
		unsigned char c = decoded[i];
		if(isprint(c)) printf("%c",c);
		else printf("Unprintable, value %u\n",c);
	}
	printf("\n");
	uint32_t count1[512]={0};
	uint32_t count2[512][512]={0};
	size_t text_len = strlen(text);
	compressCount(st, count1, count2, text, text_len);

	for(unsigned int i = 0; i < 512; i++){
		uint32_t freq = count1[i];
		if(freq > 0){
			byte c = (i>=255)? (byte)(i-255) : (byte)i;
			if(isprint(c)) printf("Char %c has %u occurrencies\n",c,freq);
			else printf("Unprintable value %u has %u occurrencies\n",c,freq);
		}
	}

	for(unsigned int i = 0; i < 512; i++){
		for(unsigned int j = 0; j < 512; j++){
			uint32_t freq = count2[i][j];
			if(freq>0){
				byte c1 = (i>=255)? (byte)(i-255) : (byte)i;
				byte c2 = (j>=255)? (byte)(j-255) : (byte)j;
				if(isprint(c1) && isprint(c2)) printf("Concat (%c, %c) has frequency: %u\n", c1,c2,freq);
				else printf("Concat byte (%u,%u) has frequency: %u\n", c1,c2,freq);
			}
		}
	}

	updateTable(st,count1,count2);
	printf("Table successfully updated\n");
	for(byte i = 0; i < st->nSymbols; i++){
		printf("Entry %u has len %u\n", i, st->entry[i].len);
	}

	free(st);
	free(decoded);
	free(outbuf);
	return 0;
}

