#pragma once

#include <string>
#include <memory>
#include <cdk/ast/typed_node.h>
#include <cdk/ast/sequence_node.h>
#include <cdk/types/basic_type.h>

namespace p6 {

  /**
   * Class for describing function declaration nodes.
   */
  class function_declaration_node : public cdk::typed_node {
    int _qualifier;
    std::string _name;
    cdk::sequence_node *_params;

  public:
    function_declaration_node(int lineno, int qualifier, std::shared_ptr<cdk::basic_type> type,
      const std::string &name, cdk::sequence_node *params) :
        cdk::typed_node(lineno), _qualifier(qualifier),
        _name(name), _params(params) {
      this->type(type);
    }

    int qualifier() { return _qualifier; }

    const std::string &name() { return _name; }

    cdk::sequence_node *params() { return _params; }

    void accept(basic_ast_visitor *sp, int level) { sp->do_function_declaration_node(this, level); }

  };

} // p6
