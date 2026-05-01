#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "vm.h"
#include "value.h"

VM vm;

static void resetStack() {
    vm.stackTop      = vm.stack;
    vm.frameCount    = 0;
    vm.openUpvalues  = NULL;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame      = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction    = frame->ip - function->chunk.code - 1;
        int line = getLine(&function->chunk, (int)instruction);
        fprintf(stderr, "[line %d] in ", line);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
    resetStack();
}

static bool clockNative(int argCount, Value* args, Value* result) {
    (void)argCount; (void)args;
    *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    return true;
}

static bool sqrtNative(int argCount, Value* args, Value* result) {
    (void)argCount;
    if (!IS_NUMBER(args[0])) {
        runtimeError("sqrt() argument must be a number.");
        return false;
    }
    double x = AS_NUMBER(args[0]);
    if (x < 0) {
        runtimeError("sqrt() argument must be non-negative.");
        return false;
    }
    *result = NUMBER_VAL(sqrt(x));
    return true;
}

static bool typeNative(int argCount, Value* args, Value* result) {
    (void)argCount;
    Value value = args[0];
    if (IS_NUMBER(value)) {
        *result = OBJ_VAL(copyString("number", 6));
    } else if (IS_BOOL(value)) {
        *result = OBJ_VAL(copyString("bool", 4));
    } else if (IS_NIL(value)) {
        *result = OBJ_VAL(copyString("nil", 3));
    } else if (IS_STRING(value)) {
        *result = OBJ_VAL(copyString("string", 6));
    } else if (IS_CLOSURE(value) || IS_FUNCTION(value)) {
        *result = OBJ_VAL(copyString("function", 8));
    } else if (IS_NATIVE(value)) {
        *result = OBJ_VAL(copyString("native", 6));
    } else if (IS_CLASS(value)) {
        *result = OBJ_VAL(copyString("class", 5));
    } else if (IS_INSTANCE(value)) {
        *result = OBJ_VAL(copyString("instance", 8));
    } else {
        *result = OBJ_VAL(copyString("unknown", 7));
    }
    return true;
}

static bool hasFieldNative(int argCount, Value* args, Value* result) {
    if (argCount != 2) {
        return false;
    }
    if (!IS_INSTANCE(args[0]) || !IS_STRING(args[1])) {
        *result = BOOL_VAL(false);
        return true;
    }
    ObjInstance* instance = AS_INSTANCE(args[0]);
    Value dummy;
    *result = BOOL_VAL(tableGet(&instance->fields, AS_STRING(args[1]), &dummy));
    return true;
}

static bool deleteFieldNative(int argCount, Value* args, Value* result) {
    if (argCount != 2 || !IS_INSTANCE(args[0]) || !IS_STRING(args[1])) {
        *result = BOOL_VAL(false);
        return true;
    }
    ObjInstance* instance = AS_INSTANCE(args[0]);
    bool deleted = tableDelete(&instance->fields, AS_STRING(args[1]));
    *result = BOOL_VAL(deleted);
    return true;
}

static void defineNative(const char* name, NativeFn function, int arity) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function, arity)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM() {
    resetStack();
    vm.objects       = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC        = 1024 * 1024;
    vm.grayCount     = 0;
    vm.grayCapacity  = 0;
    vm.grayStack     = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);
    defineNative("clock",       clockNative,       0);
    defineNative("sqrt",        sqrtNative,        1);
    defineNative("type",        typeNative,        1);
    defineNative("hasField",    hasFieldNative,    2);
    defineNative("deleteField", deleteFieldNative, 2);
    vm.initString = copyString("init", 4);
}

void freeVM() {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
    freeObjects();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.",
                     closure->function->arity, argCount);
        return false;
    }
    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure   = closure;
    frame->ip        = closure->function->chunk.code;
    frame->slots     = vm.stackTop - argCount - 1;
    frame->klass     = NULL;    // <-- ADD
    return true;
}

static bool callValue(Value callee, int argCount);

static bool bindMethod(ObjClass* klass, ObjString* name) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    ObjBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}
static bool callMethod(ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        return false;
    }
    bool result = call(AS_CLOSURE(method), argCount);
    if (result) {
        vm.frames[vm.frameCount - 1].klass = klass;
    }
    return result;
}
static bool invokeFromClass(ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    bool result = call(AS_CLOSURE(method), argCount);
    if (result) {
        vm.frames[vm.frameCount - 1].klass = klass;
    }
    return result;
}

