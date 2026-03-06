package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

class LoxMetaclass implements LoxCallable {
    final String name;
    private final Map<String, LoxFunction> staticMethods;

    LoxMetaclass(String name, Map<String, LoxFunction> staticMethods) {
        this.name = name;
        this.staticMethods = staticMethods;
    }

    LoxFunction findMethod(String name) {
        return staticMethods.getOrDefault(name, null);
    }

    @Override
    public int arity() { return 0; }

    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) { return null; }

    @Override
    public String toString() { return name; }
}