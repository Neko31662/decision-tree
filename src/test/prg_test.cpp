#include "utils/prg_sync.h"

int main() {
    block seed = emp::makeBlock(0x0123456789abcdef, 0xfedcba9876543210);
    PRGSync prg1(&seed);
    PRGSync prg2(&seed);
    unsigned char data1[200000];
    unsigned char data2[200000];
    const int test_size = 200;
    int cur = 0;
    prg1.gen_random_data((void *)(data1 + cur), 9999);
    cur += 9999;
    for (int i = 1; i <= test_size; ++i) {
        prg1.gen_random_data((void *)(data1 + cur), i);
        cur += i;
    }

    cur = 0;
    for (int i = test_size; i >= 1; --i) {
        prg2.gen_random_data((void *)(data2 + cur), i);
        cur += i;
    }
    prg2.gen_random_data((void *)(data2 + cur), 9999);
    cur += 9999;

    for (int i = 0; i < cur; ++i) {
        if (data1[i] != data2[i]) {
            printf("PRGSync test failed at byte %d: %02x != %02x\n", i, data1[i], data2[i]);
            return 1;
        }
    }
    printf("PRGSync test passed!\n");
}