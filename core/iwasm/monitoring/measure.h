#include "bh_platform.h"
#include "bh_common.h"
#include "bh_assert.h"
#include "bh_log.h"
#include "wasm_native.h"
#include "wasm_runtime_common.h"
#include "wasm_memory.h"
#if WASM_ENABLE_INTERP != 0
#include "../interpreter/wasm_runtime.h"
#endif
#if WASM_ENABLE_AOT != 0
#include "../aot/aot_runtime.h"
#if WASM_ENABLE_DEBUG_AOT != 0
#include "../aot/debug/jit_debug.h"
#endif
#endif
#if WASM_ENABLE_THREAD_MGR != 0
#include "../libraries/thread-mgr/thread_manager.h"
#if WASM_ENABLE_DEBUG_INTERP != 0
#include "../libraries/debug-engine/debug_engine.h"
#endif
#endif
#if WASM_ENABLE_SHARED_MEMORY != 0
#include "wasm_shared_memory.h"
#endif
#if WASM_ENABLE_FAST_JIT != 0
#include "../fast-jit/jit_compiler.h"
#endif
#if WASM_ENABLE_JIT != 0 || WASM_ENABLE_WAMR_COMPILER != 0
#include "../compilation/aot_llvm.h"
#endif
#include "../common/wasm_c_api_internal.h"
#include "../../version.h"

/**
 * For runtime build, BH_MALLOC/BH_FREE should be defined as
 * wasm_runtime_malloc/wasm_runtime_free.
 */
#define CHECK(a) CHECK1(a)
#define CHECK1(a) SHOULD_BE_##a

#define SHOULD_BE_wasm_runtime_malloc 1
#if !CHECK(BH_MALLOC)
#error unexpected BH_MALLOC
#endif
#undef SHOULD_BE_wasm_runtime_malloc

#define SHOULD_BE_wasm_runtime_free 1
#if !CHECK(BH_FREE)
#error unexpected BH_FREE
#endif
#undef SHOULD_BE_wasm_runtime_free

#undef CHECK
#undef CHECK1

#if WASM_ENABLE_MULTI_MODULE != 0
/**
 * A safety insurance to prevent
 * circular depencies which leads stack overflow
 * try to break early
 */
typedef struct LoadingModule {
    bh_list_link l;
    /* point to a string pool */
    const char *module_name;
} LoadingModule;

static bh_list loading_module_list_head;
static bh_list *const loading_module_list = &loading_module_list_head;
static korp_mutex loading_module_list_lock;

/**
 * A list to store all exported functions/globals/memories/tables
 * of every fully loaded module
 */
static bh_list registered_module_list_head;
static bh_list *const registered_module_list = &registered_module_list_head;
static korp_mutex registered_module_list_lock;
static void
wasm_runtime_destroy_registered_module_list();
#endif /* WASM_ENABLE_MULTI_MODULE */

/* RSSを計算する関数 */
#define PAGE_SIZE 0x1000
static void
print_page(uint64_t address, uint64_t data)
{
    printf("[PAGEMAP]0x%-16lx : pfn %-16lx soft-dirty %ld file/shared %ld "
           "swapped %ld present %ld\n",
           address, data & 0x7fffffffffffff, (data >> 55) & 1, (data >> 61) & 1,
           (data >> 62) & 1, (data >> 63) & 1);
}

