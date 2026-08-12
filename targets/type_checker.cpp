#include "targets/type_checker.h"

#include <cdk/types/types.h>

#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include ".auto/all_nodes.h" // automatically generated
#include "p6_parser.tab.h"
#include "targets/symbol.h"

#define ASSERT_UNSPEC                                                                              \
  {                                                                                                \
    if (node->type() != nullptr && !node->is_typed(cdk::TYPE_UNSPEC))                              \
      return;                                                                                      \
  }

//---------------------------------------------------------------------------
// HELPER FOR IMPLICIT TYPE PROMOTIONS
//---------------------------------------------------------------------------

static bool can_promote(std::shared_ptr<cdk::basic_type> from,
                        std::shared_ptr<cdk::basic_type> to) {
  if (!from || !to)
    return false;

  if (from->name() == to->name()) {
    // Pointers
    if (from->name() == cdk::TYPE_POINTER) {
      auto pfrom = std::dynamic_pointer_cast<cdk::reference_type>(from);
      auto pto = std::dynamic_pointer_cast<cdk::reference_type>(to);

      // Void <-> Any
      if (pfrom->referenced()->name() == cdk::TYPE_VOID ||
          pto->referenced()->name() == cdk::TYPE_VOID)
        return true;

      // Same referenced type
      return pfrom->referenced()->to_string() == pto->referenced()->to_string();
    }
    return true;
  }

  // Balanced -> Takum
  if (from->name() == cdk::TYPE_BALANCED3 && to->name() == cdk::TYPE_TAKUM3)
    return true;
  return false;
}

static bool is_integer(std::shared_ptr<cdk::basic_type> type) {
  return type && type->name() == cdk::TYPE_BALANCED3;
}

static bool is_real(std::shared_ptr<cdk::basic_type> type) {
  return type && type->name() == cdk::TYPE_TAKUM3;
}

static bool is_numeric(std::shared_ptr<cdk::basic_type> type) {
  return is_integer(type) || is_real(type);
}

static bool is_literal_initializer(cdk::expression_node *init) {
  if (dynamic_cast<cdk::balanced3_node *>(init) ||
      dynamic_cast<cdk::takum3_node *>(init) ||
      dynamic_cast<cdk::string_node *>(init) ||
      dynamic_cast<p6::nullptr_node *>(init)) {
    return true;
  }

  if (auto unary = dynamic_cast<cdk::unary_operation_node *>(init)) {
    if (dynamic_cast<cdk::unary_minus_node *>(unary) ||
        dynamic_cast<cdk::unary_plus_node *>(unary)) {
      return is_literal_initializer(unary->argument());
    }
  }

  return false;
}

static bool is_block_terminator(cdk::basic_node *node) {
  return dynamic_cast<p6::stop_node *>(node) ||
         dynamic_cast<p6::next_node *>(node) ||
         dynamic_cast<p6::return_node *>(node);
}

static void check_block_terminators(cdk::sequence_node *instructions) {
  if (!instructions)
    return;

  for (size_t i = 0; i + 1 < instructions->size(); i++) {
    if (is_block_terminator(instructions->node(i))) {
      throw std::string("stop, next, and return must be the last instruction in their block");
    }
  }
}

void p6::type_checker::do_sequence_node(cdk::sequence_node *const node, int lvl) {
  for (size_t i = 0; i < node->size(); i++) {
    if (node->node(i))
      node->node(i)->accept(this, lvl);
  }
}

//---------------------------------------------------------------------------

void p6::type_checker::do_nil_node(cdk::nil_node *const node, int lvl) {
  // EMPTY
}

void p6::type_checker::do_data_node(cdk::data_node *const node, int lvl) {
  // EMPTY
}

void p6::type_checker::do_double_node(cdk::double_node *const node, int lvl) {
  // EMPTY
}

void p6::type_checker::do_not_node(cdk::not_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->argument()->accept(this, lvl + 2);
  if (!is_integer(node->argument()->type()))
    throw std::string("wrong type in argument of logical NOT operator");
  node->type(cdk::balanced3_type::create());
}

