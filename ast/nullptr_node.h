#pragma once

#include <cdk/ast/expression_node.h>

namespace p6 {

  /**
   * Class for describing null pointer literal nodes.
   */
  class nullptr_node : public cdk::expression_node {

  public:
    nullptr_node(int lineno) :
        cdk::expression_node(lineno) {
    }

    void accept(basic_ast_visitor *sp, int level) { sp->do_nullptr_node(this, level); }

  };

} // p6
