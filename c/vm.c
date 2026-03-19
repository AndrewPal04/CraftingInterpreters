#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

VM vm;

static void resetStack() {
    vm.stack = NULL;
    vm.stackTop = NULL;
    vm.stackCapacity = 0;
}

void initVM() {
    resetStack();
}

void freeVM() {
    FREE_ARRAY(Value, vm.stack, vm.stackCapacity);
    resetStack();
}

void push(Value value) {
    if (vm.stackTop - vm.stack == vm.stackCapacity) {
        int oldCapacity = vm.stackCapacity;
        int oldTop = (int)(vm.stackTop - vm.stack);
        vm.stackCapacity = GROW_CAPACITY(oldCapacity);
        vm.stack = GROW_ARRAY(Value, vm.stack, oldCapacity, vm.stackCapacity);
        vm.stackTop = vm.stack + oldTop;
    }
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk,
            (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_ADD:      vm.stackTop[-2] += vm.stackTop[-1]; vm.stackTop--; break;
            case OP_SUBTRACT: vm.stackTop[-2] -= vm.stackTop[-1]; vm.stackTop--; break;
            case OP_MULTIPLY: vm.stackTop[-2] *= vm.stackTop[-1]; vm.stackTop--; break;
            case OP_DIVIDE:   vm.stackTop[-2] /= vm.stackTop[-1]; vm.stackTop--; break;
            case OP_NEGATE:   vm.stackTop[-1] = -vm.stackTop[-1]; break;
            case OP_RETURN: {
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(Chunk* chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}