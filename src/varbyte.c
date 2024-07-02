#include <string.h>
#include "varbyte.h"



void varEncode(int num, unsigned char* buffer){
    int idx = 0;
    while (num > 0) {
        unsigned char byte = num % 128;
        num /= 128;
        if (num > 0) {
            byte = byte + 128;
        }
        buffer[idx++] = byte;
    }
}

int varDecode(const unsigned char* bytes) {
    int num = 0;
    int shift = 0;
    int idx = 0;

    while (1) {
        unsigned char byte = bytes[idx++];
        num += (byte & 127) << shift;
        shift += 7;

        if (!(byte & 128)) {
            break;
        }
    }

    return num;
}