void p6::type_checker::do_and_node(cdk::and_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->left()->accept(this, lvl + 2);
  if (!is_integer(node->left()->type()))
    throw std::string("wrong type in left argument of logical AND operator");

  node->right()->accept(this, lvl + 2);
  if (!is_integer(node->right()->type()))
    throw std::string("wrong type in right argument of logical AND operator");

  node->type(cdk::balanced3_type::create());
}

void p6::type_checker::do_or_node(cdk::or_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->left()->accept(this, lvl + 2);
  if (!is_integer(node->left()->type()))
    throw std::string("wrong type in left argument of logical OR operator");

  node->right()->accept(this, lvl + 2);
  if (!is_integer(node->right()->type()))
    throw std::string("wrong type in right argument of logical OR operator");

  node->type(cdk::balanced3_type::create());
}

//---------------------------------------------------------------------------

void p6::type_checker::do_integer_node(cdk::integer_node *const node, int lvl) {
  // EMPTY
}

void p6::type_checker::do_string_node(cdk::string_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->type(cdk::primitive_type::create(4, cdk::TYPE_STRING));
}

//---------------------------------------------------------------------------

void p6::type_checker::processUnaryExpression(cdk::unary_operation_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->argument()->accept(this, lvl + 2);
  if (node->argument()->is_typed(cdk::TYPE_BALANCED3)) {
    node->type(cdk::balanced3_type::create());
  } else if (node->argument()->is_typed(cdk::TYPE_TAKUM3)) {
    node->type(cdk::takum3_type::create());
  } else {
    throw std::string("wrong type in argument of unary expression");
  }
}

void p6::type_checker::do_unary_minus_node(cdk::unary_minus_node *const node, int lvl) {
  processUnaryExpression(node, lvl);
}

void p6::type_checker::do_unary_plus_node(cdk::unary_plus_node *const node, int lvl) {
  processUnaryExpression(node, lvl);
}

//---------------------------------------------------------------------------

static std::shared_ptr<cdk::basic_type> numeric_result_type(std::shared_ptr<cdk::basic_type> lt,
                                                            std::shared_ptr<cdk::basic_type> rt,
                                                            const std::string &op) {
  if (!is_numeric(lt) || !is_numeric(rt))
    throw std::string("invalid types in " + op);

  if (is_real(lt) || is_real(rt))
    return cdk::takum3_type::create();

  return cdk::balanced3_type::create();
}

void p6::type_checker::do_add_node(cdk::add_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->left()->accept(this, lvl + 2);
  node->right()->accept(this, lvl + 2);

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  if (lt->name() == cdk::TYPE_POINTER && is_integer(rt)) {
    // Pointer + Offset
    node->type(lt);
  } else if (is_integer(lt) && rt->name() == cdk::TYPE_POINTER) {
    // Offset + Pointer
    node->type(rt);
  } else {
    node->type(numeric_result_type(lt, rt, "addition"));
  }
}

void p6::type_checker::do_sub_node(cdk::sub_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->left()->accept(this, lvl + 2);
  node->right()->accept(this, lvl + 2);

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  if (lt->name() == cdk::TYPE_POINTER && is_integer(rt)) {
    // Pointer - Offset
    node->type(lt);
  } else if (lt->name() == cdk::TYPE_POINTER && rt->name() == cdk::TYPE_POINTER) {
    // Pointer - Pointer
    auto p1 = std::dynamic_pointer_cast<cdk::reference_type>(lt);
    auto p2 = std::dynamic_pointer_cast<cdk::reference_type>(rt);
    if (p1->referenced()->name() == cdk::TYPE_VOID || p2->referenced()->name() == cdk::TYPE_VOID ||
        p1->referenced()->to_string() == p2->referenced()->to_string()) {
      // Void Pointer or Same Pointer Type
      node->type(cdk::balanced3_type::create());
    } else {
      throw std::string("invalid pointer types in subtraction");
    }
  } else {
    node->type(numeric_result_type(lt, rt, "subtraction"));
  }
}

void p6::type_checker::do_mul_node(cdk::mul_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->left()->accept(this, lvl + 2);
  node->right()->accept(this, lvl + 2);

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  node->type(numeric_result_type(lt, rt, "multiplication"));
}

