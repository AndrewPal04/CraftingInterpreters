package com.craftinginterpreters.lox;

import java.util.List;
import java.util.Map;

class LoxClass implements LoxCallable {
    final String name;
    final LoxClass superclass;
    private final Map<String, LoxFunction> methods;
    final LoxMetaclass metaclass;

    LoxClass(String name, LoxClass superclass, Map<String, LoxFunction> methods,
             Map<String, LoxFunction> staticMethods) {
        this.name = name;
        this.superclass = superclass;
        this.methods = methods;
        this.metaclass = new LoxMetaclass(name + " metaclass", staticMethods);
    }

    LoxFunction findMethod(String name) {
        if (methods.containsKey(name)) return methods.get(name);
        if (superclass != null) return superclass.findMethod(name);
        return null;
    }

    LoxFunction findStaticMethod(String name) {
        return metaclass.findMethod(name);
    }

    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        LoxInstance instance = new LoxInstance(this);
        LoxFunction initializer = findMethod("init");
        if (initializer != null) {
            initializer.bind(instance).call(interpreter, arguments);
        }
        return instance;
    }

    @Override
    public int arity() {
        LoxFunction initializer = findMethod("init");
        return initializer == null ? 0 : initializer.arity();
    }

    @Override
    public String toString() { return name; }
}