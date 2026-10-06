#ifndef WALTZZ_BINARYEN_BRIDGE_H
#define WALTZZ_BINARYEN_BRIDGE_H

#include <binaryen-c.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

BinaryenIndex WaltzzGetNumMemories(BinaryenModuleRef module);

const char* WaltzzDataSegmentGetNameByIndex(BinaryenModuleRef module, BinaryenIndex index);

const char* WaltzzGetMemoryNameByIndex(BinaryenModuleRef module, BinaryenIndex index);

bool WaltzzMemoryNameExists(BinaryenModuleRef module, const char* name);

const char* WaltzzAddMemory(BinaryenModuleRef module,
                            uint64_t initial,
                            uint64_t maximum,
                            bool has_maximum,
                            bool shared,
                            bool memory64);

const char* WaltzzAddMemoryLike(BinaryenModuleRef module,
                                BinaryenModuleRef source_module,
                                const char* source_mem_name);

const char* WaltzzAddPassiveDataSegment(BinaryenModuleRef module,
                                        const char* data,
                                        size_t size);

const char* WaltzzExpressionGetMemoryName(BinaryenExpressionRef expr,
                                          uint32_t slot);

bool WaltzzExpressionSetMemoryName(BinaryenExpressionRef expr,
                                   uint32_t slot,
                                   BinaryenModuleRef module,
                                   const char* name);

#ifdef __cplusplus
}
#endif

#endif
