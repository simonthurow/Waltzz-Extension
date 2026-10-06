#include "afl-fuzz.h"
#include "mutator_func.h"

#include <stdio.h>
#include <stdlib.h>
#include <binaryen-c.h>

wasm_mutator_t *afl_custom_init(afl_state_t *afl, unsigned int seed) {

    srand(seed);

    wasm_mutator_t *data = calloc(1, sizeof(wasm_mutator_t));
    if (!data) {

        perror("afl_custom_init calloc");
        return NULL;

    }

    if ((data->mutated_out_buf = (u8 *)calloc(MAX_FILE, sizeof(char))) == NULL) {

        perror("afl_custom_init calloc");
        return NULL;

    }


    if ((data->name_out_buf = (u8 *)calloc(256, sizeof(char))) == NULL) {

        perror("afl_custom_init calloc");
        return NULL;

    }

    wasm_init(data);
    data->afl = afl;
    return data;

}

int32_t afl_custom_init_trim(wasm_mutator_t *data, uint8_t *buf, size_t buf_size) {

    /* we disable trimming saved testcases that are introduced by
       our custom mutators. */

    return 0;

}

size_t afl_custom_trim(wasm_mutator_t *data, uint8_t **out_buf) {

    return 0;

}

int32_t afl_custom_post_trim(wasm_mutator_t *data, int success) {

    return 0;

}


const char *afl_custom_describe(wasm_mutator_t *data, size_t max_description_len) {

    snprintf((char*)data->name_out_buf, max_description_len, "br:%u,int:%u,opt:%u,cal:%u,rec:%u,spl:%u", 
             data->br_target_cnt, data->interest_cnt, data->operator_cnt,
             data->call_target_cnt, data->recurse_cnt, data->splice_cnt);

    return (const char*)data->name_out_buf;

}

size_t afl_custom_fuzz(wasm_mutator_t *data, uint8_t *buf, size_t buf_size,
                       u8 **out_buf, uint8_t *add_buf,
                       size_t add_buf_size,  // add_buf can be NULL
                       size_t max_size) {

    afl_state_t* afl = data->afl;
    if (!afl->queue_cur->custom) return 0;

    BinaryenFeatures enabled_features = BinaryenFeatureMVP() | BinaryenFeatureBulkMemory() | BinaryenFeatureSignExt() |
                                        BinaryenFeatureMutableGlobals() | BinaryenFeatureNontrappingFPToInt() |
                                        BinaryenFeatureReferenceTypes() | BinaryenFeatureMultivalue() | BinaryenFeatureSIMD128();

#ifndef DISABLE_THREADS
    enabled_features |= BinaryenFeatureAtomics();
#endif

#ifndef DISABLE_MULTI_MEMORY
    enabled_features |= BinaryenFeatureMultiMemory();
#endif

#ifndef DISABLE_RELAXED_SIMD
    enabled_features |= BinaryenFeatureRelaxedSIMD();
#endif

    BinaryenModuleRef wasm_module_1 = BinaryenModuleRead((char*)buf, buf_size);
    BinaryenModuleSetFeatures(wasm_module_1, enabled_features);

    bool use_splice = (add_buf != NULL && rand_below(afl, 2));
    BinaryenModuleRef wasm_module_2 = (use_splice) ? BinaryenModuleRead((char*)add_buf, add_buf_size) : NULL;
    if (use_splice) BinaryenModuleSetFeatures(wasm_module_2, enabled_features);

    u32 stage_cur = 0, stage_max = (u32)((use_splice ? CUSTOM_SPLICE_HAVOC : CUSTOM_HAVOC_CYCLES) * 
                                         afl->queue_cur->perf_score / afl->havoc_div) >> 8;
    if (unlikely(stage_max < HAVOC_MIN)) { stage_max = HAVOC_MIN; }

    data->splice_cnt = 0;
    data->recurse_cnt = 0;
    data->interest_cnt = 0;
    data->operator_cnt = 0;
    data->br_target_cnt = 0;
    data->call_target_cnt = 0;

    list_t fragment_pool;
    memset(&fragment_pool, 0, sizeof(list_t));
    if (use_splice) wasm_fragmentize_module(wasm_module_2, &fragment_pool);
    
    u32 mutation_cnt = 0;
    u32 mutator_cnt = (use_splice) ? 6 : 5;
    for (stage_cur = 0; stage_cur < stage_max; stage_cur += mutation_cnt) {

        switch (rand_below(afl, mutator_cnt)) {
            case 0: 
                mutation_cnt = wasm_call_target_function_mutate(data, wasm_module_1);
                data->call_target_cnt += mutation_cnt;
                break;
            case 1: 
                mutation_cnt = wasm_interest_values_mutate(data, wasm_module_1);
                data->interest_cnt += mutation_cnt;
                break;
            case 2:
                mutation_cnt = wasm_branch_target_mutate(data, wasm_module_1);
                data->br_target_cnt += mutation_cnt;
                break;
            case 3: 
                mutation_cnt = wasm_operators_mutate(data, wasm_module_1);
                data->operator_cnt += mutation_cnt;
                break;
            case 4:
                mutation_cnt = wasm_recursive_mutate(data, wasm_module_1);
                data->recurse_cnt += mutation_cnt;
                break;
            case 5:
                mutation_cnt = wasm_splicing_mutate(data, wasm_module_1, wasm_module_2, &fragment_pool);
                data->splice_cnt += mutation_cnt;
                break;
        }

    }

    BinaryenModuleAllocateAndWriteResult res_module;
    res_module = BinaryenModuleAllocateAndWrite(wasm_module_1, NULL);

    memcpy(data->mutated_out_buf, res_module.binary, res_module.binaryBytes);
    *out_buf = data->mutated_out_buf;

    if (fragment_pool.element_total_count) LIST_FOREACH_CLEAR(&fragment_pool, fragGrp, {free(el->frags); free(el);});
    if (wasm_module_2) BinaryenModuleDispose(wasm_module_2);
    BinaryenModuleDispose(wasm_module_1);
    wasm_reset_name_grp(data);
    free(res_module.binary);

    return res_module.binaryBytes;

}

void afl_custom_deinit(wasm_mutator_t *data) {

    wasm_deinit(data);

    free(data->mutated_out_buf);
    free(data->name_out_buf);
    free(data);

}
