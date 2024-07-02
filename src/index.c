#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "index.h"
unsigned int docID = 0;  // global variable for docID assignments. Here, docIds are assigned by the order the pages are parsed.
unsigned int entry_offset = 0;  // entry array length
//off_t byte_offset = 0;

unsigned int entry_capacity, page_table_capacity;

unsigned short entries_file_id = 1;  // variable for naming entry files.


int equal(const char* str1, const char* str2){
    return strcmp(str1,str2) == 0;
}
int compare_entry_by_postingLength(const void* a, const void* b){
    const entry *x = *(const entry**)a;
    const entry *y = *(const entry**)b;

    return x->posting_length - y->posting_length;
}
int compare_entry_by_term(const void* a, const void* b){
    const entry *x = (const entry*)a;
    const entry *y = (const entry*)b;

    return (strcmp(x->term,y->term));
}

void sort_entries(entry *entries, unsigned int nElement){
    // sort entries by term

    qsort(entries, nElement,sizeof(entry), compare_entry_by_term);

}

int isASCII(const char* str){
    while(*str){
        if(*str < 0 || *str > 127){
            return 0;
        }
        str++;
    }
    return 1;
}


void writeEntryToFile(FILE* file, const entry* entry, struct hashmap *lexicon_map) {
    // update entries file's byte offset of this specific entry.
    hashmap_set(lexicon_map,&(struct lexicon){.term=entry->term, .offset=ftello(file)});

    // Write posting_length
    fwrite(&entry->posting_length, sizeof(unsigned int), 1, file);
    fwrite(&entry->posting_capacity,sizeof(unsigned int), 1, file);



    // Write term
    size_t term_length = strlen(entry->term) + 1; // Include the null terminator
    fwrite(&term_length, sizeof(size_t), 1, file);

    fwrite(entry->term, sizeof(char), term_length, file);


    // Write the array of Posting structs
    fwrite(entry->posting, sizeof(posting), entry->posting_length, file);
}

void writePageTableToFile(FILE *file, const id_Url* pages){
    fwrite(&docID, sizeof(unsigned int), 1, file); //write length to file;
    fwrite(pages, sizeof(struct id_Url), docID, file);
}

void writeLexiconToFile(FILE *file, const lexicon *lc){
    size_t termLength = strlen(lc->term) + 1;// include null terminate character
    fwrite(&termLength, sizeof(size_t), 1, file);

    fwrite(lc->term, sizeof(char), termLength, file);
    fwrite(&lc->offset, sizeof(off_t), 1, file);
}



int term_compare(const void *a, const void *b, void *udata){
    const struct term_freq * ta = (const term_freq*)a;
    const struct term_freq * tb = (const term_freq*)b;
    return strcmp(ta->term, tb->term);
}

uint64_t term_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct term_freq *tf = item;
    return hashmap_sip(tf->term, strlen(tf->term), seed0, seed1);
}

void buildIndex(struct hashmap* term_freq_map, entry *entries, struct hashmap *lexicon_map){
    size_t iter = 0;
    void *item;
    //printf("term_freq_map size = %zu\n", hashmap_count(term_freq_map));
    while(hashmap_iter(term_freq_map,&iter,&item)){
        const struct term_freq *tf = item;

        // look up into hash table to check entry offset if the term exits, else put at entry_offset++
        struct lexicon *lc;
        lc = hashmap_get(lexicon_map,&(struct lexicon){.term=tf->term});

        if(lc ==NULL) {
            //printf("new word: %s \n",tf->term);
            hashmap_set(lexicon_map, &(struct lexicon) {.term=strdup(tf->term), .offset=entry_offset++});
            lc = hashmap_get(lexicon_map,&(struct lexicon){.term=tf->term});
        }

        struct entry* cur_entry = entries + lc->offset;
        unsigned int p_index = cur_entry->posting_length++;

//        if(equal(tf->term,"the")){
//            printf("the posting length = %d\n",cur_entry->posting_length);
//        }
        if(p_index >= cur_entry->posting_capacity){
            cur_entry->posting_capacity = 3*cur_entry->posting_capacity/2 + 50;
            cur_entry->posting = (posting*)realloc(cur_entry->posting,sizeof(struct posting)*cur_entry->posting_capacity);
            if(!cur_entry->posting){
                fprintf(stderr, "cannot grow posting size");
                exit (EXIT_FAILURE);
            }
        }
        if (cur_entry->term == NULL)
            cur_entry->term = strdup(tf->term);// copy token to term field
        cur_entry->posting[p_index].doc_ID = docID; // add posting doc_ID to entry
        cur_entry->posting[p_index].freq = tf->freq;// term frequency
    }

//    if (entry_offset >= entry_capacity) {
//        // needs to sort the entries and dump it to a file
//        sort_entries(entries, entry_offset - 1);
//        dump_entries(entries, entry_offset - 1);
//    }

}

void splitStringByWhiteSpace(char* input, unsigned int *total_term, struct hashmap *term_freq_map) {
    const char* delimiter = " \t\n\r,.?!:;\"'()[]{}";
    char* token = strtok(input, delimiter);


    while (token != NULL) {
        //printf("%s   %d\n",token,*total_term);   // printf for debug
        token = strtok(NULL, delimiter); // Get the next token

        if(token != NULL && isASCII(token)) {
            ++(*total_term);

            struct term_freq *tf;
            tf = hashmap_get(term_freq_map, &(struct term_freq){.term=token});
            if(tf != NULL)
                hashmap_set(term_freq_map, &(struct term_freq){.term=tf->term,.freq=tf->freq+1});
            else
                hashmap_set(term_freq_map, &(struct term_freq){.term=strdup(token),.freq=1});
        }
    }

}





