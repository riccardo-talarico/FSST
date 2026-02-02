#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "heap.h"

#define MAGIC_HASH 2971215073
#define DEBUG 0
#define ESCAPE_CODE 255
#define SYMBOL_LEN(c) (((c) >> 12) & 0x0F)

#ifdef DEBUG
    #define debug_print(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
	#define debug_print_symbol(s, slen) do{\
		for(int _debug_i = 0; _debug_i < (slen); _debug_i++) {\
		   	if(isprint(s[_debug_i])) fprintf(stderr,"%c",(char)s[_debug_i]); \
			else fprintf(stderr,"%u",s[_debug_i]); \
		} \
	}while(0)
#else
    #define debug_print(fmt, ...) do {} while (0)
	#define debug_print_symbol(s, slen) do {} while(0)
#endif

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define IS_LITTLE_ENDIAN 0
    // Mask the most significant 3 bytes (0, 1, 2 in memory)
    #define THREE_BYTE(x) ((x) & 0xFFFFFF0000000000ULL)
#else
    #define IS_LITTLE_ENDIAN 1
    // Mask the least significant 3 bytes (0, 1, 2 in memory)
    #define THREE_BYTE(x) ((x) & 0x0000000000FFFFFFULL)
#endif

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

symbolTable *stInit(void);
uint64_t hash(uint64_t x);
void insertSymbol(symbolTable *st, symbolEntry e);
uint16_t findLongestSymbol(const symbolTable *st, byte *text);
void encodeScalar(uint8_t *cur, uint8_t *out, symbolTable *st);
void compressCount(const symbolTable *st, uint32_t *count1, uint32_t count2[][512], const char *text, const size_t text_len);
void updateTable(symbolTable *st, const uint32_t *count1, const uint32_t count2[][512]);
void resetHashTable(symbolTable *st);
char *add_padding(char *text, size_t len);
symbolTable *buildSymbolTableFromText(char *text);