void p6::type_checker::do_div_node(cdk::div_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->left()->accept(this, lvl + 2);
  node->right()->accept(this, lvl + 2);

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  node->type(numeric_result_type(lt, rt, "division"));
}

void p6::type_checker::do_mod_node(cdk::mod_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->left()->accept(this, lvl + 2);
  node->right()->accept(this, lvl + 2);

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  if (is_integer(lt) && is_integer(rt)) {
    node->type(cdk::balanced3_type::create());
  } else {
    throw std::string("invalid types in modulo");
  }
}

//---------------------------------------------------------------------------
// COMPARISON OPERATORS HELPER
//---------------------------------------------------------------------------

static void process_order_comparison(p6::type_checker *tc, cdk::binary_operation_node *const node,
                                     int lvl) {
  node->left()->accept(tc, lvl + 2);
  node->right()->accept(tc, lvl + 2);

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  if (is_numeric(lt) && is_numeric(rt)) {
    node->type(cdk::balanced3_type::create());
  } else {
    throw std::string("invalid types in comparison");
  }
}

static void process_equality_comparison(p6::type_checker *tc,
                                        cdk::binary_operation_node *const node, int lvl) {
  node->left()->accept(tc, lvl + 2);
  node->right()->accept(tc, lvl + 2);

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  if (lt->name() == cdk::TYPE_POINTER && rt->name() == cdk::TYPE_POINTER) {
    // Pointers
    auto p1 = std::dynamic_pointer_cast<cdk::reference_type>(lt);
    auto p2 = std::dynamic_pointer_cast<cdk::reference_type>(rt);
    if (p1->referenced()->name() == cdk::TYPE_VOID || p2->referenced()->name() == cdk::TYPE_VOID ||
        p1->referenced()->to_string() == p2->referenced()->to_string()) {
      node->type(cdk::balanced3_type::create());
    } else {
      throw std::string("invalid pointer types in comparison");
    }
  } else if (is_numeric(lt) && is_numeric(rt)) {
    // Numerics
    node->type(cdk::balanced3_type::create());
  } else {
    throw std::string("invalid types in comparison");
  }
}

void p6::type_checker::do_lt_node(cdk::lt_node *const node, int lvl) {
  ASSERT_UNSPEC;
  process_order_comparison(this, node, lvl);
}
void p6::type_checker::do_le_node(cdk::le_node *const node, int lvl) {
  ASSERT_UNSPEC;
  process_order_comparison(this, node, lvl);
}
void p6::type_checker::do_ge_node(cdk::ge_node *const node, int lvl) {
  ASSERT_UNSPEC;
  process_order_comparison(this, node, lvl);
}
void p6::type_checker::do_gt_node(cdk::gt_node *const node, int lvl) {
  ASSERT_UNSPEC;
  process_order_comparison(this, node, lvl);
}
void p6::type_checker::do_ne_node(cdk::ne_node *const node, int lvl) {
  ASSERT_UNSPEC;
  process_equality_comparison(this, node, lvl);
}
void p6::type_checker::do_eq_node(cdk::eq_node *const node, int lvl) {
  ASSERT_UNSPEC;
  process_equality_comparison(this, node, lvl);
}

//---------------------------------------------------------------------------

void p6::type_checker::do_variable_node(cdk::variable_node *const node, int lvl) {
  ASSERT_UNSPEC;
  const std::string &id = node->name();
  std::shared_ptr<p6::symbol> symbol = _symtab.find(id);

  if (!symbol)
    throw std::string("undeclared variable '" + id + "'");

  node->type(symbol->type());
}

void p6::type_checker::do_rvalue_node(cdk::rvalue_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->lvalue()->accept(this, lvl);
  node->type(node->lvalue()->type());
}

void p6::type_checker::do_assignment_node(cdk::assignment_node *const node, int lvl) {
  ASSERT_UNSPEC;

  node->lvalue()->accept(this, lvl);
  auto lt = node->lvalue()->type();

  _expectedType = lt;
  node->rvalue()->accept(this, lvl + 2);
  _expectedType = nullptr;

  auto rt = node->rvalue()->type();

  if (can_promote(rt, lt)) {
    node->type(lt);
  } else {
    throw std::string("type mismatch in assignment");
  }
}

