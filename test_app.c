
#include "include/kronos_math.h"

#include <stdio.h>
#include <winsock2.h>

void print_binary_bytes(uint8_t* ptr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            printf("%d", (ptr[i] >> bit) & 1);
        }
        printf(" ");
    }
    printf("\n");
}

int main() {
    uint8_t data[4];
    krs_math_bitmask_set_flag(1, data, 4);

    // Print the bytes as binary
    print_binary_bytes(data, 4);

    printf("%d\n", krs_math_bitmask_get_flag(1, data, 4));
}