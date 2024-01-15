/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
/*fibonacciを実行した後にsumを実行する*/

#include "wasm_export.h"
#include "bh_read_file.h"
#include "pthread.h"
#include "wasm_runtime_common.h"

#define THREAD_NUM 2

typedef struct ThreadArgs {
    wasm_exec_env_t exec_env;
    int start;
    int length;
} ThreadArgs;

void *
thread(void *arg)
{
    ThreadArgs *thread_arg = (ThreadArgs *)arg;
    wasm_exec_env_t exec_env = thread_arg->exec_env;
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    wasm_function_inst_t func;
    uint32 argv[2];

    if (!wasm_runtime_init_thread_env()) {
        printf("failed to initialize thread environment");
        return NULL;
    }

    func = wasm_runtime_lookup_function(module_inst, "sum", NULL);
    if (!func) {
        printf("failed to lookup function sum");
        wasm_runtime_destroy_thread_env();
        return NULL;
    }
    argv[0] = thread_arg->start;
    argv[1] = thread_arg->length;

    /* call the WASM function */
    if (!wasm_runtime_call_wasm(exec_env, func, 2, argv)) {
        printf("%s\n", wasm_runtime_get_exception(module_inst));
        wasm_runtime_destroy_thread_env();
        return NULL;
    }

    wasm_runtime_destroy_thread_env();
    return (void *)(uintptr_t)argv[0];
}

void *
wamr_thread_cb(wasm_exec_env_t exec_env, void *arg)
{
    ThreadArgs *thread_arg = (ThreadArgs *)arg;
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    wasm_function_inst_t func;
    uint32 argv[2];

    func = wasm_runtime_lookup_function(module_inst, "sum", NULL);
    if (!func) {
        printf("failed to lookup function sum");
        return NULL;
    }
    argv[0] = thread_arg->start;
    argv[1] = thread_arg->length;

    /* call the WASM function */
    if (!wasm_runtime_call_wasm(exec_env, func, 2, argv)) {
        printf("%s\n", wasm_runtime_get_exception(module_inst));
        return NULL;
    }
    // argv[0] = 10000000;
    /*roop*/
    // wasm_function_inst_t func_fibonacci_roop =
    //     wasm_runtime_lookup_function(module_inst, "fibonacci_roop", NULL);
    // if (!wasm_runtime_call_wasm(exec_env, func_fibonacci_roop, 2, argv)) {
    //     printf("%s\n", wasm_runtime_get_exception(module_inst));
    //     return NULL;
    // }

    return (void *)(uintptr_t)argv[0];
}

static char global_heap_buf[512 * 1024];
static char global_heap_buf_other[512 * 1024];