//---------------------------------------------------------------------------

void p6::type_checker::do_program_node(p6::program_node *const node, int lvl) {
  _currentReturnType = cdk::balanced3_type::create();
  node->statements()->accept(this, lvl + 2);
  _currentReturnType = nullptr;
}

void p6::type_checker::do_evaluation_node(p6::evaluation_node *const node, int lvl) {
  node->argument()->accept(this, lvl + 2);
}

void p6::type_checker::do_print_node(p6::print_node *const node, int lvl) {
  if (node->arguments()) {
    node->arguments()->accept(this, lvl + 2);
    for (size_t i = 0; i < node->arguments()->size(); i++) {
      auto arg = dynamic_cast<cdk::expression_node *>(node->arguments()->node(i));
      if (arg->is_typed(cdk::TYPE_POINTER))
        throw std::string("cannot print pointer expression");
    }
  }
}

//---------------------------------------------------------------------------

void p6::type_checker::do_input_node(p6::input_node *const node, int lvl) {
  ASSERT_UNSPEC;
  if (_expectedType) {
    if (!is_numeric(_expectedType))
      throw std::string("input only accepts Integer or Real types");
    node->type(_expectedType);
  } else {
    node->type(cdk::balanced3_type::create());
  }
}

//---------------------------------------------------------------------------

void p6::type_checker::do_while_node(p6::while_node *const node, int lvl) {
  node->condition()->accept(this, lvl + 4);
  if (!is_integer(node->condition()->type())) {
    throw std::string("while condition must be an integer");
  }
  _inLoop++;
  node->block()->accept(this, lvl + 4);
  _inLoop--;
}

//---------------------------------------------------------------------------

void p6::type_checker::do_if_node(p6::if_node *const node, int lvl) {
  node->condition()->accept(this, lvl + 4);
  if (!is_integer(node->condition()->type())) {
    throw std::string("if condition must be an integer");
  }
  node->block()->accept(this, lvl + 4);
}

void p6::type_checker::do_if_else_node(p6::if_else_node *const node, int lvl) {
  node->condition()->accept(this, lvl + 4);
  if (!is_integer(node->condition()->type())) {
    throw std::string("if condition must be an integer");
  }
  node->thenblock()->accept(this, lvl + 4);
  node->elseblock()->accept(this, lvl + 4);
}

//---------------------------------------------------------------------------

void p6::type_checker::do_block_node(p6::block_node *const node, int lvl) {
  _symtab.push();
  if (node->declarations())
    node->declarations()->accept(this, lvl + 2);
  check_block_terminators(node->instructions());
  if (node->instructions())
    node->instructions()->accept(this, lvl + 2);
  _symtab.pop();
}

void p6::type_checker::do_return_node(p6::return_node *const node, int lvl) {
  if (!_currentReturnType)
    throw std::string("return statement outside function");

  if (node->retval()) {
    node->retval()->accept(this, lvl + 2);
    if (_currentReturnType->name() == cdk::TYPE_VOID)
      throw std::string("cannot return a value from a void function");

    if (!can_promote(node->retval()->type(), _currentReturnType))
      throw std::string("return type mismatch");
  } else {
    if (_currentReturnType->name() != cdk::TYPE_VOID)
      throw std::string("non-void function must return a value");
  }
}

void p6::type_checker::do_stop_node(p6::stop_node *const node, int lvl) {
  if (node->level() <= 0)
    throw std::string("stop level must be positive");
  if (_inLoop < node->level())
    throw std::string("stop level exceeds loop nesting level");
}

void p6::type_checker::do_next_node(p6::next_node *const node, int lvl) {
  if (node->level() <= 0)
    throw std::string("next level must be positive");
  if (_inLoop < node->level())
    throw std::string("next level exceeds loop nesting level");
}

//---------------------------------------------------------------------------

void p6::type_checker::do_nullptr_node(p6::nullptr_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->type(cdk::reference_type::create(4, cdk::primitive_type::create(0, cdk::TYPE_VOID)));
}

