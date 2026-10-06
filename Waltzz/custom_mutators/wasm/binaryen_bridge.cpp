#include "binaryen_bridge.h"

#include <memory>
#include <string>

#include "wasm.h"

using namespace wasm;

namespace {

Module* asModule(BinaryenModuleRef module) {
  return reinterpret_cast<Module*>(module);
}

Expression* asExpression(BinaryenExpressionRef expr) {
  return reinterpret_cast<Expression*>(expr);
}

Name uniqueName(Module* module, const char* prefix, bool memory) {
  uint32_t suffix = 0;
  Name name;
  do {
    name = std::string(prefix) + std::to_string(suffix++);
  } while (memory ? module->getMemoryOrNull(name) != nullptr
                  : module->getDataSegmentOrNull(name) != nullptr);
  return name;
}

template<typename T, typename... Rest>
const char* singleMemoryName(Expression* expr) {
  if (auto* curr = expr->dynCast<T>()) {
    return curr->memory.str.data();
  }
  if constexpr (sizeof...(Rest)) {
    return singleMemoryName<Rest...>(expr);
  }
  return nullptr;
}

const char* memoryName(Expression* expr, uint32_t slot) {
  if (auto* curr = expr->dynCast<MemoryCopy>()) {
    if (slot == 0) return curr->destMemory.str.data();
    return slot == 1 ? curr->sourceMemory.str.data() : nullptr;
  }
  if (slot != 0) return nullptr;
  return singleMemoryName<Load,
                          Store,
                          AtomicRMW,
                          AtomicCmpxchg,
                          AtomicWait,
                          AtomicNotify,
                          SIMDLoad,
                          SIMDLoadStoreLane,
                          MemorySize,
                          MemoryGrow,
                          MemoryInit,
                          MemoryFill>(expr);
}

template<typename T>
bool setSingleMemory(Expression* expr, uint32_t slot, Name name) {
  if (slot != 0) {
    return false;
  }
  if (auto* curr = expr->dynCast<T>()) {
    curr->memory = name;
    return true;
  }
  return false;
}

template<typename T>
bool setTypedMemory(Expression* expr, uint32_t slot, Name name, Type type) {
  if (slot != 0) return false;
  if (auto* curr = expr->dynCast<T>()) {
    curr->memory = name;
    curr->ptrType = type;
    curr->finalize();
    return true;
  }
  return false;
}

}

extern "C" {

BinaryenIndex WaltzzGetNumMemories(BinaryenModuleRef module) {
  auto* wasm = asModule(module);

  return (BinaryenIndex)wasm->memories.size();
}

const char* WaltzzDataSegmentGetNameByIndex(BinaryenModuleRef module, BinaryenIndex index) {
  auto* wasm = asModule(module);

  if(index >= wasm->dataSegments.size()){
    return nullptr;
  }

  auto* segment = wasm->dataSegments[index].get();
  return segment->name.str.data();
}

const char* WaltzzGetMemoryNameByIndex(BinaryenModuleRef module, BinaryenIndex index) {
  auto* wasm = asModule(module);

  if(index >= wasm->memories.size()) {
    return nullptr;
  }

  auto* memory = wasm->memories[index].get();
  return memory->name.str.data();
}

bool WaltzzMemoryNameExists(BinaryenModuleRef module, const char* name) {
  auto* wasm = asModule(module);

  for(const auto& memory : wasm->memories) {
    if(strcmp(memory->name.str.data(), name) == 0) return true;
  }

  return false;
}

const char* WaltzzAddMemory(BinaryenModuleRef module,
                            uint64_t initial,
                            uint64_t maximum,
                            bool has_maximum,
                            bool shared,
                            bool memory64) {
  auto* wasm = asModule(module);
  auto memory = std::make_unique<Memory>();
  memory->name = uniqueName(wasm, "waltzz.memory.", true);
  memory->initial = initial;
  memory->max = has_maximum ? maximum : (shared ? initial : Memory::kUnlimitedSize);
  memory->shared = shared;
  memory->indexType = memory64 ? Type::i64 : Type::i32;

  auto* added = wasm->addMemory(std::move(memory));
  return added->name.str.data();
}

const char* WaltzzAddMemoryLike(BinaryenModuleRef module,
                                BinaryenModuleRef source_module,
                                const char* source_mem_name) {
  auto* source = asModule(source_module)->getMemoryOrNull(Name(source_mem_name));
  if (!source) {
    return nullptr;
  }
  return WaltzzAddMemory(module,
                         source->initial,
                         source->max,
                         source->max != Memory::kUnlimitedSize,
                         source->shared,
                         source->indexType == Type::i64);
}

const char* WaltzzAddPassiveDataSegment(BinaryenModuleRef module,
                                        const char* data,
                                        size_t size) {
  auto* wasm = asModule(module);
  auto segment = std::make_unique<DataSegment>();
  segment->name = uniqueName(wasm, "waltzz.data.", false);
  segment->hasExplicitName = true;
  segment->isPassive = true;
  segment->data.assign(data, data + size);

  auto* added = wasm->addDataSegment(std::move(segment));
  return added->name.str.data();
}

const char* WaltzzExpressionGetMemoryName(BinaryenExpressionRef expr,
                                          uint32_t slot) {
  return expr ? memoryName(asExpression(expr), slot) : nullptr;
}

bool WaltzzExpressionSetMemoryName(BinaryenExpressionRef expr,
                                   uint32_t slot,
                                   BinaryenModuleRef module,
                                   const char* name) {
  if (!expr || !name) {
    return false;
  }

  auto* expression = asExpression(expr);
  Name memory(name);
  auto* target = asModule(module)->getMemoryOrNull(memory);
  if (!target) {
    return false;
  }
  Type indexType = target->indexType;

  if (setSingleMemory<Load>(expression, slot, memory) ||
      setSingleMemory<Store>(expression, slot, memory) ||
      setSingleMemory<AtomicRMW>(expression, slot, memory) ||
      setSingleMemory<AtomicCmpxchg>(expression, slot, memory) ||
      setSingleMemory<AtomicWait>(expression, slot, memory) ||
      setSingleMemory<AtomicNotify>(expression, slot, memory) ||
      setSingleMemory<SIMDLoad>(expression, slot, memory) ||
      setSingleMemory<SIMDLoadStoreLane>(expression, slot, memory) ||
      setSingleMemory<MemoryInit>(expression, slot, memory) ||
      setSingleMemory<MemoryFill>(expression, slot, memory)) {
    return true;
  }

  if (setTypedMemory<MemorySize>(expression, slot, memory, indexType) ||
      setTypedMemory<MemoryGrow>(expression, slot, memory, indexType)) {
    return true;
  }

  if (auto* curr = expression->dynCast<MemoryCopy>()) {
    if (slot == 0) {
      curr->destMemory = memory;
      return true;
    }
    if (slot == 1) {
      curr->sourceMemory = memory;
      return true;
    }
  }

  return false;
}

}
