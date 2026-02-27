package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.Map;
import java.util.Stack;
import java.util.List;

class Resolver implements Expr.Visitor<Void>, Stmt.Visitor<Void> {

    private final Interpreter interpreter;
    private final Stack<Map<String, VariableInfo>> scopes = new Stack<>();

    private static class VariableInfo {
        boolean defined;
        boolean used;

        VariableInfo(boolean defined, boolean used) {
            this.defined = defined;
            this.used = used;
        }
    }
    private enum FunctionType {
        NONE,
        FUNCTION
    }

    private FunctionType currentFunction = FunctionType.NONE;
    Resolver(Interpreter interpreter) {
        this.interpreter = interpreter;
    }

    void resolve(List<Stmt> statements) {
        for (Stmt statement : statements) {
            resolve(statement);
        }
    }

    private void resolve(Stmt stmt) {
        stmt.accept(this);
    }

    private void resolve(Expr expr) {
        expr.accept(this);
    }

    private void beginScope() {
        scopes.push(new HashMap<>());
    }

    private void endScope() {
        Map<String, VariableInfo> scope = scopes.pop();

        for (Map.Entry<String, VariableInfo> entry : scope.entrySet()) {
            String name = entry.getKey();
            VariableInfo info = entry.getValue();

            if (info.defined && !info.used) {
                Lox.error(0, "Local variable '" + name + "' is declared but never used.");
            }
        }
    }
    private void declare(Token name) {
        if (scopes.isEmpty()) return;

        Map<String, VariableInfo> scope = scopes.peek();

        if (scope.containsKey(name.lexeme)) {
            Lox.error(name, "Already a variable with this name in this scope.");
        }

        scope.put(name.lexeme, new VariableInfo(false, false));
    }

    private void define(Token name) {
        if (scopes.isEmpty()) return;
        scopes.peek().get(name.lexeme).defined = true;
    }
    private void resolveLocal(Expr expr, Token name) {
        for (int i = scopes.size() - 1; i >= 0; i--) {
            if (scopes.get(i).containsKey(name.lexeme)) {
                interpreter.resolve(expr, scopes.size() - 1 - i);
                return;
            }
        }
    }
    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        resolve(stmt.condition);
        resolve(stmt.thenBranch);
        if (stmt.elseBranch != null) resolve(stmt.elseBranch);
        return null;
    }

    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        resolve(stmt.condition);
        resolve(stmt.body);
        return null;
    }

    @Override
    public Void visitBreakStmt(Stmt.Break stmt) {
        return null;
    }

    @Override
    public Void visitBinaryExpr(Expr.Binary expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitUnaryExpr(Expr.Unary expr) {
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitGroupingExpr(Expr.Grouping expr) {
        resolve(expr.expression);
        return null;
    }

    @Override
    public Void visitLiteralExpr(Expr.Literal expr) {
        return null;
    }

    @Override
    public Void visitLogicalExpr(Expr.Logical expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitCallExpr(Expr.Call expr) {
        resolve(expr.callee);
        for (Expr argument : expr.arguments) {
            resolve(argument);
        }
        return null;
    }

    @Override
    public Void visitFunctionExpr(Expr.Function expr) {
        beginScope();
        for (Token param : expr.params) {
            declare(param);
            define(param);
        }
        resolve(expr.body);
        endScope();
        return null;
    }

    @Override
    public Void visitCommaExpr(Expr.Comma expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitConditionalExpr(Expr.Conditional expr) {
        resolve(expr.condition);
        resolve(expr.thenBranch);
        resolve(expr.elseBranch);
        return null;
    }
    @Override
    public Void visitVariableExpr(Expr.Variable expr) {
        if (!scopes.isEmpty()) {
            VariableInfo info = scopes.peek().get(expr.name.lexeme);
            if (info != null && !info.defined) {
                Lox.error(expr.name, "Can't read local variable in its own initializer.");
            }
        }

        resolveLocal(expr, expr.name);
        markUsed(expr.name);
        return null;
    }
    private void markUsed(Token name) {
        for (int i = scopes.size() - 1; i >= 0; i--) {
            VariableInfo info = scopes.get(i).get(name.lexeme);
            if (info != null) {
                info.used = true;
                return;
            }
        }
    }
    @Override
    public Void visitAssignExpr(Expr.Assign expr) {
        resolve(expr.value);
        resolveLocal(expr, expr.name);
        return null;
    }
    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        if (currentFunction == FunctionType.NONE) {
            Lox.error(stmt.keyword,
                    "Can't return from top-level code.");
        }

        if (stmt.value != null) {
            resolve(stmt.value);
        }

        return null;
    }
    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        declare(stmt.name);
        define(stmt.name);

        resolveFunction(stmt, FunctionType.FUNCTION);
        return null;
    }
    private void resolveFunction(Stmt.Function function,
                                 FunctionType type) {
        FunctionType enclosingFunction = currentFunction;
        currentFunction = type;

        beginScope();
        for (Token param : function.params) {
            declare(param);
            define(param);
        }
        resolve(function.body);
        endScope();

        currentFunction = enclosingFunction;
    }
    @Override
    public Void visitVarStmt(Stmt.Var stmt) {
        declare(stmt.name);

        if (stmt.initializer != null) {
            resolve(stmt.initializer);
        }

        define(stmt.name);
        return null;
    }
    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        beginScope();
        resolve(stmt.statements);
        endScope();
        return null;
    }
}