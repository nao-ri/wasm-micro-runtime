/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
// #include <stdio.h>
// #include <unistd.h>

int
sum(int start, int length)
{
    int sum = 0, i;

    for (i = start; i < start + length; i++) {
        sum += i;
    }

    sum = start + length;

    // while (1) {
    //     /* code */
    //     // sleep(1);
    // }

    // /* 変数の宣言 */
    // int n;
    // double f0, f1, f2;

    // f0 = 0;
    // f1 = 1;

    // /* フィボナッチ数(n=0)の出力 */
    // // printf("%lf\n", f0);

    // /* フィボナッチ数の計算 */
    // while (1) {
    //     n++;
    //     // フィボナッチ数の出力(n>0)
    //     if (n == 100000000) {
    //         // printf("calculation now\n");
    //         // printf("%lf\n", f1);
    //         // fflush(stdout);
    //         // n = 0;
    //         break;
    //     }
    //     // if (n == stop) {
    //     //     return 0;
    //     // }

    //     // フィボナッチ数の計算
    //     f2 = f1 + f0;
    //     // 変数の代入
    //     f0 = f1;
    //     f1 = f2;
    // }

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

int
fibonacci_roop()
{
    /* 変数の宣言 */
    int n;
    double f0, f1, f2;

    f0 = 0;
    f1 = 1;

    /* フィボナッチ数(n=0)の出力 */
    // printf("%lf\n", f0);

    /* フィボナッチ数の計算 */
    while (1) {
        n++;
        // フィボナッチ数の出力(n>0)
        if (n == 100000000) {
            // printf("calculation now\n");
            // printf("%lf\n", f1);
            // fflush(stdout);
            n = 0;
        }
        // if (n == stop) {
        //     return 0;
        // }

        // フィボナッチ数の計算
        f2 = f1 + f0;
        // 変数の代入
        f0 = f1;
        f1 = f2;
    }

    return 0;
}