static bool invoke(ObjString* name, int argCount) {
    Value receiver = peek(argCount);
    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
        return false;
    }
    ObjInstance* instance = AS_INSTANCE(receiver);
    Value value;
    if (tableGet(&instance->fields, name, &value)) {
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }
    ObjClass* klass = instance->klass;
    ObjClass* methodClass = NULL;
    Value topMethod;
    ObjClass* cur = klass;
    while (cur != NULL) {
        Value m;
        if (tableGet(&cur->ownMethods, name, &m)) {
            methodClass = cur;
            topMethod = m;
        }
        cur = cur->superclass;
    }
    if (methodClass == NULL) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    bool result = call(AS_CLOSURE(topMethod), argCount);
    if (result) {
        vm.frames[vm.frameCount - 1].klass = methodClass;
    }
    return result;
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
                if (klass->initializer != NULL) {
                    bool result = call(klass->initializer, argCount);
                    if (result) {
                        vm.frames[vm.frameCount - 1].klass = klass;
                    }
                    return result;
                } else if (argCount != 0) {
                    runtimeError("Expected 0 arguments but got %d.", argCount);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_FUNCTION: {
                ObjClosure* temp = newClosure(AS_FUNCTION(callee));
                return call(temp, argCount);
            }
            case OBJ_NATIVE: {
                ObjNative* native = (ObjNative*)AS_OBJ(callee);
                if (argCount != native->arity) {
                    runtimeError("Expected %d arguments but got %d.",
                                 native->arity, argCount);
                    return false;
                }
                Value result;
                if (!native->function(argCount, vm.stackTop - argCount, &result)) {
                    return false;
                }
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default: break;
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue     = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue     = upvalue->next;
    }
    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }
    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next       = upvalue;
    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }
    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    while (vm.openUpvalues != NULL &&
           vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed     = *upvalue->location;
        upvalue->location   = &upvalue->closed;
        vm.openUpvalues     = upvalue->next;
    }
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));
    int length   = a->length + b->length;
    char* chars  = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';
    ObjString* result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (false)

static void defineMethod(ObjString* name) {
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->ownMethods, name, method);
    Value existing;
    if (!tableGet(&klass->methods, name, &existing)) {
        tableSet(&klass->methods, name, method);
    }
    if (name == vm.initString) {
        klass->initializer = AS_CLOSURE(method);
    }
    pop();
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    register uint8_t* ip = frame->ip;

#define READ_BYTE()     (*ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())
#define READ_SHORT() \
    (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(&frame->closure->function->chunk,
            (int)(ip - frame->closure->function->chunk.code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL:   push(NIL_VAL); break;
            case OP_TRUE:  push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP:   pop(); break;

            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(0))) {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjInstance* instance = AS_INSTANCE(peek(0));
                ObjString* name = READ_STRING();
                Value value;
                if (tableGet(&instance->fields, name, &value)) {
                    pop();
                    push(value);
                    break;
                }
                if (!bindMethod(instance->klass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1))) {
                    runtimeError("Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjInstance* instance = AS_INSTANCE(peek(1));
                tableSet(&instance->fields, READ_STRING(), peek(0));
                Value value = pop();
                pop();
                push(value);
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:    BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                frame->ip = ip;
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index   = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            }
            case OP_INHERIT: {
                Value superclass = peek(1);
                if (!IS_CLASS(superclass)) {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass* subclass = AS_CLASS(peek(0));
                subclass->superclass = AS_CLASS(superclass);
                tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
                pop();
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pop());
                if (!bindMethod(superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pop());
                frame->ip = ip;
                if (!invokeFromClass(superclass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_INNER: {
                CallFrame* curFrame = &vm.frames[vm.frameCount - 1];
                ObjClass* klass = curFrame->klass;
                if (klass == NULL) {
                    runtimeError("Can't call 'inner' outside of a method.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjString* name = curFrame->closure->function->name;
                Value receiver = curFrame->slots[0];
                ObjInstance* instance = AS_INSTANCE(receiver);
                ObjClass* subclass = instance->klass;
                if (subclass == klass) {
                    break;
                }
                Value method;
                if (!tableGet(&subclass->ownMethods, name, &method)) {
                    break;
                }
                push(receiver);
                frame->ip = ip;
                bool result = call(AS_CLOSURE(method), 0);
                if (!result) return INTERPRET_RUNTIME_ERROR;
                vm.frames[vm.frameCount - 1].klass = subclass;
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_METHOD: {
                defineMethod(READ_STRING());
                break;
            }
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                frame->ip = ip;
                if (!invoke(method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_RETURN: {
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }
                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_CLASS:
                push(OBJ_VAL(newClass(READ_STRING())));
                break;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef READ_SHORT
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}