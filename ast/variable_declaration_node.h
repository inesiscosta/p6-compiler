#pragma once

#include <string>
#include <memory>
#include <cdk/ast/typed_node.h>
#include <cdk/ast/expression_node.h>
#include <cdk/types/basic_type.h>

namespace p6 {

  /**
   * Class for describing variable declaration nodes.
   */
  class variable_declaration_node : public cdk::typed_node {
    int _qualifier;
    std::string _name;
    cdk::expression_node *_initializer;

  public:
    variable_declaration_node(int lineno, int qualifier, std::shared_ptr<cdk::basic_type> vartype,
      const std::string &name, cdk::expression_node *initializer) :
        cdk::typed_node(lineno), _qualifier(qualifier),
        _name(name), _initializer(initializer) {
      this->type(vartype);
    }

    int qualifier() { return _qualifier; }

    std::shared_ptr<cdk::basic_type> vartype() { return this->type(); }

    const std::string &name() { return _name; }

    cdk::expression_node *initializer() { return _initializer; }

    void accept(basic_ast_visitor *sp, int level) { sp->do_variable_declaration_node(this, level); }

  };

} // p6
