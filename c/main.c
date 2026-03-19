#include <stdio.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main() {
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    // 1.2 + 3.4 * -5.6
    writeConstant(&chunk, 1.2, 123);
    writeConstant(&chunk, 3.4, 123);
    writeConstant(&chunk, 5.6, 123);
    writeChunk(&chunk, OP_NEGATE, 123);
    writeChunk(&chunk, OP_MULTIPLY, 123);
    writeChunk(&chunk, OP_ADD, 123);
    writeChunk(&chunk, OP_RETURN, 123);

    disassembleChunk(&chunk, "test chunk");
    interpret(&chunk);

    freeChunk(&chunk);
    freeVM();
    return 0;
}
