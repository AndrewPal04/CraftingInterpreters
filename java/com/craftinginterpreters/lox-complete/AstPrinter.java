//> Representing Code ast-printer
package com.craftinginterpreters.lox;
//> omit

import java.util.List;
//< omit

/* Representing Code ast-printer < Statements and State omit
class AstPrinter implements Expr.Visitor<String> {
*/
//> Statements and State omit
class AstPrinter implements com.craftinginterpreters.lox.Expr.Visitor<String>, com.craftinginterpreters.lox.Stmt.Visitor<String> {
//< Statements and State omit
  String print(com.craftinginterpreters.lox.Expr expr) {
    return expr.accept(this);
  }
//> Statements and State omit

  String print(com.craftinginterpreters.lox.Stmt stmt) {
    return stmt.accept(this);
  }
//< Statements and State omit
//> visit-methods
//> Statements and State omit
  @Override
  public String visitBlockStmt(com.craftinginterpreters.lox.Stmt.Block stmt) {
    StringBuilder builder = new StringBuilder();
    builder.append("(block ");

    for (com.craftinginterpreters.lox.Stmt statement : stmt.statements) {
      builder.append(statement.accept(this));
    }

    builder.append(")");
    return builder.toString();
  }
//< Statements and State omit
//> Classes omit

  @Override
  public String visitClassStmt(com.craftinginterpreters.lox.Stmt.Class stmt) {
    StringBuilder builder = new StringBuilder();
    builder.append("(class " + stmt.name.lexeme);
//> Inheritance omit

    if (stmt.superclass != null) {
      builder.append(" < " + print(stmt.superclass));
    }
//< Inheritance omit

    for (com.craftinginterpreters.lox.Stmt.Function method : stmt.methods) {
      builder.append(" " + print(method));
    }

    builder.append(")");
    return builder.toString();
  }
//< Classes omit
//> Statements and State omit

  @Override
  public String visitExpressionStmt(com.craftinginterpreters.lox.Stmt.Expression stmt) {
    return parenthesize(";", stmt.expression);
  }
//< Statements and State omit
//> Functions omit

  @Override
  public String visitFunctionStmt(com.craftinginterpreters.lox.Stmt.Function stmt) {
    StringBuilder builder = new StringBuilder();
    builder.append("(fun " + stmt.name.lexeme + "(");

    for (com.craftinginterpreters.lox.Token param : stmt.params) {
      if (param != stmt.params.get(0)) builder.append(" ");
      builder.append(param.lexeme);
    }

    builder.append(") ");

    for (com.craftinginterpreters.lox.Stmt body : stmt.body) {
      builder.append(body.accept(this));
    }

    builder.append(")");
    return builder.toString();
  }
//< Functions omit
//> Control Flow omit

  @Override
  public String visitIfStmt(com.craftinginterpreters.lox.Stmt.If stmt) {
    if (stmt.elseBranch == null) {
      return parenthesize2("if", stmt.condition, stmt.thenBranch);
    }

    return parenthesize2("if-else", stmt.condition, stmt.thenBranch,
        stmt.elseBranch);
  }
//< Control Flow omit
//> Statements and State omit

  @Override
  public String visitPrintStmt(com.craftinginterpreters.lox.Stmt.Print stmt) {
    return parenthesize("print", stmt.expression);
  }
//< Statements and State omit
//> Functions omit

  @Override
  public String visitReturnStmt(com.craftinginterpreters.lox.Stmt.Return stmt) {
    if (stmt.value == null) return "(return)";
    return parenthesize("return", stmt.value);
  }
//< Functions omit
//> Statements and State omit

  @Override
  public String visitVarStmt(com.craftinginterpreters.lox.Stmt.Var stmt) {
    if (stmt.initializer == null) {
      return parenthesize2("var", stmt.name);
    }

    return parenthesize2("var", stmt.name, "=", stmt.initializer);
  }
//< Statements and State omit
//> Control Flow omit

  @Override
  public String visitWhileStmt(com.craftinginterpreters.lox.Stmt.While stmt) {
    return parenthesize2("while", stmt.condition, stmt.body);
  }
//< Control Flow omit
//> Statements and State omit

  @Override
  public String visitAssignExpr(com.craftinginterpreters.lox.Expr.Assign expr) {
    return parenthesize2("=", expr.name.lexeme, expr.value);
  }
//< Statements and State omit

