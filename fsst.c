#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "heap.h"

#define DEBUG 1
#define ESCAPE_CODE 255


typedef uint8_t byte;

typedef struct symbolEntry{
	// can range from 1 to 8 byte
	byte symbol[8];
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
	uint16_t firstIdx[257]; 
}symbolTable;

symbolTable *stInit(void){
	symbolTable *st = malloc(sizeof(*st));
	for(int i = 0; i < 257; i++){
		st->firstIdx[i] = 256;
	}
	st->nSymbols = 0;
	return st;
}

void insertSymbol(symbolTable *st, symbolEntry e){
	st->entry[st->nSymbols++] = e;
}

byte findLongestSymbol(const symbolTable *st, byte *text){
	byte first = text[0];

	uint16_t startCode = st->firstIdx[first];
	uint16_t endCode = st->firstIdx[first+1];
	//FIXME: I'm not sure this is correct!
	if(endCode = 256 && startCode != 256) endCode = startCode+1;
	for(byte code=startCode; code<endCode; code++){
		byte len = st->entry[code].len;
		if(memcmp(text, st->entry[code].symbol, len)==0) return code;
	}
	// Return the escape byte
	return ESCAPE_CODE;
}


void encode(const symbolTable *st, byte **in, byte **out){
	byte code = findLongestSymbol(st, *in);
#if DEBUG
	printf("Code found: %u\n",code);
#endif
	if(code == ESCAPE_CODE){
		// Adding escape sequence
		*((*out)++) = ESCAPE_CODE;	
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
	if(code != ESCAPE_CODE){
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


void compressCount(const symbolTable *st, uint32_t *count1, uint32_t count2[][512], const char *text, 
		const size_t text_len){
	size_t pos = 0;
	byte *t = (byte*)text;
	uint16_t code =(uint16_t)findLongestSymbol(st,t);

	size_t symbolLen = (code==ESCAPE_CODE)? 1 : st->entry[code].len; 
	if(code==ESCAPE_CODE) code=t[pos];
	else code+=ESCAPE_CODE;
	count1[code]++;
#if DEBUG
	printf("Symbol len %lu, code found: %u, current pos: %lu\n", symbolLen, code, pos);
#endif
	uint16_t prev = code;
	while((pos+=symbolLen)<text_len){
		code = findLongestSymbol(st, t+pos);
#if DEBUG
		printf("Code found: %u, current prev: %u, current pos: %lu\n", code, prev, pos);
#endif
		if(code == ESCAPE_CODE){
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
			code += ESCAPE_CODE;
#if DEBUG
			printf("Updating frequency of concat prev,code: %u,%u, the second is the entry %u of the symbol table\n",prev, code,code-ESCAPE_CODE);
#endif
			// Count frequencies of symbols
			count1[code]++;
			// Count frequencies of concat(prev,code)
			count2[prev][code]++;
			prev=code;

		}	
		symbolLen = (code==ESCAPE_CODE)? 1 : st->entry[code-ESCAPE_CODE].len; 
#if DEBUG
		printf("Symbol len %lu\n", symbolLen);
#endif
	}
}

static int cmpsymbol(const void *p1, const void *p2){
	const symbolEntry *se1 = (const symbolEntry *)p1;
	const symbolEntry *se2 = (const symbolEntry *)p2;
	
	byte len = (se1->len < se2->len)? se1->len : se2->len;
	int i = memcmp(se1->symbol, se2->symbol, len);	

	if(i!=0) return i;
	else return (int)se2->len - (int)se1->len;
}

void makeIndex(symbolTable *st){
	qsort(st->entry,(size_t)st->nSymbols,sizeof(symbolEntry), cmpsymbol);
	for(byte i=st->nSymbols; i != 0; i--){
		byte letter = (st->entry[i-1].symbol)[0];
		st->firstIdx[letter] = i-1;
	}
}


void updateTable(symbolTable *st, const uint32_t *count1, const uint32_t count2[][512]){
	heap *h = hinit();
	for(uint32_t i = 0; i < ESCAPE_CODE+((uint32_t)st->nSymbols); i++){
		uint32_t gain=count1[i];
		if(gain == 0) continue;
		candidate c;
		c.gain = gain;
		if(i<ESCAPE_CODE){
			c.len = 1;
			memset(c.symbol, 0,8);
			c.symbol[0] = i;
		} else{
			uint32_t j = i- ESCAPE_CODE;
		   	c.len = st->entry[j].len;
#if DEBUG
			printf("st->entry[%u].symbol=",j);
			for(byte in = 0; in<st->entry[j].len; in++){
				printf("%c", st->entry[j].symbol[in]);
			}
			printf("\n");
#endif
			c.gain *= ((uint32_t)c.len);
			memcpy(c.symbol, st->entry[j].symbol,c.len);
#if DEBUG
			printf("i=%u, so actual position in st is %u. Candidate symbol:",i,j);
			for(byte in = 0; in < c.len; in++){
				printf("%c",c.symbol[in]);
			}
			printf("\n");
#endif
		}
		hpush(h, c);
		byte remaining_len = 8-c.len;
		printf("Remaining len: %u\n", remaining_len);
		byte old_len = c.len;
		for(uint32_t k =0; k<ESCAPE_CODE+((uint32_t)st->nSymbols); k++){
			if(remaining_len == 0) break;
			uint32_t freq = count2[i][k];
			if(freq == 0) continue;
			
			if(k<ESCAPE_CODE){
				memcpy(&(c.symbol[old_len]), &k, 1);
				c.len = 1 + old_len;
#if DEBUG
				printf("i:%u, k:%u Candidate concatenation, k no symbol:\n",i,k);
                for(byte testi=0; testi<c.len; testi++){
                    if(isprint(c.symbol[testi])) printf("%c",(char)c.symbol[testi]);
                    else printf("%u", c.symbol[testi]);
                }
				printf("\n");
#endif
			}
			else{
				uint32_t j = k-ESCAPE_CODE;
				byte copy_len = (remaining_len>st->entry[j].len)? st->entry[j].len : remaining_len;
				memcpy(&(c.symbol[old_len]), st->entry[j].symbol, copy_len);
				c.len = old_len + copy_len;
#if DEBUG
				printf("i=%u,k=%u,c.len=%u ,Candidate concatenation, both symbols:",i,k,c.len);
				for(byte testi=0; testi<c.len; testi++){
					if(isprint(c.symbol[testi])) printf("%c",(char)c.symbol[testi]);
					else printf("%u", c.symbol[testi]);
				}
				printf("\n");
#endif
				
			}
			c.gain = c.len * freq;
			hpush(h, c);
		}
	}
	st->nSymbols = 0;
	while(st->nSymbols < ESCAPE_CODE && h->size > 0){

		candidate c = hgetmin(h);
#if DEBUG
		printf("candidate c popped from heap has len %u and symbol %s\n",c.len, c.symbol);
#endif	
		symbolEntry e={0};
		e.len = c.len;
		e.code = st->nSymbols;
		memcpy(e.symbol, c.symbol, c.len);
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
		if(outbuf[i] !=ESCAPE_CODE){
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

	updateTable(st,count1,count2);

	
	uint32_t count11[512]={0};
	uint32_t count22[512][512]={0};
	compressCount(st, count11, count22, text, text_len);

	updateTable(st, count11, count22);
	printf("Table successfully updated twice!\n");
	for(byte i = 0; i < st->nSymbols; i++){
		printf("Entry %u, symbol=%s has len %u\n", i, st->entry[i].symbol,st->entry[i].len);
	}

	free(st);
	free(decoded);
	free(outbuf);
	return 0;
}