void processDoc(FILE* file, id_Url* pages, entry *entries, struct hashmap *lexicon_map, unsigned int max_index_size,  unsigned int max_page_table){
    char line[1024];

    while (fgets(line, sizeof(line), file) != NULL && entry_offset <= max_index_size && docID < max_page_table - 1) {
        if(equal(line,"<TEXT>\n")){
            // new page encountered
            struct hashmap *term_freq_map = hashmap_new(sizeof(term_freq), 0,0,0,term_hash,term_compare,NULL,NULL);
            fgets(pages[docID].url,sizeof(pages[docID].url), file); //get url
            pages[docID].page_offset = ftello(file);
            pages[docID].total_term = 0;
            pages[docID].id = docID;
            while (fgets(line,sizeof(line),file) != NULL && !equal(line,"</TEXT>\n")){
                // parsing the current document...
                splitStringByWhiteSpace(line, &(pages[docID].total_term), term_freq_map);
            }
            buildIndex(term_freq_map,entries,lexicon_map);

            hashmap_free(term_freq_map);

            ++docID;

            printf("doc_id = %d  , entry_offset = %d \n",docID,entry_offset);
        }

    }

}


//int main() {
//    FILE *input_file = fopen("../fulldocs-new.trec", "r");
//
//    if (input_file == NULL) {
//        fprintf(stderr, "Error: Unable to open the file.\n");
//        return EXIT_FAILURE;
//    }
//    unsigned int page_size  = (int)(3213836/4); //number of pages. total 3213836
//    unsigned int index_size = 10200000; // number of entries
////    unsigned int lexicon_size = 500; // maximum in-memory lexicon size in MB.
//    unsigned int initial_posting_length = 50;
////    scanf("%u",&max_page_table);
////    scanf("%u",&max_index);
//
////    page_size = page_size * (int)pow(10,6);  // max_page_table in Bytes. Note that the maximum unsigned int is 4,294,967,295
////    page_size = (int)(page_size/sizeof(id_Url));      // Integer_floor[total bytes/sizeof(Id_Url)]
////
////    index_size = index_size * (int)pow(10,6);  // max_index in Bytes.
////    index_size = (int)(index_size/sizeof(entry));
//
////    max_lexicon = max_lexicon * (int)pow(10,6);  // max_lexicon in Bytes.
////    max_lexicon = max_lexicon/sizeof(Id_Url); //need to update sizeof(???)
//
//    id_Url *pages = (id_Url*)malloc(sizeof(id_Url)*page_size); // Id_Url struct is 4 + 4 + 1024 = 1032 bytes,
//
//    page_table_capacity = page_size;
//    printf("%d\n",index_size);
//    entry *entries = (entry*)malloc(sizeof(entry)*index_size);
//    for(int i = 0; i < index_size; i++){
//        entries[i].posting_length = 0;
//        entries[i].posting_capacity = initial_posting_length;
//        entries[i].term = NULL;
//        entries[i].posting = malloc(sizeof(posting)* initial_posting_length);
//    }
//
//    entry_capacity = index_size;
//
//    struct hashmap* lexicon_map = hashmap_new(sizeof(struct lexicon),0, 0, 0,term_hash,term_compare,NULL,NULL);
//
//    processDoc(input_file, pages, entries, lexicon_map, index_size - 1000, page_size); //process document and generates sorted entries for merging in the next step.
//    //merge(sizeof(Entry),(int)(index_size*pow(10,6)), entries_file_id, "..\\entries", "merged","..\\final_index" );
//
//    //sort_entries(entries, entry_offset - 1);
//    fclose(input_file);
//    FILE *entries_file = fopen("../final_index/entries.bin","wb");
//    if (entries_file == NULL) {
//        fprintf(stderr, "Error opening file for writing.\n");
//        return 1;
//    }
//
//    printf("writing entries to file...\n");
//
//    //fwrite(&entry_offset,sizeof(unsigned int),1,entries_file);  //no need to write entries length to file. just use offset field from lexicon to determine the position of an entry in file.
//
//    for (unsigned int i = 0; i < entry_offset; ++i){
//        writeEntryToFile(entries_file, &entries[i],lexicon_map);
//    }
//    fclose(entries_file);
//
//    printf("writing page table to file...\n");
//    FILE *pages_file = fopen("../final_index/pageTable.bin","wb");
//    if (pages_file == NULL) {
//        fprintf(stderr, "Error opening file for writing.\n");
//        return 1;
//    }
//    writePageTableToFile(pages_file, pages);
//    fclose(pages_file);
//
//    printf("writing lexicon to file...\n");
//    FILE *lexicon_file = fopen("../final_index/lexicon.bin","wb");
//    if (lexicon_file == NULL) {
//        fprintf(stderr, "Error opening file for writing.\n");
//        return 1;
//    }
//
//    unsigned int lexicon_size = hashmap_count(lexicon_map);
//    fwrite(&lexicon_size,sizeof(unsigned int),1,lexicon_file);
//    size_t iter = 0;
//    void *item;
//    while(hashmap_iter(lexicon_map,&iter,&item)){
//        const struct lexicon *lc = item;
//        writeLexiconToFile(lexicon_file,lc);
//    }
//
//    fclose(lexicon_file);
//    return 0;
//
//}