int
main(int argc, char *argv[])
{
    char *wasm_file = "wasm-apps/test.wasm"; // 二つの引数を足して返す
    uint8 *wasm_file_buf = NULL;
    uint32 wasm_file_size, wasm_argv[2], i, threads_created;
    uint32 stack_size = 16 * 1024, heap_size = 16 * 1024;
    wasm_module_t wasm_module = NULL;
    wasm_module_inst_t wasm_module_inst = NULL;
    wasm_exec_env_t exec_env = NULL;
    RuntimeInitArgs init_args;
    ThreadArgs thread_arg[THREAD_NUM];
    pthread_t tid[THREAD_NUM];
    wasm_thread_t wasm_tid[THREAD_NUM];
    uint32 result[THREAD_NUM], sum;
    wasm_function_inst_t func;
    char error_buf[128] = { 0 };

    memset(thread_arg, 0, sizeof(ThreadArgs) * THREAD_NUM);
    memset(&init_args, 0, sizeof(RuntimeInitArgs));
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = malloc;
    init_args.mem_alloc_option.allocator.realloc_func = realloc;
    init_args.mem_alloc_option.allocator.free_func = free;
    init_args.max_thread_num = THREAD_NUM;

    /*memory mode = Alloc_With_Pool*/
    // init_args.mem_alloc_type = Alloc_With_Pool;
    // init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    // init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);
    // init_args.max_thread_num = THREAD_NUM;

    /* initialize runtime environment */
    if (!wasm_runtime_full_init(&init_args)) {
        printf("Init runtime environment failed.\n");
        return -1;
    }

    /* load WASM byte buffer from WASM bin file */
    if (!(wasm_file_buf =
              (uint8 *)bh_read_file_to_buffer(wasm_file, &wasm_file_size))) {
        // goto fail1;
    }

    /* load WASM module */
    if (!(wasm_module = wasm_runtime_load(wasm_file_buf, wasm_file_size,
                                          error_buf, sizeof(error_buf)))) {
        printf("%s\n", error_buf);
        // goto fail2;
    }

    /* instantiate the module */
    if (!(wasm_module_inst =
              wasm_runtime_instantiate(wasm_module, stack_size, heap_size,
                                       error_buf, sizeof(error_buf)))) {
        printf("%s\n", error_buf);
        // goto fail3;
    }

    /* Create the first exec_env */
    if (!(exec_env =
              wasm_runtime_create_exec_env(wasm_module_inst, stack_size))) {
        printf("failed to create exec_env\n");
        // goto fail4;
    }

    // /*wasm_runtime_dump_mem_consumption*/
    // printf("\n[sum wasm module]\n");
    // printf("--use wasm_runtime_dump_mem_consumption--\n");
    // printf("--After wasm_runtime_create_exec_env --\n");
    // wasm_runtime_dump_mem_consumption(exec_env);

    /*sum module exec test*/
    func = wasm_runtime_lookup_function(wasm_module_inst, "sum", NULL);
    if (!func) {
        printf("failed to lookup function sum");
        // goto fail5;
    }
    wasm_argv[0] = 10;
    wasm_argv[1] = THREAD_NUM * 10;

    /*
     * Execute the wasm function in current thread, get the expect result
     */
    if (!wasm_runtime_call_wasm(exec_env, func, 2, wasm_argv)) {
        printf("errer in wasm_runtime_call_wasm\n");
        printf("%s\n", wasm_runtime_get_exception(wasm_module_inst));
    }
    printf("expect result: %d\n", wasm_argv[0]);

    // /*wasm_runtime_dump_mem_consumption*/
    // printf("\n[sum wasm module]\n");
    // printf("--use wasm_runtime_dump_mem_consumption--\n");
    // printf("--After wasm_runtime_call_wasm--\n");
    // wasm_runtime_dump_mem_consumption(exec_env);

    /*
    other module init
    */
    char *wasm_file_other =
        "wasm-apps/fibonacci.wasm"; // 引数に関わらずフィボナッチを無限に計算する（帰ってこない）
    uint8 *wasm_file_buf_other = NULL;
    uint32 wasm_file_size_other, wasm_argv_other[2], i_other,
        threads_created_other;
    uint32 stack_size_other = 16 * 1024, heap_size_other = 16 * 1024;
    wasm_module_t wasm_module_other = NULL;
    wasm_module_inst_t wasm_module_inst_other = NULL;
    wasm_exec_env_t exec_env_other = NULL;
    RuntimeInitArgs init_args_other;
    ThreadArgs thread_arg_other[THREAD_NUM];
    pthread_t tid_other[THREAD_NUM];
    wasm_thread_t wasm_tid_other[THREAD_NUM];
    uint32 result_other[THREAD_NUM], sum_other;
    wasm_function_inst_t func_other;
    char error_buf_other[128] = { 0 };

    /*other module init args*/
    memset(thread_arg_other, 0, sizeof(ThreadArgs) * THREAD_NUM);
    memset(&init_args_other, 0, sizeof(RuntimeInitArgs));
    init_args_other.mem_alloc_type = Alloc_With_Allocator;
    init_args_other.mem_alloc_option.allocator.malloc_func = malloc;
    init_args_other.mem_alloc_option.allocator.realloc_func = realloc;
    init_args_other.mem_alloc_option.allocator.free_func = free;
    init_args_other.max_thread_num = THREAD_NUM;

    /*memory mode = Alloc_With_Pool*/
    // init_args_other.mem_alloc_type = Alloc_With_Pool;
    // init_args_other.mem_alloc_option.pool.heap_buf = global_heap_buf_other;
    // init_args_other.mem_alloc_option.pool.heap_size =
    //     sizeof(global_heap_buf_other);
    // init_args_other.max_thread_num = THREAD_NUM;

    /* initialize runtime environment */
    if (!wasm_runtime_full_init(&init_args_other)) {
        printf("Init runtime environment failed.\n");
        return -1;
    }
    if (!(wasm_file_buf_other = (uint8 *)bh_read_file_to_buffer(
              wasm_file_other, &wasm_file_size_other))) {
        // goto fail1;
    }

    if (!(wasm_module_other =
              wasm_runtime_load(wasm_file_buf_other, wasm_file_size_other,
                                error_buf_other, sizeof(error_buf_other)))) {
        printf("%s\n", error_buf_other);
        // goto fail2;
    }

    if (!(wasm_module_inst_other = wasm_runtime_instantiate(
              wasm_module_other, stack_size_other, heap_size_other,
              error_buf_other, sizeof(error_buf_other)))) {
        printf("%s\n", error_buf_other);
        // goto fail3;
    }

    if (!(exec_env_other = wasm_runtime_create_exec_env(wasm_module_inst_other,
                                                        stack_size_other))) {
        printf("failed to create exec_env\n");
        // goto fail4;
    }

    // /*wasm_runtime_dump_mem_consumption*/
    // printf("\n[other wasm module]\n");
    // printf("--use wasm_runtime_dump_mem_consumption--\n");
    // printf("--After wasm_runtime_create_exec_env --\n");
    // wasm_runtime_dump_mem_consumption(exec_env_other);

    // /*other module exec test*/
    // func_other =
    //     wasm_runtime_lookup_function(wasm_module_inst_other, "sum", NULL);
    // wasm_argv_other[0] = 0;
    // wasm_argv_other[1] = THREAD_NUM * 10;
    // if (!wasm_runtime_call_wasm(exec_env_other, func_other, 2,
    //                             wasm_argv_other)) {
    //     printf("%s\n", wasm_runtime_get_exception(wasm_module_inst_other));
    // }
    // printf("expect result_other: %d\n", wasm_argv_other[0]);

    // /*other module*/
    // memset(thread_arg_other, 0, sizeof(ThreadArgs) * THREAD_NUM);
    // for (i_other = 0; i_other < THREAD_NUM; i_other++) {
    //     thread_arg_other[i].start = 10 * i;
    //     thread_arg_other[i].length = 10;
    //     printf("[DEBUG]run other_module wasm_runtime_spawn_thread\n");
    //     /* No need to spawn exec_env manually */
    //     WASMExecEnv *new_exec_env_other =
    //         wasm_runtime_spawn_exec_env(exec_env_other);
    //     if (0
    //         != wasm_runtime_spawn_thread(new_exec_env_other,
    //         &wasm_tid_other[i],
    //                                      wamr_thread_cb,
    //                                      &thread_arg_other[i])) {
    //         printf("failed to spawn other_module thread.\n");
    //         break;
    //     }
    // }

    /*exec_envを配列で管理*/
    memset(thread_arg_other, 0, sizeof(ThreadArgs) * THREAD_NUM);
    for (i_other = 0; i_other < THREAD_NUM; i_other++) {
        thread_arg_other[i_other].start = 10 * i;
        thread_arg_other[i_other].length = 10;
        printf("[DEBUG]run other_module wasm_runtime_spawn_thread\n");
        /* No need to spawn exec_env manually */
        WASMExecEnv *new_exec_env_other =
            wasm_runtime_spawn_exec_env(exec_env_other);

        if (new_exec_env_other)
            thread_arg_other[i_other].exec_env = new_exec_env_other;
        else {
            printf("failed to spawn exec_env\n");
            break;
        }
        if (0
            != wasm_runtime_spawn_thread(
                thread_arg_other[i_other].exec_env, &wasm_tid_other[i_other],
                wamr_thread_cb, &thread_arg_other[i_other])) {
            printf("failed to spawn other_module thread.\n");
            break;
        }
    }
    // prom_main_time(thread_arg_other[1].exec_env, 1);
    printf("@@@@@@@@@@@1\n");
    printf("exec_env_other %p\n", exec_env_other);
    printf("prom_exe_env_array[1] = thread_arg[1].exec_env %p\n",
           thread_arg_other[0].exec_env);
    printf("prom_exe_env_array[1] = thread_arg[1].exec_env %p\n",
           thread_arg_other[1].exec_env);

    /*テスト*/
    // WASMExecEnv *new_exec_env_other1 =
    //     wasm_runtime_spawn_exec_env(exec_env_other);
    // wasm_runtime_spawn_thread(new_exec_env_other1, &wasm_tid_other[1],
    //                           wamr_thread_cb, &thread_arg_other[1]);

    /*
     * Run wasm function in multiple thread created by pthread_create
     */
    memset(thread_arg, 0, sizeof(ThreadArgs) * THREAD_NUM);
    for (i = 0; i < THREAD_NUM; i++) {
        wasm_exec_env_t new_exec_env;
        thread_arg[i].start = 10 * i;
        thread_arg[i].length = 10;

        /* spawn a new exec_env to be executed in other threads */
        new_exec_env = wasm_runtime_spawn_exec_env(exec_env);
        if (new_exec_env)
            thread_arg[i].exec_env = new_exec_env;
        else {
            printf("failed to spawn exec_env\n");
            break;
        }

        // /*wasm_runtime_dump_mem_consumption*/
        // printf("\n[sum wasm module]\n");
        // printf("--use wasm_runtime_dump_mem_consumption--\n");
        // printf("--After new_exec_env wasm_runtime_spawn_exec_env --\n");
        // wasm_runtime_dump_mem_consumption(new_exec_env);

        wasm_runtime_destroy_spawned_exec_env(new_exec_env);
        /* If we use:
            thread_arg[i].exec_env = exec_env,
            we may get wrong result */
        // printf("[DEBUG]run pthread_create\n");
        // if (0 != pthread_create(&tid[i], NULL, thread, &thread_arg[i])) {
        //     printf("failed to create thread.\n");
        //     wasm_runtime_destroy_spawned_exec_env(new_exec_env);
        //     break;
        // }
    }

    // threads_created = i;

    // sum = 0;
    // memset(result, 0, sizeof(uint32) * THREAD_NUM);
    // for (i = 0; i < threads_created; i++) {
    //     pthread_join(tid[i], (void **)&result[i]);
    //     sum += result[i];
    //     /* destroy the spawned exec_env */
    //     if (thread_arg[i].exec_env)
    //         wasm_runtime_destroy_spawned_exec_env(thread_arg[i].exec_env);
    // }
    printf("[DEBUG]before pthread_join \n");
    printf("[pthread]sum result: %d\n", sum);
    printf("[DEBUG]after pthread_join\n\n");
    /*
     * Run wasm function in multiple thread created by wamr spawn API
     */
    // wasm_exec_env_t new_exec_env1;
    // new_exec_env1 = wasm_runtime_spawn_exec_env(exec_env);
    memset(thread_arg, 0, sizeof(ThreadArgs) * THREAD_NUM);
    for (i = 0; i < THREAD_NUM; i++) {
        thread_arg[i].start = 10;
        thread_arg[i].length = 10;
        printf("[DEBUG]run wasm_runtime_spawn_thread\n");
        /* No need to spawn exec_env manually */
        WASMExecEnv *new_exec_env = wasm_runtime_spawn_exec_env(exec_env);

        /* spawn a new exec_env to be executed in other threads */
        if (new_exec_env)
            thread_arg[i].exec_env = new_exec_env;
        else {
            printf("failed to spawn exec_env\n");
            break;
        }

        if (0
            != wasm_runtime_spawn_thread(thread_arg[i].exec_env, &wasm_tid[i],
                                         wamr_thread_cb, &thread_arg[i])) {
            printf("failed to spawn thread.\n");
            break;
        }
    }

    // WASMExecEnv
    // *exec_envを配列に格納して関数に渡せるようにするために、配列を作る
    int prom_exec_env_num = 2;
    wasm_exec_env_t *prom_exe_env_array[prom_exec_env_num];
    prom_exe_env_array[0] = thread_arg_other[0].exec_env;
    prom_exe_env_array[1] = thread_arg_other[1].exec_env;
    // prom_exe_env_array[0] = thread_arg[0].exec_env;
    // prom_exe_env_array[1] = thread_arg[1].exec_env;
    // prom_exe_env_array[0] = exec_env;
    // prom_exe_env_array[1] = exec_env_other;

    printf("@@@@@@@@@@@2\n");
    printf("exec_env_other %p\n", exec_env_other);
    printf("prom_exe_env_array[1] = thread_arg[1].exec_env %p\n",
           thread_arg_other[0].exec_env);
    printf("prom_exe_env_array[1] = thread_arg[1].exec_env %p\n",
           thread_arg_other[1].exec_env);

    prom_main_multi(prom_exe_env_array, prom_exec_env_num, 1);
    // prom_main_time(exec_env_other, 1);
    // prom_main_time(exec_env_other, 1);

    // prometheusのメモリ計測
    // prom_main();
    // prom_main_time(thread_arg_other[0].exec_env, 1);

    /*指定したexec_env関連付いたメモリインスタンス（リニアメモリ）についてファイルに出力*/
    // wasm_runtime_measure_mem_use(exec_env_other);
    // wasm_runtime_measure_mem_use(thread_arg_other[0].exec_env); //
    // fibonacci
    //                                                             // wasm
    // wasm_runtime_measure_mem_use(thread_arg_other[1].exec_env);
    // wasm_runtime_measure_mem_use(thread_arg[0].exec_env); // sum wasm
    // wasm_runtime_measure_mem_use(thread_arg[1].exec_env);

    // /*wasm_runtime_dump_mem_consumption*/
    // printf("\n[sum wasm module]\n");
    // printf("--use wasm_runtime_dump_mem_consumption--\n");
    // printf("--After wasm_runtime_spawn_thread --\n");
    // wasm_runtime_dump_mem_consumption(exec_env);

    printf("[DEBUG] after wasm_runtime_spawn_thread\n");
    threads_created = i;

    sum = 0;
    memset(result, 0, sizeof(uint32) * THREAD_NUM);
    for (i = 0; i < threads_created; i++) {
        wasm_runtime_join_thread(wasm_tid[i], (void **)&result[i]);
        printf("result[i] %d%d\n", result[i], i);
        sum += result[i];
        /* No need to destroy the spawned exec_env */
    }
    printf("[spwan_thread]sum result: %d(func in wasm:%d*10*thread_num)\n", sum,
           THREAD_NUM);

    /*other module*/
    threads_created_other = i_other;
    sum_other = 0;
    memset(result_other, 0, sizeof(uint32) * THREAD_NUM);
    for (i = 0; i < threads_created_other; i++) {
        wasm_runtime_join_thread(wasm_tid_other[i], (void **)&result_other[i]);
        sum_other += result_other[i];
        /* No need to destroy the spawned exec_env */
    }
    printf("[spwan_thread]sum_other result: %d\n", sum_other);

fail5:
    wasm_runtime_destroy_exec_env(exec_env);

fail4:
    /* destroy the module instance */
    wasm_runtime_deinstantiate(wasm_module_inst);

fail3:
    /* unload the module */
    wasm_runtime_unload(wasm_module);

fail2:
    /* free the file buffer */
    wasm_runtime_free(wasm_file_buf);

fail1:
    /* destroy runtime environment */
    wasm_runtime_destroy();
    return 0;
}
