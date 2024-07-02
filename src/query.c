//
// Created by linwe on 2023/11/13.
//

#include <string.h>
#include <math.h>
#include "query.h"
#define MAX_TERMS 100

int splitQuery(const char* query, char* terms[]) {
    // Make a copy of the query string since strtok modifies the original string
    char queryCopy[strlen(query) + 1];
    strcpy(queryCopy, query);

    // Tokenize the query
    char* token = strtok(queryCopy, " ");
    int termCount = 0;

    while (token != NULL && termCount < MAX_TERMS) {
        terms[termCount] = malloc(strlen(token) + 1);  // +1 for null terminator
        strcpy(terms[termCount], token);
        termCount++;

        token = strtok(NULL, " ");
    }

    return termCount;
}

int compare_bm25(const void* a, const void* b){
    const id_bm25 *x = (const id_bm25 *)a;
    const id_bm25 *y = (const id_bm25 *)b;
    if (x->bm25 < y->bm25) return -1;
    if (x->bm25 > y->bm25) return 1;
    return 0;
}

int page_id_compare(const void*a, const void*b, void *udata){
    const struct id_Url *pa = a;
    const struct id_Url *pb = b;
    return pa->id - pb->id;
}

uint64_t page_id_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct id_Url *page = item;
    return hashmap_sip(&page->id, sizeof(unsigned int), seed0, seed1);
}

void readLexiconFromFile(FILE *file, lexicon *lc){
    size_t termLength;
    fread(&termLength, sizeof(size_t),1,file);

    lc->term = malloc(termLength* sizeof(char));
    fread(lc->term, sizeof(char), termLength, file);
    fread(&lc->offset, sizeof(off_t), 1, file);
}


id_Url* readPageTableFromFile(FILE *file, unsigned int *maxdocID){
    unsigned int page_length;
    fread(&page_length, sizeof(unsigned int),1,file);
    *maxdocID = page_length;
    id_Url* pages = malloc(sizeof(id_Url)*page_length);
    fread(pages, sizeof(id_Url), page_length, file);
    return pages;
}


void readEntryFromFile(FILE* file, entry* entry, long offset) {

    fseek(file, offset, SEEK_SET); //move file pointer to offset

    // Read posting_length
    fread(&entry->posting_length, sizeof(unsigned int), 1, file);
    fread(&entry->posting_capacity, sizeof(unsigned int), 1, file);
    // Read term
    size_t term_length;
    fread(&term_length, sizeof(size_t), 1, file);
    entry->term = (char*)malloc(term_length * sizeof(char));
    fread(entry->term, sizeof(char), term_length, file);

    // Read the array of Posting structs
    entry->posting = (posting*)malloc(entry->posting_length * sizeof(posting));
    fread(entry->posting, sizeof(posting), entry->posting_length, file);
}

void processTop10(unsigned int top10_size,id_bm25 *top10,struct hashmap *page_map){
    for(int i = top10_size - 1; i >=0; i--){
        struct id_Url *page = (id_Url*)hashmap_get(page_map, &(struct id_Url){.id = top10[i].id});
        printf("bm25 score = %f, URL : %s\n", top10[i].bm25, page->url);
    }
}


struct entry* openList(char* term, struct hashmap *lexicon_map, FILE *entries_file){
    //open the inverted list for term for reading
    const struct lexicon* lc = hashmap_get(lexicon_map,&(struct lexicon){.term=term});
    if(lc == NULL)return NULL;

    struct entry *lp = malloc(sizeof(struct entry));
    readEntryFromFile(entries_file,lp,lc->offset);
    return lp;
}

struct posting* getPosting(entry* lp, unsigned int docID){
    // get the posting at entry lp for a given docID
    unsigned int left,right;
    left=0;right=lp->posting_length;
    while(left <=right){
        unsigned int mid = left + (right-left)/2;
        if(lp->posting[mid].doc_ID == docID){
            return &lp->posting[mid];
        }
        else if(lp->posting[mid].doc_ID >docID){
            right = mid -1;
        }
        else{
            left = mid + 1;
        }
    }

    return NULL;
}



unsigned int nextGEQ(entry* lp, unsigned int k, unsigned int maxdocID){
    // find the next posting in list lp with docID >= k and return its docID.
    // return value > MAXDID if none exits.
    unsigned int left,right;
    left=0;right=lp->posting_length;
    while(left <=right){
        unsigned int mid = left + (right-left)/2;
        if(lp->posting[mid].doc_ID == k){
            return lp->posting[mid].doc_ID;
        }
        else if(lp->posting[mid].doc_ID >k){
            if(mid == 0 || lp->posting[mid-1].doc_ID <k){
                return lp->posting[mid].doc_ID;
            }
            right = mid -1;
        }
        else{
            left = mid + 1;
        }
    }

    return maxdocID + 1;
}

