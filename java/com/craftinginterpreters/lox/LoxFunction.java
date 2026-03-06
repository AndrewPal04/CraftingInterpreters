package com.craftinginterpreters.lox;

import java.util.List;

class LoxFunction implements LoxCallable {

    private final List<Token> params;
    private final List<Stmt> body;
    private final Environment closure;
    private final boolean isInitializer;
    private final boolean isGetter;

    // Named function declaration
    LoxFunction(Stmt.Function declaration, Environment closure,
                boolean isInitializer, boolean isGetter) {
        this.params = declaration.params;
        this.body = declaration.body;
        this.closure = closure;
        this.isInitializer = isInitializer;
        this.isGetter = isGetter;
    }

    // Anonymous lambda expression
    LoxFunction(Expr.Function declaration, Environment closure) {
        this.params = declaration.params;
        this.body = declaration.body;
        this.closure = closure;
        this.isInitializer = false;
        this.isGetter = false;
    }

    // Bind 'this' to a new environment wrapping the closure
    LoxFunction bind(LoxInstance instance) {
        Environment environment = new Environment(closure);
        environment.define("this", instance);
        // Re-wrap as a Stmt.Function-style LoxFunction with same flags
        return new LoxFunction(this.params, this.body, environment,
                this.isInitializer, this.isGetter);
    }

    // Private constructor used by bind()
    private LoxFunction(List<Token> params, List<Stmt> body,
                        Environment closure, boolean isInitializer,
                        boolean isGetter) {
        this.params = params;
        this.body = body;
        this.closure = closure;
        this.isInitializer = isInitializer;
        this.isGetter = isGetter;
    }

    public boolean isGetter() {
        return isGetter;
    }

    @Override
    public int arity() {
        return params == null ? 0 : params.size();
    }

    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        Environment environment = new Environment(closure);

        if (params != null) {
            for (int i = 0; i < params.size(); i++) {
                environment.define(params.get(i).lexeme, arguments.get(i));
            }
        }

        try {
            interpreter.executeBlock(body, environment);
        } catch (Return returnValue) {
            // If inside init(), always return 'this'
            if (isInitializer) return closure.getThis();
            return returnValue.value;
        }

        if (isInitializer) return closure.getThis();
        return null;
    }

    @Override
    public String toString() {
        return "<fn>";
    }
}