int
get_rss(uint64_t start_array_address, uint64_t end_array_address)
{

    // time_t start_time, end_time;
    // /* 処理開始前の時間を取得 */
    // start_time = time(NULL);

    char filename[BUFSIZ];
    int RSS_num = 0;
    // if (argc != 4) {
    //     printf("Usage: %s pid start_address end_address\n", argv[0]);
    //     return 1;
    // }

    errno = 0;
    // int pid = (int)strtol(argv[1], NULL, 0);
    int pid = getpid();

    if (errno) {
        perror("strtol");
        return 1;
    }
    snprintf(filename, sizeof filename, "/proc/%d/pagemap", pid);

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // uint64_t start_address = strtoul(argv[2], NULL, 0);
    // uint64_t end_address = strtoul(argv[3], NULL, 0);

    uint64_t start_address = start_array_address;
    uint64_t end_address = end_array_address;

    for (uint64_t i = start_address; i < end_address; i += 0x1000) {
        uint64_t data;
        uint64_t index = (i / PAGE_SIZE) * sizeof(data);
        if (pread(fd, &data, sizeof(data), index) != sizeof(data)) {
            perror("pread");
            break;
        }

        // RSS計算　presentが1のものをカウント
        if ((data >> 63) & 1) {
            RSS_num = RSS_num + 1;
        }

        // print_page(i, data);
    }

    close(fd);

    /* RSSの表示 */
    int RSS_value = RSS_num * PAGE_SIZE;
    printf("[PAGEMAP]RSS:%dKB\n", RSS_num * PAGE_SIZE / 1024);

    // /* 処理開始後の時間とクロックを取得 */
    // end_time = time(NULL);
    // /* 計測時間の表示 */
    // printf("[PAGEMAP]time:%ld\n\n", end_time - start_time);
    return RSS_value;
}
/* ここまでRSSを計算する関数 */

