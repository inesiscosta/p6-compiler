#pragma once

#include "targets/basic_ast_visitor.h"

namespace p6 {

  struct semantic_context {
    std::shared_ptr<cdk::basic_type> current_return_type = nullptr;
    int loop_depth = 0;
    std::shared_ptr<cdk::basic_type> expected_type = nullptr;
  };

  /**
   * Check semantic validity and assign types to AST expression nodes.
   */
  class type_checker : public basic_ast_visitor {
    cdk::symbol_table<p6::symbol> &_symtab;

    basic_ast_visitor *_parent;

    std::shared_ptr<cdk::basic_type> _currentReturnType = nullptr;
    int _inLoop = 0;
    std::shared_ptr<cdk::basic_type> _expectedType = nullptr;

  public:
    type_checker(std::shared_ptr<cdk::compiler> compiler, cdk::symbol_table<p6::symbol> &symtab,
                 basic_ast_visitor *parent, semantic_context context = {})
        : basic_ast_visitor(compiler), _symtab(symtab), _parent(parent),
          _currentReturnType(context.current_return_type), _inLoop(context.loop_depth),
          _expectedType(context.expected_type) {}

  public:
    ~type_checker() { os().flush(); }

  protected:
    void processUnaryExpression(cdk::unary_operation_node *const node, int lvl);
    template <typename T> void process_literal(cdk::literal_node<T> *const node, int lvl) {}

  public:
    // do not edit these lines
#define __IN_VISITOR_HEADER__
#include ".auto/visitor_decls.h" // automatically generated
#undef __IN_VISITOR_HEADER__
    // do not edit these lines: end
  };

} // p6

//---------------------------------------------------------------------------
//     HELPER MACRO FOR TYPE CHECKING
//---------------------------------------------------------------------------

#define CHECK_TYPES(compiler, symtab, node, context)                                               \
  {                                                                                                \
    try {                                                                                          \
      p6::type_checker checker(compiler, symtab, this, context);                                   \
      (node)->accept(&checker, 0);                                                                 \
    } catch (const std::string &problem) {                                                         \
      std::cerr << (node)->lineno() << ": " << problem << std::endl;                               \
      this->set_semantic_error();                                                                  \
      return;                                                                                      \
    }                                                                                              \
  }

#define ASSERT_SAFE_EXPRESSIONS CHECK_TYPES(_compiler, _symtab, node, semantic_context())