void p6::type_checker::do_address_of_node(p6::address_of_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->lvalue()->accept(this, lvl + 2);
  node->type(cdk::reference_type::create(4, node->lvalue()->type()));
}

void p6::type_checker::do_index_node(p6::index_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->base()->accept(this, lvl + 2);
  if (node->base()->type()->name() != cdk::TYPE_POINTER)
    throw std::string("indexing requires pointer base");

  node->index()->accept(this, lvl + 2);
  if (!is_integer(node->index()->type()))
    throw std::string("index must be an integer");

  auto ref = std::dynamic_pointer_cast<cdk::reference_type>(node->base()->type());
  if (ref->referenced()->name() == cdk::TYPE_VOID)
    throw std::string("cannot index void pointer");

  node->type(ref->referenced());
}

void p6::type_checker::do_stack_alloc_node(p6::stack_alloc_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->argument()->accept(this, lvl + 2);
  if (!is_integer(node->argument()->type()))
    throw std::string("stack allocation size must be an integer");

  if (!_expectedType || _expectedType->name() != cdk::TYPE_POINTER)
    throw std::string("stack allocation requires pointer context");

  node->type(_expectedType);
}

void p6::type_checker::do_sizeof_node(p6::sizeof_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->argument()->accept(this, lvl + 2);
  node->type(cdk::balanced3_type::create());
}

void p6::type_checker::do_builtin_node(p6::builtin_node *const node, int lvl) {
  ASSERT_UNSPEC;
  // key(ms) and random(n) take an integer; clock takes nothing. All three give an integer.
  if (node->argument()) {
    node->argument()->accept(this, lvl + 2);
    if (!node->argument()->is_typed(cdk::TYPE_BALANCED3))
      throw std::string(node->kind() == tKEY ? "key" : "random") + " expects an integer argument";
  }
  node->type(cdk::balanced3_type::create());
}

void p6::type_checker::do_function_call_node(p6::function_call_node *const node, int lvl) {
  ASSERT_UNSPEC;
  const std::string &id = node->name();
  auto sym = _symtab.find(id);
  if (!sym)
    throw std::string("undeclared function '" + id + "'");

  if (!sym->is_function())
    throw std::string("symbol '" + id + "' is not a function");

  auto func_type = std::dynamic_pointer_cast<cdk::functional_type>(sym->type());
  if (node->arguments()->size() != func_type->input_length())
    throw std::string("wrong number of arguments in call to function '" + id + "'");

  for (size_t i = 0; i < node->arguments()->size(); i++) {
    _expectedType = func_type->input(i);
    node->arguments()->node(i)->accept(this, lvl + 2);
    _expectedType = nullptr;

    auto arg_node = dynamic_cast<cdk::expression_node *>(node->arguments()->node(i));
    if (!can_promote(arg_node->type(), func_type->input(i)))
      throw std::string("argument type mismatch in call to function '" + id + "'");
  }

  node->type(func_type->output(0)); // P6 only supports single return value
}

//---------------------------------------------------------------------------

void p6::type_checker::do_variable_declaration_node(p6::variable_declaration_node *const node,
                                                    int lvl) {
  const std::string &id = node->name();
  if (_symtab.find_local(id))
    throw std::string("variable '" + id + "' already declared in this scope");

  std::shared_ptr<cdk::basic_type> var_type = node->vartype();

  if (!var_type) {
    // Auto
    if (!node->initializer())
      throw std::string("auto variable '" + id + "' requires initializer");

    node->initializer()->accept(this, lvl + 2);
    var_type = node->initializer()->type();
    if (var_type->name() == cdk::TYPE_VOID)
      throw std::string("cannot declare auto variable with void type");

    node->type(var_type);
  } else {
    // Explicit
    if (var_type->name() == cdk::TYPE_VOID)
      throw std::string("cannot declare variable '" + id + "' with void type");

    if (node->initializer()) {
      _expectedType = var_type;
      node->initializer()->accept(this, lvl + 2);
      _expectedType = nullptr;
      if (!can_promote(node->initializer()->type(), var_type))
        throw std::string("initializer type mismatch for variable '" + id + "'");
    }
  }

  // Add to symbol table
  bool is_global = (_currentReturnType == nullptr);
  if (!is_global && node->qualifier() != 0)
    throw std::string("cannot use public or forward qualifiers in local variable declaration");

  if (is_global && node->initializer() && !is_literal_initializer(node->initializer()))
    throw std::string("global variable '" + id + "' initializer must be a literal");

  auto sym =
      std::make_shared<p6::symbol>(var_type, id, 0, is_global, false, false, node->qualifier());
  _symtab.insert(id, sym);
  if (_parent)
    _parent->set_new_symbol(sym);
}

