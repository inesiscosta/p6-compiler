#pragma once

#include <cdk/ast/expression_node.h>

namespace p6 {

  /**
   * Class for describing the interactive built-ins: key(ms), clock and random(n).
   * The kind is the keyword's token (tKEY, tCLOCK or tRANDOM); clock has no argument.
   */
  class builtin_node : public cdk::expression_node {
    int _kind;
    cdk::expression_node *_argument;

  public:
    builtin_node(int lineno, int kind, cdk::expression_node *argument = nullptr) :
        cdk::expression_node(lineno), _kind(kind), _argument(argument) {
    }

    int kind() const { return _kind; }
    cdk::expression_node *argument() { return _argument; }

    void accept(basic_ast_visitor *sp, int level) { sp->do_builtin_node(this, level); }

  };

} // p6
