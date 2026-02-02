#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "heap.h"

#define DEBUG 1
#define ESCAPE_CODE 255


typedef uint8_t byte;

typedef struct symbolEntry{	
	uint16_t code; //bits [0..8]=code, [12..15]=len, unused=511 because it can be casted to 255 easily
	// can range from 1 to 8 byte
	union{
		byte symbol[8];
		uint64_t num;
	};
	byte ignoredBits;
}symbolEntry;


/* Symbol Table data structure. It contains an array of 255 symbols plus a special entry for the escape byte
 * The symbols are stored in lexicographical order but when one string prefixes the other, the longer is first.
 * The firsIdx array stores for every byte the code of the longest symbol that start with that byte
 */
#define HASH_TABLE_SIZE 4096 //fits in L1 cache
#define SHORT_CODES_SIZE 256*256
typedef struct symbolTable{
	symbolEntry hashTable[HASH_TABLE_SIZE];
	symbolEntry entry[255];
	uint16_t shortCodes[SHORT_CODES_SIZE];
	byte nSymbols;
}symbolTable;

symbolTable *stInit(void){
	symbolTable *st = malloc(sizeof(*st));
	//FIXME: not sure about this initialization
	symbolEntry s = {511, {0},0};
	for(int i = 0; i < HASH_TABLE_SIZE; i++){
		st->hashTable[i] = s;
	}
	for(int i = 0; i < SHORT_CODES_SIZE; i++){
		st->shortCodes[i] = 511;
	}
	st->nSymbols = 0;
	return st;
}

#define MAGIC_HASH 2971215073
//FF is a byte, 6F means taking the first 3 bits
#define THREE_BYTE(x) x & 0xFFFFFF
uint64_t hash(uint64_t x){
	x = THREE_BYTE(x);
	return ((x*MAGIC_HASH)^(x>>15)) & (HASH_TABLE_SIZE-1);
}

#define SYMBOL_LEN(c) (((c) >> 12) & 0x0F)
void insertSymbol(symbolTable *st, symbolEntry e){
	st->hashTable[hash(e.num)] = e;
#if DEBUG
	printf("Inserting symbol %s in hash position %lu, and in non hash-pos: %u\n",e.symbol,hash(e.num), st->nSymbols);
#endif
	st->entry[st->nSymbols++] = e;
	if(SYMBOL_LEN(e.code) < 3) st->shortCodes[e.num] = e.code;
#if DEBUG 
	if(SYMBOL_LEN(e.code) < 3) printf("Inserting into short codes\n");
#endif 
}

uint16_t findLongestSymbol(const symbolTable *st, byte *text){
	//Assuming padded text
	uint64_t word = *(uint64_t*)text;
	uint64_t idx = hash(word);
#if DEBUG
	printf("Retrieving from hash table at index %lu\n",idx);
#endif
	symbolEntry s = st->hashTable[idx];
    
	uint64_t mask = 0xFFFFFFFFFFFFFFFF >> (s.ignoredBits & 63);
	uint64_t num = word & mask; 
#if DEBUG
	printf("Ignored bits: %u, word: %lu, num: %lu, s.num: %lu. Symbol. %s\n",s.ignoredBits, word, num, s.num, s.symbol); 
	if(s.num==num) printf("----------Code retrieved from hash table actually matches---------\n");
#endif
	return ((s.num==num) && (s.code != 511))? s.code : st->shortCodes[word&0xFFFF]; 
}

void encodeScalar(uint8_t *cur, uint8_t *out, symbolTable *st){
	uint64_t word = *(uint64_t*)cur;

	// Speculatively write 1st byte
	out[1] = (uint8_t) word;

	// Look-up in lossy hash table
	uint64_t idx = hash(word);
	symbolEntry s = st->hashTable[idx];
	// If look up is successful then the word should be equal (in bytes) to the symbol if you consider the same len
	uint64_t num = word & (0xFFFFFFFFFFFFFFFF >> s.ignoredBits);

	// if look up is successful write code, otherwise check in shortCodes
	uint16_t code = ((s.num == num) & (s.code != 511))? s.code : st->shortCodes[word&0xFFFF];
	out[0] = (uint8_t)code; //write out code, note that (uint8_t) 511=255
	
	// if code is not 511 then 9th bit is 1? 
	out += 2-((code>>8)&1); // increase with 1 or 2 (escape=9th bit)
	cur += SYMBOL_LEN(code); // symbol length is in bits [12..15] of code 
}