void p6::type_checker::do_function_declaration_node(p6::function_declaration_node *const node,
                                                    int lvl) {
  const std::string &id = node->name();

  if (_symtab.find(id))
    throw std::string("symbol '" + id + "' already declared");

  auto func_type = std::dynamic_pointer_cast<cdk::functional_type>(node->type());
  auto sym = std::make_shared<p6::symbol>(func_type, id, 0, true, false, true, node->qualifier());

  _symtab.insert(id, sym);
  if (_parent)
    _parent->set_new_symbol(sym);
}

void p6::type_checker::do_function_definition_node(p6::function_definition_node *const node,
                                                   int lvl) {
  const std::string &id = node->name();
  auto existing = _symtab.find(id);

  std::vector<std::shared_ptr<cdk::basic_type>> param_types;
  std::set<std::string> seen_params;
  if (node->params()) {
    for (size_t i = 0; i < node->params()->size(); i++) {
      auto decl = dynamic_cast<p6::variable_declaration_node *>(node->params()->node(i));
      if (seen_params.find(decl->name()) != seen_params.end())
        throw std::string("parameter '" + decl->name() + "' declared more than once in function '" +
                          id + "'");

      seen_params.insert(decl->name());

      if (decl->vartype()->name() == cdk::TYPE_VOID)
        throw std::string("parameter '" + decl->name() + "' has void type in function '" + id +
                          "'");

      param_types.push_back(decl->vartype());
    }
  }

  auto func_type = cdk::functional_type::create(param_types, node->type());

  std::shared_ptr<p6::symbol> sym;
  if (existing) {
    if (!existing->is_function())
      throw std::string("symbol '" + id + "' already declared as a variable");

    if (existing->qualifier() == tEXTERN)
      throw std::string("cannot define extern function '" + id + "'");

    if (existing->is_defined())
      throw std::string("function '" + id + "' already defined");

    auto existing_func = std::dynamic_pointer_cast<cdk::functional_type>(existing->type());
    if (existing_func->to_string() != func_type->to_string())
      throw std::string("conflicting definition for function '" + id + "'");

    sym = existing;
  } else {
    sym = std::make_shared<p6::symbol>(func_type, id, 0, true, false, true, node->qualifier());
    _symtab.insert(id, sym);
    if (_parent)
      _parent->set_new_symbol(sym);
  }

  sym->set_defined();

  // Body type checking
  _symtab.push();
  if (node->params()) {
    for (size_t i = 0; i < node->params()->size(); i++) {
      auto decl = dynamic_cast<p6::variable_declaration_node *>(node->params()->node(i));
      auto param_sym =
          std::make_shared<p6::symbol>(decl->vartype(), decl->name(), 0, false, true, false, 0);
      _symtab.insert(decl->name(), param_sym);
    }
  }

  auto old_return = _currentReturnType;
  _currentReturnType = node->type();

  node->body()->accept(this, lvl + 2);

  _currentReturnType = old_return;
  _symtab.pop();
}

//---------------------------------------------------------------------------

void p6::type_checker::do_balanced3_node(cdk::balanced3_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->type(cdk::balanced3_type::create());
}

void p6::type_checker::do_takum3_node(cdk::takum3_node *const node, int lvl) {
  ASSERT_UNSPEC;
  node->type(cdk::takum3_type::create());
}

void p6::type_checker::do_posit3_node(cdk::posit3_node *const node, int lvl) {
  // EMPTY
}
