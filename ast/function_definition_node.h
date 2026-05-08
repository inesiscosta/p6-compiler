#pragma once

#include <string>
#include <memory>
#include <cdk/ast/typed_node.h>
#include <cdk/ast/sequence_node.h>
#include <cdk/types/basic_type.h>

namespace p6 {

  /**
   * Class for describing function definition nodes.
   */
  class function_definition_node : public cdk::typed_node {
    int _qualifier;
    std::string _name;
    cdk::sequence_node *_params;
    cdk::basic_node *_body;

  public:
    function_definition_node(int lineno, int qualifier, std::shared_ptr<cdk::basic_type> type,
      const std::string &name, cdk::sequence_node *params, cdk::basic_node *body) :
        cdk::typed_node(lineno), _qualifier(qualifier),
        _name(name), _params(params), _body(body) {
      this->type(type);
    }

    int qualifier() { return _qualifier; }

    const std::string &name() { return _name; }

    cdk::sequence_node *params() { return _params; }

    cdk::basic_node *body() { return _body; }

    void accept(basic_ast_visitor *sp, int level) { sp->do_function_definition_node(this, level); }

  };

} // p6
