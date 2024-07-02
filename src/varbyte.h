#ifndef INDEX_VARBYTE_H
#define INDEX_VARBYTE_H

void varEncode(int num,unsigned char* buffer);
int varDecode(const unsigned char * bytes);

#endif //INDEX_VARBYTE_H
