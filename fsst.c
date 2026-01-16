#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define DEBUG 1

typedef uint8_t byte;

typedef struct symbolEntry{
	// can range from 1 to 8 byte
	char *symbol;
	uint8_t code;
	//max length in byte is 8
	byte len;
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
	byte pos = findLongestSymbol(st, *in);
#if DEBUG
	printf("Position found: %u\n",pos);
#endif
	if(pos == 255){
		// Adding escape sequence
		*((*out)++) = 255;	
		// Copying the byte
		*((*out)++) = *((*in)++);
	}
	else{
		*((*out)++) = pos;
		*in += st->entry[pos].len;
	}
}	


void decode(symbolTable *st, byte *in, byte *out){
	byte code = *in++;
	if(code != 255){
		*((uint64_t*)out) = (uint64_t)(*(st->entry[code].symbol));
		out += st->entry[code].len;
	}
	else{
		*out++=*in++;
	}
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
	char *text = "Hello World";
	
	byte *p = (byte*)text;
	byte *outbuf = malloc(sizeof(*text)*2 + 1);
	byte *outp = outbuf;
	size_t len = 0;
	while(p[0]!=0){
		printf("Pointer %p\n", p);
		encode(st, &p, &outp);
		len+=2;	
	}
	// Adding null terminated byte
	outbuf[len] = 0;
	printf("Successfully Encoded\n");
	for(size_t i = 0; i < len; i++){
		if(outbuf[i] !=255){
			unsigned char c = (outbuf[i]);
			if(isprint(c)) printf("%c\n",c);
			else printf("Unprintable, value %u\n",c);
		}
		else printf("Escape-");
	}
	free(st);

	free(outbuf);
	return 0;
}

