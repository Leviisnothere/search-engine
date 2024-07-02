#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "merge.h"
/* data structure for one input/output buffer */


buffer *ioBufs;          /* array of structures for in/output buffers */
heapStruct heap;         /* heap structure */
int recSize;             /* size of record (in bytes) */
int bufSize;             /* # of records that fit in each buffer */


int merge(int rec_size, int mem_size, int max_degree, char* fin_list, char* out_file_prefix, char* fout_list )
{
    FILE *finlist, *foutlist;  /* files with lists of in/output file names */
    int memSize;             /* available memory for merge buffers (in bytes) */
    int maxDegree, degree;   /* max allowed merge degree, and actual degree */
    int numFiles = 0;        /* # of output files that are generated */
    char *bufSpace;
    char filename[1024];
    int i;

    recSize = rec_size;
    memSize = mem_size;
    bufSpace = (unsigned char *) malloc(memSize);
    maxDegree = max_degree;
    ioBufs = (buffer *) malloc((maxDegree + 1) * sizeof(buffer));
    heap.arr = (int *) malloc((maxDegree + 1) * sizeof(int));
    heap.cache = (void *) malloc(maxDegree * recSize);

    finlist =  fopen(fin_list, "r");
    foutlist = fopen(fout_list, "w");

    while (!feof(finlist))
    {
        for (degree = 0; degree < maxDegree; degree++)
        {
            fscanf(finlist, "%s", filename);
            if (feof(finlist)) break;
            ioBufs[degree].f = fopen(filename, "r");
        }
        if (degree == 0) break;

        /* open output file (output is handled by the buffer ioBufs[degree]) */
        sprintf(filename, "%s%d", out_file_prefix, numFiles);
        ioBufs[degree].f = fopen(filename, "w");

        /* assign buffer space (all buffers same space) and init to empty */
        bufSize = memSize / ((degree + 1) * recSize);
        for (i = 0; i <= degree; i++)
        {
            ioBufs[i].buf = &(bufSpace[i * bufSize * recSize]);
            ioBufs[i].curRec = 0;
            ioBufs[i].numRec = 0;
        }

        /* initialize heap with first elements. Heap root is in heap[1] (not 0) */
        heap.size = degree;
        for (i = 0; i < degree; i++)  heap.arr[i+1] = nextRecord(i);
        for (i = degree; i > 0; i--)  heapify(i);

        /* now do the merge - ridiculously simple: do 2 steps until heap empty */
        while (heap.size > 0)
        {
            /* copy the record corresponding to the minimum to the output */
            writeRecord(&(ioBufs[degree]), heap.arr[1]);

            /* replace minimum in heap by the next record from that file */
            if (nextRecord(heap.arr[1]) == -1)
                heap.arr[1] = heap.arr[heap.size--];     /* if EOF, shrink heap by 1 */
            if (heap.size > 1)  heapify(1);
        }

        /* flush output, add output file to list, close in/output files, and next */
        writeRecord(&(ioBufs[degree]), -1);
        fprintf(foutlist, "%s\n", filename);
        for (i = 0; i <= degree; i++)  fclose(ioBufs[i].f);
        numFiles++;
    }

    fclose(finlist);
    fclose(foutlist);
    free(ioBufs);
    free(heap.arr);
    free(heap.cache);
    return 0;
}


/* standard heapify on node i. Note that minimum is node 1. */
void heapify(int i)

{
    int s, t;

    s = i;
    while(1)
    {
        /* find minimum key value of current node and its two children */
        if (((i<<1) <= heap.size) && (compare_entry_by_term(KEY(i<<1) ,KEY(i)) < 0 ))  s = i<<1;
        if (((i<<1)+1 <= heap.size) && (compare_entry_by_term(KEY((i<<1)+1) , KEY(s)) < 0))  s = (i<<1)+1;

        /* if current is minimum, then done. Else swap with child and go down */
        if (s == i)  break;
        t = heap.arr[i];
        heap.arr[i] = heap.arr[s];
        heap.arr[s] = t;
        i = s;
    }
}


/* get next record from input file into heap cache; return -1 if EOF */
int nextRecord(int i)

{
    buffer *b = &(ioBufs[i]);

    /* if buffer consumed, try to refill buffer with data */
    if (b->curRec == b->numRec)
        for (b->curRec = 0, b->numRec = 0; b->numRec < bufSize; b->numRec++)
        {
            fread(&(b->buf[b->numRec*recSize]), recSize, 1, b->f);
            if (feof(b->f))  break;
        }

    /* if buffer still empty, return -1; else copy next record into heap cache */
    if (b->numRec == 0)  return(-1);
    memcpy(heap.cache+i*recSize, &(b->buf[b->curRec*recSize]), recSize);
    b->curRec++;
    return(i);
}


/* copy i-th record from heap cache to out buffer; write to disk if full */
/* If i==-1, then out buffer is just flushed to disk (must be last call) */
void writeRecord(buffer *b, int i)

{
    int j;

    /* flush buffer if needed */
    if ((i == -1) || (b->curRec == bufSize))
    {
        for (j = 0; j < b->curRec; j++)
            fwrite(&(b->buf[j*recSize]), recSize, 1, b->f);
        b->curRec = 0;
    }

    if (i != -1)
        memcpy(&(b->buf[(b->curRec++)*recSize]), heap.cache+i*recSize, recSize);
}
