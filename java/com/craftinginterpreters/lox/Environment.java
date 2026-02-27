package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.Map;
import java.util.List;
import java.util.ArrayList;

class Environment {
    final Environment enclosing;
    private final Map<String, Object> globals = new HashMap<>();
    private final List<Object> values = new ArrayList<>();


    Environment() {
        enclosing = null;
    }

    Environment(Environment enclosing) {
        this.enclosing = enclosing;
    }

    void define(String name, Object value) {
        if (enclosing == null) {
            globals.put(name, value);
        } else {
            values.add(value);
        }
    }

    Object get(Token name) {
        if (globals.containsKey(name.lexeme)) {
            return globals.get(name.lexeme);
        }

        if (enclosing != null) return enclosing.get(name);

        throw new RuntimeError(name,
                "Undefined variable '" + name.lexeme + "'.");
    }

    void assign(Token name, Object value) {
        if (globals.containsKey(name.lexeme)) {
            globals.put(name.lexeme, value);
            return;
        }

        if (enclosing != null) {
            enclosing.assign(name, value);
            return;
        }

        throw new RuntimeError(name,
                "Undefined variable '" + name.lexeme + "'.");
    }
    private Environment ancestor(int distance) {
        Environment environment = this;
        for (int i = 0; i < distance; i++) {
            environment = environment.enclosing;
        }
        return environment;
    }

    Object getAtSlot(int distance, int slot) {
        return ancestor(distance).values.get(slot);
    }

    void assignAtSlot(int distance, int slot, Object value) {
        ancestor(distance).values.set(slot, value);
    }

}