#define ESCAPE_CODE 255
void compressCount(const symbolTable *st, uint32_t *count1, uint32_t count2[][512], const char *text, 
		const size_t text_len){
	size_t pos = 0;
	byte *t = (byte*)text;
	uint16_t code = findLongestSymbol(st,t);
	size_t symbolLen = (code==511)? 1 : SYMBOL_LEN(code); 
#if DEBUG
	printf("Original symbol found: %u\n", code);
#endif
	//Just considering the first 8 bits
	code = code&0xFF;
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
		
		symbolLen = SYMBOL_LEN(code);
		code = code&0xFF;
		if(code == ESCAPE_CODE){
			byte next = t[pos];
#if DEBUG
			printf("Updating byte: %u, which is letter %c and concat %u, byte\n", next, (char)next, prev);
#endif
			count1[next]++;
			count2[prev][next]++;
			prev=next;
			symbolLen = 1;
		}
		else{
			// Symbol frequency count is stored from 255 on
			code += ESCAPE_CODE;
#if DEBUG
			printf("Updating frequency of concat prev,code: %u,%u, the second is the entry %u of the symbol table\n",prev, code,code-ESCAPE_CODE);
			printf("st->entry[%u]=",code-ESCAPE_CODE);
			for(byte t = 0; t < symbolLen;t++){
				printf("%c",st->entry[code-ESCAPE_CODE].symbol[t]);
			}
			printf("\n");
#endif
			// Count frequencies of symbols
			count1[code]++;
			// Count frequencies of concat(prev,code)
			count2[prev][code]++;
			prev=code;

		}	
#if DEBUG
		printf("Symbol len %lu\n", symbolLen);
#endif
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
#if DEBUG
			printf("------------------\nCANDIDATE SYMBOL: %u\n",i);
#endif
			c.symbol[0] = i;
		} else{
			uint32_t j = i- ESCAPE_CODE;
		   	c.len = SYMBOL_LEN(st->entry[j].code);
#if DEBUG
			printf("-----------------\n");
			printf("st->entry[%u].c.len=%u. CANDIDATE SYMBOL=",j,c.len);
			for(byte in = 0; in<c.len; in++){
				printf("%c", st->entry[j].symbol[in]);
			}
			printf("\n");
#endif
			c.gain *= ((uint32_t)c.len);
			memcpy(c.symbol, st->entry[j].symbol,c.len);
#if DEBUG
			printf("----------------\ni=%u, so actual position in st is %u.c.len=%u Candidate symbol:",i,j,c.len);
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
				c.symbol[old_len] = (byte)k;
				c.len = 1 + old_len;
#if DEBUG
				printf("--------------\ni:%u, k:%u, c.len=%u CANDIDATE SYMBOL:\n",i,k,c.len);
                for(byte testi=0; testi<c.len; testi++){
                    if(isprint(c.symbol[testi])) printf("%c",(char)c.symbol[testi]);
                    else printf("%u", c.symbol[testi]);
                }
				printf("\n");
#endif
			}
			else{
				uint32_t j = k-ESCAPE_CODE;
				byte slen = SYMBOL_LEN(st->entry[j].code);
				//FIXME: some implementations actually count the gain only if slen>remaining_len! Check if
				// it needs to be fixed.
				byte copy_len = (remaining_len>slen)? slen : remaining_len;
				memcpy(&(c.symbol[old_len]), st->entry[j].symbol, copy_len);
				c.len = old_len + copy_len;
#if DEBUG
				printf("----------------\ni=%u,k=%u,c.len=%u ,CANDIDATE SYMBOL:",i,k,c.len);
				for(byte testi=0; testi<c.len; testi++){
					if(isprint(c.symbol[testi])) printf("%c",(char)c.symbol[testi]);
					else printf("%u", c.symbol[testi]);
				}
				printf("\n");

#endif
#if DEBUG
				printf("----------------\ni=%u, so actual position in st is %u.c.len=%u Candidate symbol:",i,j,c.len);
				for(byte in = 0; in < c.len; in++){
					printf("%c",c.symbol[in]);
				}
				printf("\n");
#endif
				
			}
			c.gain = c.len * freq;
#if DEBUG
			printf("Inserting symbol: ");
			for(byte in = 0; in < c.len; in++){
				printf("%c", c.symbol[in]);
			}
			printf("\n");
#endif
			hpush(h, c);
		}
	}
	st->nSymbols = 0;
	while(st->nSymbols < 255 && h->size > 0){
		candidate min = hgetmin(h);
#if DEBUG
		printf("candidate min popped from heap has len %u and symbol:\n",min.len);
		for(byte pi = 0; pi < min.len; pi++){
			printf("%c",min.symbol[pi]);
		}
		printf("\nHeap has size: %lu\n",h->size);
#endif	
		symbolEntry e={0};
		e.code = (uint16_t)st->nSymbols + ((uint16_t)min.len << 12);
#if DEBUG
		printf("Before inserting in static table: e.code=%u, st->nSymbols=%u\n",e.code, st->nSymbols);
#endif
		e.ignoredBits = (8-min.len)*8;
		memcpy(e.symbol, min.symbol, min.len);
		insertSymbol(st,e);
	}
	free(h);
}
	
symbolTable *buildSymbolTableFromText(char *text){
	symbolTable *st = stInit();
	size_t text_len = strlen(text);
	for(uint8_t i=0; i < 5; i++){
		uint32_t count1[512] = {0};
		uint32_t count2[512][512] = {0};
		compressCount(st, count1, count2, text, text_len);
		updateTable(st, count1, count2);
	}
	return st;
}


int main(void){
	
	char *text = "tumwitumcvldb";

	symbolTable *st = buildSymbolTableFromText(text);
	for(int i = 0; i < st->nSymbols; i++){
		if(st->entry[i].code != 0){
			printf("Symbol: %s in entry %d\n", st->entry[i].symbol,i);
		}
	}
	free(st);
	return 0;
}
