#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "heap.h"
#include "hashFsst.h"

symbolTable *stInit(void){
	symbolTable *st = malloc(sizeof(*st));
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

uint64_t hash(uint64_t x){
	x = THREE_BYTE(x);
	return ((x*MAGIC_HASH)^(x>>15)) & (HASH_TABLE_SIZE-1);
}

void insertSymbol(symbolTable *st, symbolEntry e){
	st->hashTable[hash(e.num)] = e;

	debug_print_symbol(e.symbol, SYMBOL_LEN(e.code));
	debug_print("=symbol inserted in hash position %lu, and in non hash pos: %u\n",hash(e.num), st->nSymbols);

	st->entry[st->nSymbols++] = e;
	if(SYMBOL_LEN(e.code) < 3) st->shortCodes[e.num] = e.code;
}

uint16_t findLongestSymbol(const symbolTable *st, byte *text){
	//Assuming padded text
	uint64_t word = *(uint64_t*)text;
	uint64_t idx = hash(word);
	symbolEntry s = st->hashTable[idx];
    
	uint64_t mask = 0xFFFFFFFFFFFFFFFF >> (s.ignoredBits & 63);
	uint64_t num = word & mask; 

	debug_print_symbol(s.symbol, SYMBOL_LEN(s.code));
	debug_print("=symbol with ignored bits: %u, word: %lu, num: %lu, s.num: %lu, Idx: %lu\n",s.ignoredBits, word, num, s.num, idx); 
	
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

void compressCount(const symbolTable *st, uint32_t *count1, uint32_t count2[][512], const char *text, 
		const size_t text_len){
	size_t pos = 0;
	byte *t = (byte*)text;
	uint16_t code = findLongestSymbol(st,t);
	size_t symbolLen = (code==511)? 1 : SYMBOL_LEN(code); 
	//Just considering the first 8 bits
	code = code&0xFF;
	if(code==ESCAPE_CODE) code=t[pos];
	else code+=ESCAPE_CODE;
	count1[code]++;

	debug_print("Symbol len %lu, code found: %u, current pos: %lu\n", symbolLen, code, pos);

	uint16_t prev = code;
	while((pos+=symbolLen)<text_len){
		code = findLongestSymbol(st, t+pos);
		debug_print("Code found: %u, which is %i.Current prev: %u, current pos: %lu\n",code,code&0xFF,prev,pos);
		
		symbolLen = SYMBOL_LEN(code);
		code = code&0xFF;
		if(code == ESCAPE_CODE){
			byte next = t[pos];

			debug_print("Updating byte: %u, which is letter %c and concat %u, byte\n", next, (char)next, prev);

			count1[next]++;
			count2[prev][next]++;
			prev=next;
			symbolLen = 1;
		}
		else{
			// Symbol frequency count is stored from 255 on
			code += ESCAPE_CODE;
			debug_print("Updating frequency of concat prev,code: %u,%u, the second is the entry %u of the symbol table\n",prev, code,code-ESCAPE_CODE);
			
			// Count frequencies of symbols
			count1[code]++;
			// Count frequencies of concat(prev,code)
			count2[prev][code]++;
			prev=code;

		}	
		debug_print("Symbol len %lu\n", symbolLen);
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
			debug_print("------------------\nCANDIDATE SYMBOL: %u\n",i);
			c.symbol[0] = i;
		} else{
			uint32_t j = i- ESCAPE_CODE;
		   	c.len = SYMBOL_LEN(st->entry[j].code);

			debug_print("st->entry[%u].c.len=%u. CANDIDATE SYMBOL=",j,c.len);
			debug_print_symbol(st->entry[j].symbol, c.len);

			c.gain *= ((uint32_t)c.len);
			memcpy(c.symbol, st->entry[j].symbol,c.len);

			debug_print("\ni=%u, so actual position in st is %u.c.len=%u Candidate symbol:",i,j,c.len);
			debug_print_symbol(c.symbol, c.len);
			
		}
		hpush(h, c);
		byte remaining_len = 8-c.len;
		
		debug_print("\nRemaining len: %u\n", remaining_len);
		
		byte old_len = c.len;
		for(uint32_t k =0; k<ESCAPE_CODE+((uint32_t)st->nSymbols); k++){
			if(remaining_len == 0) break;
			uint32_t freq = count2[i][k];
			if(freq == 0) continue;
			
			if(k<ESCAPE_CODE){
				c.symbol[old_len] = (byte)k;
				c.len = 1 + old_len;

				debug_print("--------------\ni:%u, k:%u, c.len=%u CANDIDATE SYMBOL:\n",i,k,c.len);
                debug_print_symbol(c.symbol, c.len);
				
			}
			else{
				uint32_t j = k-ESCAPE_CODE;
				byte slen = SYMBOL_LEN(st->entry[j].code);
				
				byte copy_len = slen;
				if(remaining_len < slen){
					copy_len = remaining_len;
					//count the gain only if the actual symbol can be constructed
					freq = 0;
				}
				memcpy(&(c.symbol[old_len]), st->entry[j].symbol, copy_len);
				c.len = old_len + copy_len;

				debug_print("----------------\ni=%u,k=%u,c.len=%u ,CANDIDATE SYMBOL:",i,k,c.len);
				debug_print_symbol(c.symbol, c.len);
				
			}
			c.gain = c.len * freq;

			debug_print("\nInserting symbol: ");
			debug_print_symbol(c.symbol, c.len);

			hpush(h, c);
		}
	}
	st->nSymbols = 0;
	while(st->nSymbols < 255 && h->size > 0){
		candidate min = hgetmin(h);

		debug_print("candidate min popped from heap has len %u and symbol=",min.len);
		debug_print_symbol(min.symbol, min.len);
		debug_print("\nHeap has size: %lu\n",h->size);
		
		symbolEntry e={0};
		e.code = (uint16_t)st->nSymbols + ((uint16_t)min.len << 12);

		debug_print("Before inserting in static table: e.code=%u, st->nSymbols=%u\n",e.code, st->nSymbols);

		e.ignoredBits = (8-min.len)*8;
		memcpy(e.symbol, min.symbol, min.len);
		insertSymbol(st,e);
	}
	free(h);
}
	
// To avoid conflicts with previous hashTable
void resetHashTable(symbolTable *st){
	symbolEntry s = {511, {0},0};
	for(int i = 0; i < HASH_TABLE_SIZE; i++){
		st->hashTable[i] = s;
	}
	for(int i = 0; i < SHORT_CODES_SIZE; i++){
		st->shortCodes[i] = 511;
	}
}

char *add_padding(char *text, size_t len){
	char *t = malloc(len+8);
	memcpy(t, text, len);
	for(size_t i = len; i < len+8; i++){
		t[i] = 0;
	}
	return t;
}

symbolTable *buildSymbolTableFromText(char *text){
	symbolTable *st = stInit();
	size_t text_len = strlen(text);
	char *t = add_padding(text,text_len);
	for(uint8_t i=0; i < 5; i++){
		uint32_t count1[512] = {0};
		uint32_t count2[512][512] = {0};
		compressCount(st, count1, count2, text, text_len);
		resetHashTable(st);
		updateTable(st, count1, count2);
	}
	free(t);
	return st;
}