double getScore(entry* lp,unsigned int documentID, unsigned int maxdocID, double average_doc_length, struct hashmap *page_map){
    // get the impact score of the current posting in the list lp

    struct id_Url * cur_page = hashmap_get(page_map,&(struct id_Url){.id = documentID});
    struct posting *p = getPosting(lp,documentID);
    if(cur_page == NULL || p==NULL)return 0;

    unsigned int fd = p->freq;
    unsigned int N = maxdocID;
    unsigned int ft = lp->posting_length;
    unsigned int d = cur_page->total_term;
    double d_avg = average_doc_length;
    double b = 0.75;
    double k1 = 1.2;
    double K = k1 *((1-b) + b*(d/d_avg));
    double bm25 = log((N-ft+0.5)/(ft+0.5))*(((k1+1)*fd)/(K+fd));
    return bm25;
}






int main(){

    printf("loading lexicon...\n");
    FILE* lexicon_file = fopen("../final_index/lexicon.bin","rb");
    if(lexicon_file == NULL){
        fprintf(stderr, "cannot open lexicon file for reading\n");
        exit(1);
    }

    struct hashmap* lexicon_map = hashmap_new(sizeof(struct lexicon),0, 0, 0,term_hash,term_compare,NULL,NULL);
    unsigned int lexicon_length;
    fread(&lexicon_length,sizeof(unsigned int),1,lexicon_file);

    for(unsigned int i = 0; i < lexicon_length; ++i){
        struct lexicon* lc = malloc(sizeof(struct lexicon));
        readLexiconFromFile(lexicon_file, lc);
        //printf("term %s, offset %d\n",lc->term, lc->offset);
        hashmap_set(lexicon_map,lc);
    }
    fclose(lexicon_file);

    printf("loading page table...\n");

    FILE *page_table_file = fopen("../final_index/pageTable.bin","rb");
    if(page_table_file == NULL){
        fprintf(stderr, "cannot open page table file for reading\n");
        exit(1);
    }
    unsigned int maxdocID;
    struct id_Url *page_table = readPageTableFromFile(page_table_file, &maxdocID);
    // add each page to hashmap.
    fclose(page_table_file);

    struct hashmap* page_map = hashmap_new(sizeof(struct id_Url),0, 0, 0,page_id_hash,page_id_compare,NULL,NULL);
    long total_doc_length = 0;
    for(unsigned int i = 0; i < maxdocID; ++i){

        hashmap_set(page_map,&page_table[i]);
        total_doc_length += page_table[i].total_term;
    }
    double average_doc_length = total_doc_length/(double)maxdocID;


    FILE *entries_file = fopen("../final_index/entries.bin","rb");
    if(entries_file == NULL){
        fprintf(stderr, "cannot open entries file for reading\n");
        exit(1);
    }
//    unsigned int entry_length;// comment out if didn't write entry_length to file.
//    fread(&entry_length,sizeof(unsigned int),1,entries_file);
    while(1) {
        char query[1024];
        printf("\n");
        printf("Enter your query: ");
        scanf("%1023[^\n]", query);

        char *terms[MAX_TERMS];
        int queryTerms = splitQuery(query,terms);

        struct entry **entries = malloc(queryTerms* sizeof(struct entry*));
        bool one_or_more_term_missing = false;
        int termCount = 0;
        for(int i = 0 ; i < queryTerms; ++i){
            struct entry *lp = openList(terms[i], lexicon_map, entries_file);
            if(lp == NULL) one_or_more_term_missing = true;
            else entries[termCount++] = lp;
            //printf("term = %s, posting_length = %d\n",entries[i]->term,entries[i]->posting_length);
        }

        if(termCount > 0) {
            qsort(entries, termCount, sizeof(entry *), compare_entry_by_postingLength);


            //process conjunctive query

            struct id_bm25 top10[10];
            size_t top10_size = 0;
            double minimum_bm25 = -1;

            unsigned int did = 0;

            while (did <= maxdocID) {
                did = nextGEQ(entries[0], did, maxdocID);
                unsigned int d;

                for (int i = 0; i < termCount && ((d = nextGEQ(entries[i], did, maxdocID)) == did); i++);

                if (d > did) did = d;
                else {
                    double bm25 = 0;
                    for (int j = 0; j < termCount; j++) {
                        bm25 += getScore(entries[j], did, maxdocID, average_doc_length, page_map);
                    }
                    if (top10_size < 10) {
                        top10[top10_size].bm25 = bm25;
                        top10[top10_size].id = did;
                        top10_size++;
                    } else if (bm25 > minimum_bm25) {
                        top10[0].id = did;
                        top10[0].bm25 = bm25;
                        qsort(top10, top10_size, sizeof(id_bm25), compare_bm25);
                        minimum_bm25 = top10[0].bm25;
                    }
                    did++;
                }
            }
            processTop10(top10_size, top10, page_map);

        }
        else printf("No matching found\n");

        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
    fclose(entries_file);
}