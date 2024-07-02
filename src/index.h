#ifndef INDEX_INDEX_H
#define INDEX_INDEX_H
#include "hashmap.h"
#include "merge.h"

typedef struct posting{
    unsigned int doc_ID;
    unsigned int freq;
}posting;

typedef struct compPosting{
    unsigned char * doc_ID;
    unsigned char * freq;
}compPosting;

typedef struct id_Url{
    unsigned int id;
    unsigned int total_term;
    off_t page_offset;
    char url[128];
} id_Url;

typedef struct entry{
    unsigned int posting_length;
    unsigned int posting_capacity;
    char* term;
    posting * posting;
} entry;

typedef struct lexicon{
    char* term;
    off_t offset;
}lexicon;

typedef struct Word_id{
    unsigned int term_id;
}word_id;

typedef struct term_freq{
    char * term;
    unsigned int freq;
} term_freq;

int equal(const char* str1, const char* str2);

int compare_entry_by_postingLength(const void* a, const void* b);
int compare_entry_by_term(const void* a, const void* b);

uint64_t term_hash(const void *item, uint64_t seed0, uint64_t seed1);

int term_compare(const void *a, const void *b, void *udata);

void sort_entries(entry *entries, unsigned int nElement);

void writeEntryToFile(FILE* file, const entry* entry, struct hashmap *lexicon_map);
void writePageTableToFile(FILE *file, const id_Url* pages);
void writeLexiconToFile(FILE *file, const lexicon *lc);

int isASCII(const char*str);


void buildIndex(struct hashmap* term_freq_map, entry *entries, struct hashmap *lexicon_map);
void splitStringByWhiteSpace(char* input, unsigned int *total_term, struct hashmap *term_freq_map);

void processDoc(FILE* file, id_Url* pages, entry *entries, struct hashmap* lexicon_map, unsigned int index_size, unsigned int max_page_table);

//int dump_entries(entry* entries, unsigned int nElement);

#endif //INDEX_INDEX_H
