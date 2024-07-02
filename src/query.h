//
// Created by linwe on 2023/11/13.
//
#ifndef INDEX_QUERY_H
#define INDEX_QUERY_H
#include <stdio.h>
#include <stdlib.h>
#include "index.h"
#include "hashmap.h"
typedef struct id_bm25{
    unsigned int id;
    double bm25;
}id_bm25;

int page_id_compare(const void*a, const void*b, void *udata);
uint64_t page_id_hash(const void *item, uint64_t seed0, uint64_t seed1);
void readEntryFromFile(FILE* file, entry* entry, long offset);
void readLexiconFromFile(FILE *file, lexicon *lc);
id_Url* readPageTableFromFile(FILE *file,unsigned int *maxdocID);

int compare_bm25(const void* a, const void* b);

struct entry* openList(char* term, struct hashmap *lexicon_map, FILE *entries_file);
unsigned int nextGEQ(entry* list, unsigned int k, unsigned int maxdocID);

void processTop10(unsigned int top10_size,id_bm25 *top10,struct hashmap *page_map);

double getScore(entry* lp,unsigned int documentID, unsigned int maxdocID, double average_doc_length, struct hashmap *page_map);
struct posting* getPosting(entry* lp, unsigned int docID);
int splitQuery(const char* query, char* terms[]);

#endif //INDEX_QUERY_H
