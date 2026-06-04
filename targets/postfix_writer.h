#pragma once

#include "targets/type_checker.h"

#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <cdk/emitters/basic_postfix_emitter.h>
#include <cdk/ast/basic_node.h>

namespace p6 {

  //!
  //! Traverse syntax tree and generate the corresponding assembly code.
  //!
  class postfix_writer: public basic_ast_visitor {
    cdk::symbol_table<p6::symbol> &_symtab;
    cdk::basic_postfix_emitter &_pf;
    int _lbl;

    // State for local variable offset allocation
    bool _allocating_offsets;
    int _local_offset;
    std::unordered_map<p6::variable_declaration_node*, int> _declOffsets;

    // Vectors for loop control labels (stop and next multilevel logic)
    std::vector<std::string> _loopStopLabels;
    std::vector<std::string> _loopNextLabels;

    // Return type of the function being compiled
    std::shared_ptr<cdk::basic_type> _currentReturnType;

    // Flag indicating if compiling the main program block
    bool _inMainProgram;

    // Set of defined functions to avoid duplicate extern declarations
    bool _scanned_functions;
    std::unordered_set<std::string> _definedFunctions;
    void scan_defined_functions(cdk::basic_node *node);

  public:
    postfix_writer(std::shared_ptr<cdk::compiler> compiler, cdk::symbol_table<p6::symbol> &symtab,
                   cdk::basic_postfix_emitter &pf) :
        basic_ast_visitor(compiler), _symtab(symtab), _pf(pf), _lbl(0),
        _allocating_offsets(false), _local_offset(0), _currentReturnType(nullptr), _inMainProgram(false),
        _scanned_functions(false) {
    }

    p6::semantic_context semantic_context() const {
      return {_currentReturnType, static_cast<int>(_loopStopLabels.size()), nullptr};
    }

  public:
    ~postfix_writer() {
      os().flush();
    }

  private:
    /** Method used to generate sequential labels. */
    inline std::string mklbl(int lbl) {
      std::ostringstream oss;
      if (lbl < 0)
        oss << ".L" << -lbl;
      else
        oss << "_L" << lbl;
      return std::move(oss).str();
    }

    void promote(std::shared_ptr<cdk::basic_type> from, std::shared_ptr<cdk::basic_type> to);
    void emit_comparison(int op, std::shared_ptr<cdk::basic_type> operand_type);
    void emit_static_initializer(cdk::expression_node *init, std::shared_ptr<cdk::basic_type> type, bool sign_neg = false);
    std::shared_ptr<cdk::basic_type> get_common_type(std::shared_ptr<cdk::basic_type> lt, std::shared_ptr<cdk::basic_type> rt);


  public:
  // do not edit these lines
#define __IN_VISITOR_HEADER__
#include ".auto/visitor_decls.h"       // automatically generated
#undef __IN_VISITOR_HEADER__
  // do not edit these lines: end

  };

} // p6
