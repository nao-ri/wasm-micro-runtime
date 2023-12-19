/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
// #include <stdio.h>
// #include <unistd.h>
// #include <time.h>
#include <stdlib.h>
#include <string.h>

void
allocateMemory(char **array, int size)
{
    int test;
    test++;
    // char *array = malloc(size * sizeof(char));
    *array = malloc(size * sizeof(char));
    // array = ;
    memset(*array, 0, size * sizeof(char));
}

int
sum(int start, int length)
{
    int sum = 0, i;

    for (i = start; i < start + length; i++) {
        sum += i;
    }

    int iterations = 100;               // プログラムの実行回数
    int initialSize = 100000;           // 初期メモリサイズ
    int increment = 100000;             // メモリの増加量
    sum = start + length + initialSize; // 変更

    int set_size = initialSize;
    for (i = 0; i < iterations; i++) {
        // char *array;
        // allocateMemory(&array, initialSize + i * increment);
        // free(array);
        int *array;
        array = malloc(set_size * sizeof(int));
        memset(array, 0, set_size * sizeof(int));

        array = &iterations;

        // // malloc
        // array = malloc(initialSize * sizeof(char));
        // // size
        // set_size = +increment;
        // // realloc
        // array = realloc(array, increment * sizeof(char));

        // sleep(1);=
    }

    return sum;
}

// int
// fibonacci(int n)
// {
//     if (n == 1 || n == 2) {
//         return 1;
//     }
//     // get_pow(1, 1);
//     return fibonacci(n - 1) + fibonacci(n - 2);
// }

// int
// fibonacci_roop()
// {
//     /* 変数の宣言 */
//     int n;
//     double f0, f1, f2;

//     f0 = 0;
//     f1 = 1;

//     /* フィボナッチ数(n=0)の出力 */
//     // printf("%lf\n", f0);

//     /* フィボナッチ数の計算 */
//     while (1) {
//         n++;
//         // フィボナッチ数の出力(n>0)
//         if (n == 100000000) {
//             // printf("calculation now\n");
//             // printf("%lf\n", f1);
//             // fflush(stdout);
//             n = 0;
//         }
//         // if (n == stop) {
//         //     return 0;
//         // }

//         // フィボナッチ数の計算
//         f2 = f1 + f0;
//         // 変数の代入
//         f0 = f1;
//         f1 = f2;
//     }

//     return 0;
// }