  @Override
  public String visitBinaryExpr(com.craftinginterpreters.lox.Expr.Binary expr) {
    return parenthesize(expr.operator.lexeme,
                        expr.left, expr.right);
  }
//> Functions omit

  @Override
  public String visitCallExpr(com.craftinginterpreters.lox.Expr.Call expr) {
    return parenthesize2("call", expr.callee, expr.arguments);
  }
//< Functions omit
//> Classes omit

  @Override
  public String visitGetExpr(com.craftinginterpreters.lox.Expr.Get expr) {
    return parenthesize2(".", expr.object, expr.name.lexeme);
  }
//< Classes omit

  @Override
  public String visitGroupingExpr(com.craftinginterpreters.lox.Expr.Grouping expr) {
    return parenthesize("group", expr.expression);
  }

  @Override
  public String visitLiteralExpr(com.craftinginterpreters.lox.Expr.Literal expr) {
    if (expr.value == null) return "nil";
    return expr.value.toString();
  }
//> Control Flow omit

  @Override
  public String visitLogicalExpr(com.craftinginterpreters.lox.Expr.Logical expr) {
    return parenthesize(expr.operator.lexeme, expr.left, expr.right);
  }
//< Control Flow omit
//> Classes omit

  @Override
  public String visitSetExpr(com.craftinginterpreters.lox.Expr.Set expr) {
    return parenthesize2("=",
        expr.object, expr.name.lexeme, expr.value);
  }
//< Classes omit
//> Inheritance omit

  @Override
  public String visitSuperExpr(com.craftinginterpreters.lox.Expr.Super expr) {
    return parenthesize2("super", expr.method);
  }
//< Inheritance omit
//> Classes omit

  @Override
  public String visitThisExpr(com.craftinginterpreters.lox.Expr.This expr) {
    return "this";
  }
//< Classes omit

  @Override
  public String visitUnaryExpr(com.craftinginterpreters.lox.Expr.Unary expr) {
    return parenthesize(expr.operator.lexeme, expr.right);
  }
//> Statements and State omit

  @Override
  public String visitVariableExpr(com.craftinginterpreters.lox.Expr.Variable expr) {
    return expr.name.lexeme;
  }
//< Statements and State omit
//< visit-methods
//> print-utilities
  private String parenthesize(String name, com.craftinginterpreters.lox.Expr... exprs) {
    StringBuilder builder = new StringBuilder();

    builder.append("(").append(name);
    for (com.craftinginterpreters.lox.Expr expr : exprs) {
      builder.append(" ");
      builder.append(expr.accept(this));
    }
    builder.append(")");

    return builder.toString();
  }
//< print-utilities
//> omit
  // Note: AstPrinting other types of syntax trees is not shown in the
  // book, but this is provided here as a reference for those reading
  // the full code.
  private String parenthesize2(String name, Object... parts) {
    StringBuilder builder = new StringBuilder();

    builder.append("(").append(name);
    transform(builder, parts);
    builder.append(")");

    return builder.toString();
  }

  private void transform(StringBuilder builder, Object... parts) {
    for (Object part : parts) {
      builder.append(" ");
      if (part instanceof com.craftinginterpreters.lox.Expr) {
        builder.append(((com.craftinginterpreters.lox.Expr)part).accept(this));
//> Statements and State omit
      } else if (part instanceof com.craftinginterpreters.lox.Stmt) {
        builder.append(((com.craftinginterpreters.lox.Stmt) part).accept(this));
//< Statements and State omit
      } else if (part instanceof com.craftinginterpreters.lox.Token) {
        builder.append(((com.craftinginterpreters.lox.Token) part).lexeme);
      } else if (part instanceof List) {
        transform(builder, ((List) part).toArray());
      } else {
        builder.append(part);
      }
    }
  }
//< omit
/* Representing Code printer-main < Representing Code omit
  public static void main(String[] args) {
    Expr expression = new Expr.Binary(
        new Expr.Unary(
            new Token(TokenType.MINUS, "-", null, 1),
            new Expr.Literal(123)),
        new Token(TokenType.STAR, "*", null, 1),
        new Expr.Grouping(
            new Expr.Literal(45.67)));

    System.out.println(new AstPrinter().print(expression));
  }
*/
}
