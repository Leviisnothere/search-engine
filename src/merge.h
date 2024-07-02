#ifndef INDEX_MERGE_H
#define INDEX_MERGE_H

#include "index.h"
typedef struct {FILE *f; char* buf; int curRec; int numRec;} buffer;
typedef struct {int *arr; struct entry *cache; int size; } heapStruct;
#define KEY(z) ((void *)(&(heap.cache[heap.arr[(z)]*recSize])))

int merge(int rec_size, int mem_size, int max_degree, char* fin_list, char* out_file_prefix, char* fout_list );

void heapify(int i);



int nextRecord(int i);



void writeRecord(buffer *b, int i);


#endif //INDEX_MERGE_H