void
wasm_runtime_measure_mem(WASMExecEnv *exec_env)
{
    WASMModuleInstMemConsumption module_inst_mem_consps;
    WASMModuleMemConsumption module_mem_consps;
    WASMModuleInstanceCommon *module_inst_common;
    WASMModuleCommon *module_common = NULL;
    void *heap_handle = NULL;
    uint32 total_size = 0, app_heap_peak_size = 0;
    uint32 max_aux_stack_used = -1;

    module_inst_common = exec_env->module_inst;
#if WASM_ENABLE_INTERP != 0
    if (module_inst_common->module_type == Wasm_Module_Bytecode) {
        WASMModuleInstance *wasm_module_inst =
            (WASMModuleInstance *)module_inst_common;
        WASMModule *wasm_module = wasm_module_inst->module;
        module_common = (WASMModuleCommon *)wasm_module;
        if (wasm_module_inst->memories) {
            heap_handle = wasm_module_inst->memories[0]->heap_handle;
        }
        wasm_get_module_inst_mem_consumption(wasm_module_inst,
                                             &module_inst_mem_consps);
        wasm_get_module_mem_consumption(wasm_module, &module_mem_consps);
        if (wasm_module_inst->module->aux_stack_top_global_index != (uint32)-1)
            max_aux_stack_used = wasm_module_inst->e->max_aux_stack_used;
    }
#endif

    // 5秒ごとにメモリの使用量をファイルに書き込み5ループしたら終了
    for (int i = 1; i <= 5; i++) {
        FILE *fp;
        char *filename = "memory_usage.txt";
        if ((fp = fopen(filename, "a")) == NULL) {
            printf("file open error!!\n");
            exit(EXIT_FAILURE);
        }

        fprintf(
            fp, "exec_env_addr: %p RSS: %d B\\n", exec_env,
            get_rss(wasm_module_inst_debug->memories[0]->memory_data,
                    wasm_module_inst_debug->memories[0]->memory_data
                        + wasm_module_inst_debug->memories[0]->cur_page_count
                              * wasm_module_inst_debug->memories[0]
                                    ->num_bytes_per_page));
        fclose(fp);
        sleep(5);
    }

    WASMModuleInstance *wasm_module_inst_debug =
        (WASMModuleInstance *)module_inst_common;
    // os_printf("[DEBUG]wasm_module_inst->memories[0]->memory_data:     %p\n",
    //           wasm_module_inst_debug->memories[0]->memory_data);
    // os_printf("[DEBUG]wasm_module_inst->memories[0]->heap_data:       %p\n",
    //           wasm_module_inst_debug->memories[0]->heap_data);
    // os_printf("[DEBUG]wasm_module_inst->memories[0]->memory_data_end: %p\n",
    //           wasm_module_inst_debug->memories[0]->memory_data_end);
    // os_printf("[DEBUG]wasm_module_inst->memories[0]->heap_data_end:   %p\n",
    //           wasm_module_inst_debug->memories[0]->heap_data_end);
    // os_printf("[DEBUG]wasm_module_inst->memories[0]->cur_page_count:   %d\n",
    //           wasm_module_inst_debug->memories[0]->cur_page_count);
    // os_printf("[DEBUG]memories[0]->cur_page_count *
    // memories[0]->num_bytes_per_"
    //           "page:   %d\n",
    //           wasm_module_inst_debug->memories[0]->cur_page_count
    //               * wasm_module_inst_debug->memories[0]->num_bytes_per_page);
    // os_printf(
    //     "[DEBUG]memories[0]->memory_data_end-memories[0]->memory_data: %d\n",
    //     wasm_module_inst_debug->memories[0]->memory_data_end
    //         - wasm_module_inst_debug->memories[0]->memory_data);

    os_printf("memory instance address:%p ~ %p\n",
              wasm_module_inst_debug->memories[0]->memory_data,
              wasm_module_inst_debug->memories[0]->memory_data_end);
    os_printf("    VSS: %d B\n",
              wasm_module_inst_debug->memories[0]->cur_page_count
                  * wasm_module_inst_debug->memories[0]->num_bytes_per_page);
    os_printf("    RSS: %d B\n",
              get_rss(wasm_module_inst_debug->memories[0]->memory_data,
                      wasm_module_inst_debug->memories[0]->memory_data
                          + wasm_module_inst_debug->memories[0]->cur_page_count
                                * wasm_module_inst_debug->memories[0]
                                      ->num_bytes_per_page));
    os_printf("app heap address :%p ~ %p\n",
              wasm_module_inst_debug->memories[0]->heap_data,
              wasm_module_inst_debug->memories[0]->heap_data_end);

#if WASM_ENABLE_AOT != 0
    if (module_inst_common->module_type == Wasm_Module_AoT) {
        AOTModuleInstance *aot_module_inst =
            (AOTModuleInstance *)module_inst_common;
        AOTModule *aot_module = (AOTModule *)aot_module_inst->module;
        module_common = (WASMModuleCommon *)aot_module;
        if (aot_module_inst->memories) {
            AOTMemoryInstance **memories = aot_module_inst->memories;
            heap_handle = memories[0]->heap_handle;
        }
        aot_get_module_inst_mem_consumption(aot_module_inst,
                                            &module_inst_mem_consps);
        aot_get_module_mem_consumption(aot_module, &module_mem_consps);
    }
#endif

    bh_assert(module_common != NULL);

    if (heap_handle) {
        app_heap_peak_size = gc_get_heap_highmark_size(heap_handle);
    }

    /*memory dumpで取れるtotal_size*/
    total_size = offsetof(WASMExecEnv, wasm_stack.s.bottom)
                 + exec_env->wasm_stack_size + module_mem_consps.total_size
                 + module_inst_mem_consps.total_size;

    os_printf("\nMemory consumption summary (bytes):\n");
    wasm_runtime_dump_module_mem_consumption(module_common);
    wasm_runtime_dump_module_inst_mem_consumption(module_inst_common);
    wasm_runtime_dump_exec_env_mem_consumption(exec_env);
    os_printf("\nTotal memory consumption of module, module inst and "
              "exec env: %u\n",
              total_size);
    os_printf("Total interpreter stack used: %u\n",
              exec_env->max_wasm_stack_used);

    if (max_aux_stack_used != (uint32)-1)
        os_printf("Total auxiliary stack used: %u\n", max_aux_stack_used);
    else
        os_printf("Total aux stack used: no enough info to profile\n");

    /*
     * Report the native stack usage estimation.
     *
     * Unlike the aux stack above, we report the amount unused
     * because we don't know the stack "bottom".
     *
     * Note that this is just about what the runtime itself observed.
     * It doesn't cover host func implementations, signal handlers, etc.
     */
    if (exec_env->native_stack_top_min != (void *)UINTPTR_MAX)
        os_printf("Native stack left: %zd\n",
                  exec_env->native_stack_top_min
                      - exec_env->native_stack_boundary);
    else
        os_printf("Native stack left: no enough info to profile\n");

    os_printf("Total app heap used: %u\n", app_heap_peak_size);

    // os_printf("wasm_module_inst->memories[0]->memory_data: %u\n",
    //           module_inst_common->memories[0]->memory_data);
}
#endif /* end of (WASM_ENABLE_MEMORY_PROFILING != 0) \
                 || (WASM_ENABLE_MEMORY_TRACING != 0) */
