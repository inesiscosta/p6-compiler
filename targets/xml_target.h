#pragma once

#include "targets/type_checker.h"
#include "targets/xml_writer.h"
#include <cdk/ast/basic_node.h>
#include <cdk/targets/basic_target.h>

namespace p6 {

  class xml_target : public cdk::basic_target {
    static xml_target _self;

  private:
    xml_target() : cdk::basic_target("xml") {}

  public:
    bool evaluate(std::shared_ptr<cdk::compiler> compiler) {
      // this symbol table will be used to check identifiers
      // an exception will be thrown if identifiers are used before declaration
      cdk::symbol_table<p6::symbol> symtab;

      // Type-check before traversal for debugging
      try {
        type_checker checker(compiler, symtab, nullptr);
        compiler->ast()->accept(&checker, 0);
      } catch (const std::string &problem) {
        std::cerr << problem << std::endl;
        return false;
      }

      xml_writer writer(compiler, symtab);
      compiler->ast()->accept(&writer, 0);
      return true;
    }
  };

} // namespace p6
