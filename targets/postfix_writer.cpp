#include "targets/postfix_writer.h"
#include ".auto/all_nodes.h" // all_nodes.h is automatically generated
#include "p6_parser.tab.h"
#include "targets/type_checker.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

//---------------------------------------------------------------------------
// Private Helpers
//---------------------------------------------------------------------------

void p6::postfix_writer::promote(std::shared_ptr<cdk::basic_type> from,
                                 std::shared_ptr<cdk::basic_type> to) {
  if (!from || !to)
    return;
  if (from->name() == to->name())
    return;
  if (from->name() == cdk::TYPE_BALANCED3 && to->name() == cdk::TYPE_TAKUM3)
    _pf.B2T();
}

std::shared_ptr<cdk::basic_type>
p6::postfix_writer::get_common_type(std::shared_ptr<cdk::basic_type> lt,
                                    std::shared_ptr<cdk::basic_type> rt) {
  if (lt->name() == cdk::TYPE_TAKUM3 || rt->name() == cdk::TYPE_TAKUM3)
    return cdk::takum3_type::create();

  if (lt->name() == cdk::TYPE_BALANCED3 || rt->name() == cdk::TYPE_BALANCED3)
    return cdk::balanced3_type::create();

  return lt;
}

static int pointer_step_size(std::shared_ptr<cdk::basic_type> type) {
  auto ref = std::dynamic_pointer_cast<cdk::reference_type>(type);
  return ref->referenced()->name() == cdk::TYPE_VOID ? 1 : ref->referenced()->size();
}

void p6::postfix_writer::scan_defined_functions(cdk::basic_node *node) {
  if (!node)
    return;
  if (auto fdef = dynamic_cast<p6::function_definition_node *>(node)) {
    std::string id = fdef->name();
    if (id == "p6")
      id = "_main";
    _definedFunctions.insert(id);
  } else if (auto seq = dynamic_cast<cdk::sequence_node *>(node)) {
    for (size_t i = 0; i < seq->size(); i++) {
      scan_defined_functions(seq->node(i));
    }
  }
}

void p6::postfix_writer::emit_comparison(int op, std::shared_ptr<cdk::basic_type> operand_type) {
  if (operand_type->name() == cdk::TYPE_BALANCED3) {
    std::string func;
    switch (op) {
    case 'E':
      func = "balanced3_eq";
      break;
    case 'N':
      func = "balanced3_ne";
      break;
    case 'L':
      func = "balanced3_lt";
      break;
    case 'G':
      func = "balanced3_gt";
      break;
    case 'l':
      func = "balanced3_le";
      break;
    case 'g':
      func = "balanced3_ge";
      break;
    }
    _pf.SWAP64();
    _pf.CALL(func);
    _pf.TRASH(16);
    _pf.LDFVAL64I();
  } else if (operand_type->name() == cdk::TYPE_TAKUM3) {
    std::string func;
    switch (op) {
    case 'E':
      func = "takum3_eq";
      break;
    case 'N':
      func = "takum3_ne";
      break;
    case 'L':
      func = "takum3_lt";
      break;
    case 'G':
      func = "takum3_gt";
      break;
    case 'l':
      func = "takum3_le";
      break;
    case 'g':
      func = "takum3_ge";
      break;
    }
    _pf.SWAP128();
    _pf.CALL(func);
    _pf.TRASH(32);
    _pf.LDFVAL64I();
  } else {
    switch (op) {
    case 'E':
      _pf.EQ();
      break;
    case 'N':
      _pf.NE();
      break;
    case 'L':
      _pf.LT();
      break;
    case 'G':
      _pf.GT();
      break;
    case 'l':
      _pf.LE();
      break;
    case 'g':
      _pf.GE();
      break;
    }
    _pf.I2B();
  }
}

void p6::postfix_writer::emit_static_initializer(cdk::expression_node *init,
                                                 std::shared_ptr<cdk::basic_type> type,
                                                 bool sign_neg) {
  // Handle Unary
  if (auto node = dynamic_cast<cdk::unary_minus_node *>(init)) {
    emit_static_initializer(node->argument(), type, !sign_neg);
    return;
  }
  if (auto node = dynamic_cast<cdk::unary_plus_node *>(init)) {
    emit_static_initializer(node->argument(), type, sign_neg);
    return;
  }

  if (auto node = dynamic_cast<cdk::balanced3_node *>(init)) {
    cdk::treg_value val = node->value();
    if (sign_neg) {
      cdk::treg_value dst;
      cdk::treg_alu::neg(dst, val);
      val = dst;
    }
    if (type->name() == cdk::TYPE_BALANCED3) {
      _pf.SBALANCED3(val);
    } else if (type->name() == cdk::TYPE_TAKUM3) {
      cdk::treg_value tk;
      cdk::treg_alu::load_takum3_from_balanced3(tk, val);
      _pf.STAKUM3(tk);
    }
  } else if (auto node = dynamic_cast<cdk::takum3_node *>(init)) {
    cdk::treg_value val = node->value();
    if (sign_neg) {
      cdk::treg_value dst;
      cdk::treg_alu::neg(dst, val);
      val = dst;
    }
    _pf.STAKUM3(val);
  } else if (dynamic_cast<p6::nullptr_node *>(init)) {
    _pf.SINT(0);
  } else if (auto node = dynamic_cast<cdk::string_node *>(init)) {
    int lbl = ++_lbl;
    _pf.RODATA();
    _pf.ALIGN();
    _pf.LABEL(mklbl(lbl));
    _pf.SSTRING(node->value());
    _pf.DATA();
    _pf.ALIGN();
    _pf.SADDR(mklbl(lbl));
  }
}

//---------------------------------------------------------------------------
// Standard visitors
//---------------------------------------------------------------------------

void p6::postfix_writer::do_nil_node(cdk::nil_node *const node, int lvl) {
  // EMPTY
}

void p6::postfix_writer::do_data_node(cdk::data_node *const node, int lvl) {
  // EMPTY
}

void p6::postfix_writer::do_double_node(cdk::double_node *const node, int lvl) {
  // EMPTY
}

void p6::postfix_writer::do_not_node(cdk::not_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->argument()->accept(this, lvl);
    return;
  }

  node->argument()->accept(this, lvl);
  _pf.KNOT();
}

void p6::postfix_writer::do_and_node(cdk::and_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  int lbl_end = ++_lbl;
  node->left()->accept(this, lvl);
  _pf.DUP64();
  _pf.B2I();
  _pf.INT(0);
  _pf.JLT(mklbl(lbl_end)); // if < 0 (False), short-circuit jump to end

  node->right()->accept(this, lvl);
  _pf.KAND();
  _pf.LABEL(mklbl(lbl_end));
}

void p6::postfix_writer::do_or_node(cdk::or_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  int lbl_end = ++_lbl;
  node->left()->accept(this, lvl);
  _pf.DUP64();
  _pf.B2I();
  _pf.INT(0);
  _pf.JGT(mklbl(lbl_end)); // if > 0 (True), short-circuit jump to end

  node->right()->accept(this, lvl);
  _pf.KOR();
  _pf.LABEL(mklbl(lbl_end));
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_sequence_node(cdk::sequence_node *const node, int lvl) {
  if (!_scanned_functions) {
    _scanned_functions = true;
    scan_defined_functions(node);
  }

  for (size_t i = 0; i < node->size(); i++) {
    node->node(i)->accept(this, lvl);
  }
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_integer_node(cdk::integer_node *const node, int lvl) {
  // EMPTY
}

void p6::postfix_writer::do_string_node(cdk::string_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  /* generate the string */
  int lbl1 = ++_lbl;
  _pf.RODATA();
  _pf.ALIGN();
  _pf.LABEL(mklbl(lbl1));
  _pf.SSTRING(node->value());

  /* Add String addr to stack */
  _pf.TEXT();
  _pf.ADDR(mklbl(lbl1));
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_unary_minus_node(cdk::unary_minus_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->argument()->accept(this, lvl);
    return;
  }

  node->argument()->accept(this, lvl);
  auto type = node->argument()->type();
  if (type->name() == cdk::TYPE_BALANCED3) {
    _pf.BNEG();
  } else if (type->name() == cdk::TYPE_TAKUM3) {
    _pf.TNEG();
  }
}

void p6::postfix_writer::do_unary_plus_node(cdk::unary_plus_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->argument()->accept(this, lvl);
    return;
  }

  node->argument()->accept(this, lvl);
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_add_node(cdk::add_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  if (lt->name() == cdk::TYPE_POINTER) {
    // Pointer + Integer
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    if (rt->name() == cdk::TYPE_BALANCED3)
      _pf.B2I();

    _pf.INT(pointer_step_size(lt));
    _pf.MUL();
    _pf.ADD();
  } else if (rt->name() == cdk::TYPE_POINTER) {
    // Integer + Pointer
    node->left()->accept(this, lvl);
    if (lt->name() == cdk::TYPE_BALANCED3)
      _pf.B2I();

    _pf.INT(pointer_step_size(rt));
    _pf.MUL();
    node->right()->accept(this, lvl);
    _pf.ADD();
  } else {
    // Numeric + Numeric
    auto common = get_common_type(lt, rt);
    node->left()->accept(this, lvl);
    promote(lt, common);
    node->right()->accept(this, lvl);
    promote(rt, common);
    if (common->name() == cdk::TYPE_BALANCED3) {
      _pf.BADD();
    } else if (common->name() == cdk::TYPE_TAKUM3) {
      _pf.TADD();
    }
  }
}

void p6::postfix_writer::do_sub_node(cdk::sub_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  if (lt->name() == cdk::TYPE_POINTER && rt->name() == cdk::TYPE_POINTER) {
    // Pointer - Pointer
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    _pf.SUB();
    _pf.INT(pointer_step_size(lt));
    _pf.DIV();
    _pf.I2B();
  } else if (lt->name() == cdk::TYPE_POINTER) {
    // Pointer - Integer
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    if (rt->name() == cdk::TYPE_BALANCED3) {
      _pf.B2I();
    }
    _pf.INT(pointer_step_size(lt));
    _pf.MUL();
    _pf.SUB();
  } else {
    auto common = get_common_type(lt, rt);
    node->left()->accept(this, lvl);
    promote(lt, common);
    node->right()->accept(this, lvl);
    promote(rt, common);
    if (common->name() == cdk::TYPE_BALANCED3) {
      _pf.BSUB();
    } else if (common->name() == cdk::TYPE_TAKUM3) {
      _pf.TSUB();
    }
  }
}

void p6::postfix_writer::do_mul_node(cdk::mul_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  auto common = get_common_type(lt, rt);
  node->left()->accept(this, lvl);
  promote(lt, common);
  node->right()->accept(this, lvl);
  promote(rt, common);
  if (common->name() == cdk::TYPE_BALANCED3) {
    _pf.BMUL();
  } else if (common->name() == cdk::TYPE_TAKUM3) {
    _pf.TMUL();
  }
}

void p6::postfix_writer::do_div_node(cdk::div_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  auto common = get_common_type(lt, rt);
  node->left()->accept(this, lvl);
  promote(lt, common);
  node->right()->accept(this, lvl);
  promote(rt, common);
  if (common->name() == cdk::TYPE_BALANCED3) {
    _pf.BDIV();
  } else if (common->name() == cdk::TYPE_TAKUM3) {
    _pf.TDIV();
  }
}

void p6::postfix_writer::do_mod_node(cdk::mod_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  node->left()->accept(this, lvl);
  node->right()->accept(this, lvl);
  _pf.BMOD();
}

void p6::postfix_writer::do_lt_node(cdk::lt_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  auto common = get_common_type(lt, rt);
  node->left()->accept(this, lvl);
  promote(lt, common);
  node->right()->accept(this, lvl);
  promote(rt, common);
  emit_comparison('L', common);
}

void p6::postfix_writer::do_le_node(cdk::le_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  auto common = get_common_type(lt, rt);
  node->left()->accept(this, lvl);
  promote(lt, common);
  node->right()->accept(this, lvl);
  promote(rt, common);
  emit_comparison('l', common);
}

void p6::postfix_writer::do_ge_node(cdk::ge_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  auto common = get_common_type(lt, rt);
  node->left()->accept(this, lvl);
  promote(lt, common);
  node->right()->accept(this, lvl);
  promote(rt, common);
  emit_comparison('g', common);
}

void p6::postfix_writer::do_gt_node(cdk::gt_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  auto common = get_common_type(lt, rt);
  node->left()->accept(this, lvl);
  promote(lt, common);
  node->right()->accept(this, lvl);
  promote(rt, common);
  emit_comparison('G', common);
}

void p6::postfix_writer::do_ne_node(cdk::ne_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  auto common = get_common_type(lt, rt);
  node->left()->accept(this, lvl);
  promote(lt, common);
  node->right()->accept(this, lvl);
  promote(rt, common);
  emit_comparison('N', common);
}

void p6::postfix_writer::do_eq_node(cdk::eq_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->left()->accept(this, lvl);
    node->right()->accept(this, lvl);
    return;
  }

  auto lt = node->left()->type();
  auto rt = node->right()->type();

  auto common = get_common_type(lt, rt);
  node->left()->accept(this, lvl);
  promote(lt, common);
  node->right()->accept(this, lvl);
  promote(rt, common);
  emit_comparison('E', common);
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_variable_node(cdk::variable_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  auto sym = _symtab.find(node->name());
  if (sym->is_global()) {
    _pf.ADDR(sym->name());
  } else {
    _pf.LOCAL(sym->offset());
  }
}

void p6::postfix_writer::do_rvalue_node(cdk::rvalue_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->lvalue()->accept(this, lvl);
    return;
  }

  node->lvalue()->accept(this, lvl);
  int size = node->type()->size();
  if (size == 4) {
    _pf.LDINT();
  } else if (size == 8) {
    _pf.LDBALANCED3();
  } else if (size == 16) {
    _pf.LDTAKUM3();
  }
}

void p6::postfix_writer::do_assignment_node(cdk::assignment_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->lvalue()->accept(this, lvl);
    node->rvalue()->accept(this, lvl);
    return;
  }

  node->rvalue()->accept(this, lvl);
  promote(node->rvalue()->type(), node->lvalue()->type());
  int size = node->lvalue()->type()->size();
  if (size == 4) {
    _pf.DUP32();
  } else if (size == 8) {
    _pf.DUP64();
  } else if (size == 16) {
    _pf.DUP128();
  }

  node->lvalue()->accept(this, lvl);
  if (size == 4) {
    _pf.STINT();
  } else if (size == 8) {
    _pf.STBALANCED3();
  } else if (size == 16) {
    _pf.STTAKUM3();
  }
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_program_node(p6::program_node *const node, int lvl) {
  _inMainProgram = true;
  _currentReturnType = cdk::balanced3_type::create();
  ASSERT_SAFE_EXPRESSIONS;

  _allocating_offsets = true;
  _local_offset = 0;
  if (node->statements())
    node->statements()->accept(this, lvl);

  _allocating_offsets = false;

  // generate the main function (RTS mandates that its name be "_main")
  _pf.TEXT();
  _pf.ALIGN();
  _pf.GLOBAL("_main", _pf.FUNC());
  _pf.LABEL("_main");
  _pf.ENTER(-_local_offset);

  if (node->statements())
    node->statements()->accept(this, lvl);

  // end the main function
  _pf.INT(0);
  _pf.STFVAL32I();
  _pf.LEAVE();
  _pf.RET();

  _currentReturnType = nullptr;
  _inMainProgram = false;

  // library function imports
  _pf.EXTERN("readi");
  _pf.EXTERN("printi");
  _pf.EXTERN("prints");
  _pf.EXTERN("println");

  _pf.EXTERN("balanced3_read");
  _pf.EXTERN("balanced3_print");
  _pf.EXTERN("takum3_read");
  _pf.EXTERN("takum3_print");

  // comparison functions externs
  _pf.EXTERN("balanced3_eq");
  _pf.EXTERN("balanced3_ne");
  _pf.EXTERN("balanced3_lt");
  _pf.EXTERN("balanced3_gt");
  _pf.EXTERN("balanced3_le");
  _pf.EXTERN("balanced3_ge");

  _pf.EXTERN("takum3_eq");
  _pf.EXTERN("takum3_ne");
  _pf.EXTERN("takum3_lt");
  _pf.EXTERN("takum3_gt");
  _pf.EXTERN("takum3_le");
  _pf.EXTERN("takum3_ge");
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_evaluation_node(p6::evaluation_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->argument()->accept(this, lvl);
    return;
  }

  node->argument()->accept(this, lvl);
  _pf.TRASH(node->argument()->type()->size());
}

void p6::postfix_writer::do_print_node(p6::print_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    if (node->arguments())
      node->arguments()->accept(this, lvl);

    return;
  }

  if (node->arguments()) {
    for (size_t i = 0; i < node->arguments()->size(); i++) {
      auto expr = dynamic_cast<cdk::expression_node *>(node->arguments()->node(i));
      expr->accept(this, lvl);
      if (expr->is_typed(cdk::TYPE_STRING)) {
        _pf.CALL("prints");
        _pf.TRASH(4);
      } else if (expr->is_typed(cdk::TYPE_BALANCED3)) {
        _pf.CALL("balanced3_print");
        _pf.TRASH(8);
      } else if (expr->is_typed(cdk::TYPE_TAKUM3)) {
        _pf.CALL("takum3_print");
        _pf.TRASH(16);
      }
    }
  }
  if (node->newline()) {
    _pf.CALL("println");
  }
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_input_node(p6::input_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  if (node->is_typed(cdk::TYPE_BALANCED3)) {
    _pf.CALL("balanced3_read");
    _pf.LDFVAL64I();
  } else if (node->is_typed(cdk::TYPE_TAKUM3)) {
    _pf.INT(16);
    _pf.ALLOC();
    _pf.SP();
    _pf.CALL("takum3_read");
    _pf.TRASH(4);
  }
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_while_node(p6::while_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    _loopNextLabels.push_back("");
    _loopStopLabels.push_back("");
    node->condition()->accept(this, lvl);
    node->block()->accept(this, lvl);
    _loopNextLabels.pop_back();
    _loopStopLabels.pop_back();
    return;
  }

  int lbl_step = ++_lbl;
  int lbl_end = ++_lbl;

  _loopNextLabels.push_back(mklbl(lbl_step));
  _loopStopLabels.push_back(mklbl(lbl_end));

  _pf.LABEL(mklbl(lbl_step));
  node->condition()->accept(this, lvl);
  if (node->condition()->is_typed(cdk::TYPE_BALANCED3)) {
    _pf.B2I();
    _pf.INT(0);
    _pf.JLE(mklbl(lbl_end));
  } else {
    _pf.JZ(mklbl(lbl_end));
  }

  node->block()->accept(this, lvl + 2);

  _pf.JMP(mklbl(lbl_step));
  _pf.LABEL(mklbl(lbl_end));

  _loopNextLabels.pop_back();
  _loopStopLabels.pop_back();
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_if_node(p6::if_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->condition()->accept(this, lvl);
    node->block()->accept(this, lvl);
    return;
  }

  int lbl_end = ++_lbl;
  node->condition()->accept(this, lvl);
  if (node->condition()->is_typed(cdk::TYPE_BALANCED3)) {
    _pf.B2I();
    _pf.INT(0);
    _pf.JLE(mklbl(lbl_end));
  } else {
    _pf.JZ(mklbl(lbl_end));
  }
  node->block()->accept(this, lvl + 2);
  _pf.LABEL(mklbl(lbl_end));
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_if_else_node(p6::if_else_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->condition()->accept(this, lvl);
    node->thenblock()->accept(this, lvl);
    node->elseblock()->accept(this, lvl);
    return;
  }

  int lbl_else = ++_lbl;
  int lbl_end = ++_lbl;

  node->condition()->accept(this, lvl);
  if (node->condition()->is_typed(cdk::TYPE_BALANCED3)) {
    _pf.B2I();
    _pf.INT(0);
    _pf.JLE(mklbl(lbl_else));
  } else {
    _pf.JZ(mklbl(lbl_else));
  }
  node->thenblock()->accept(this, lvl + 2);
  _pf.JMP(mklbl(lbl_end));
  _pf.LABEL(mklbl(lbl_else));
  node->elseblock()->accept(this, lvl + 2);
  _pf.LABEL(mklbl(lbl_end));
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_block_node(p6::block_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  _symtab.push();
  if (node->declarations())
    node->declarations()->accept(this, lvl);

  if (node->instructions())
    node->instructions()->accept(this, lvl);

  _symtab.pop();
}

void p6::postfix_writer::do_return_node(p6::return_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    if (node->retval())
      node->retval()->accept(this, lvl);

    return;
  }

  if (node->retval()) {
    node->retval()->accept(this, lvl);
    promote(node->retval()->type(), _currentReturnType);
    if (_inMainProgram) {
      _pf.B2I();
      _pf.STFVAL32I();
    } else if (_currentReturnType->name() == cdk::TYPE_TAKUM3) {
      // Store 16 bytes into sret pointer passed at +8
      _pf.LOCV(8);
      _pf.STTAKUM3();
      _pf.LOCV(8);
      _pf.STFVAL32I();
    } else {
      int size = _currentReturnType->size();
      if (size == 4) {
        _pf.STFVAL32I();
      } else if (size == 8) {
        _pf.STFVAL64I();
      }
    }
  }
  _pf.LEAVE();
  _pf.RET();
}

void p6::postfix_writer::do_stop_node(p6::stop_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  int index = _loopStopLabels.size() - node->level();
  _pf.JMP(_loopStopLabels[index]);
}

void p6::postfix_writer::do_next_node(p6::next_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  int index = _loopNextLabels.size() - node->level();
  _pf.JMP(_loopNextLabels[index]);
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_nullptr_node(p6::nullptr_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  _pf.INT(0);
}

void p6::postfix_writer::do_address_of_node(p6::address_of_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  node->lvalue()->accept(this, lvl);
}

void p6::postfix_writer::do_index_node(p6::index_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->base()->accept(this, lvl);
    node->index()->accept(this, lvl);
    return;
  }

  node->base()->accept(this, lvl);
  node->index()->accept(this, lvl);
  _pf.B2I();
  auto ref = std::dynamic_pointer_cast<cdk::reference_type>(node->base()->type());
  _pf.INT(ref->referenced()->size());
  _pf.MUL();
  _pf.ADD();
}

void p6::postfix_writer::do_stack_alloc_node(p6::stack_alloc_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    node->argument()->accept(this, lvl);
    return;
  }

  node->argument()->accept(this, lvl);
  _pf.B2I();
  auto ref = std::dynamic_pointer_cast<cdk::reference_type>(node->type());
  _pf.INT(ref->referenced()->size());
  _pf.MUL();
  _pf.ALLOC();
  _pf.SP();
}

void p6::postfix_writer::do_sizeof_node(p6::sizeof_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  int size = node->argument()->type()->size();
  _pf.INT(size);
  _pf.I2B();
}

void p6::postfix_writer::do_function_call_node(p6::function_call_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets) {
    if (node->arguments())
      node->arguments()->accept(this, lvl);

    return;
  }

  std::string id = node->name();
  if (id == "p6")
    id = "_main";

  auto sym = _symtab.find(id);
  auto func_type = std::dynamic_pointer_cast<cdk::functional_type>(sym->type());
  bool is_extern = (sym && sym->qualifier() == tEXTERN);

  bool is_sret = (func_type->output(0)->name() == cdk::TYPE_TAKUM3) && !is_extern;

  if (is_sret) {
    _pf.INT(16);
    _pf.ALLOC();
  }

  int K = 0;
  if (node->arguments()) {
    for (int i = static_cast<int>(node->arguments()->size()) - 1; i >= 0; i--) {
      auto arg = dynamic_cast<cdk::expression_node *>(node->arguments()->node(i));
      arg->accept(this, lvl);
      promote(arg->type(), func_type->input(i));
      if (is_extern) {
        if (func_type->input(i)->name() == cdk::TYPE_BALANCED3) {
          _pf.B2I();
          K += 4;
        } else if (func_type->input(i)->name() == cdk::TYPE_TAKUM3) {
          _pf.T2D();
          K += 8;
        } else {
          K += func_type->input(i)->size();
        }
      } else {
        K += func_type->input(i)->size();
      }
    }
  }

  if (is_sret) {
    _pf.SP();
    _pf.INT(K);
    _pf.ADD();
  }

  _pf.CALL(id);

  if (is_sret) {
    _pf.TRASH(K + 4);
  } else {
    _pf.TRASH(K);
    auto out_type = func_type->output(0);
    if (out_type->name() != cdk::TYPE_VOID) {
      if (is_extern) {
        if (out_type->name() == cdk::TYPE_BALANCED3) {
          _pf.LDFVAL32I();
          _pf.I2B();
        } else if (out_type->name() == cdk::TYPE_TAKUM3) {
          _pf.LDFVAL64F();
          _pf.D2T();
        } else {
          _pf.LDFVAL32I();
        }
      } else {
        int size = out_type->size();
        if (size == 4) {
          _pf.LDFVAL32I();
        } else if (size == 8) {
          _pf.LDFVAL64I();
        }
      }
    }
  }
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_variable_declaration_node(p6::variable_declaration_node *const node,
                                                      int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  auto sym = _symtab.find(node->name());
  if (_allocating_offsets) {
    _local_offset -= sym->type()->size();
    sym->offset(_local_offset);
    _declOffsets[node] = _local_offset;
    return;
  }

  if (sym->is_global()) {
    if (sym->qualifier() == tFORWARD) {
      _pf.EXTERN(sym->name());
      return;
    }

    if (node->initializer()) {
      _pf.DATA();
      _pf.ALIGN();
      if (sym->qualifier() == tPUBLIC)
        _pf.GLOBAL(sym->name(), _pf.OBJ());

      _pf.LABEL(sym->name());
      emit_static_initializer(node->initializer(), sym->type());
    } else {
      _pf.BSS();
      _pf.ALIGN();
      if (sym->qualifier() == tPUBLIC)
        _pf.GLOBAL(sym->name(), _pf.OBJ());

      _pf.LABEL(sym->name());
      _pf.SALLOC(sym->type()->size());
    }
  } else {
    sym->offset(_declOffsets[node]);
    if (node->initializer()) {
      node->initializer()->accept(this, lvl);
      promote(node->initializer()->type(), sym->type());
      int size = sym->type()->size();
      _pf.LOCAL(sym->offset());
      if (size == 4) {
        _pf.STINT();
      } else if (size == 8) {
        _pf.STBALANCED3();
      } else if (size == 16) {
        _pf.STTAKUM3();
      }
    }
  }
}

void p6::postfix_writer::do_function_declaration_node(p6::function_declaration_node *const node,
                                                      int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  std::string id = node->name();
  if (id == "p6")
    id = "_main";
  if (_definedFunctions.count(id) == 0)
    _pf.EXTERN(id);
}

void p6::postfix_writer::do_function_definition_node(p6::function_definition_node *const node,
                                                     int lvl) {
  ASSERT_SAFE_EXPRESSIONS;

  std::string id = node->name();
  if (id == "p6")
    id = "_main";

  // Create and insert parameters into the symbol table, setting their offsets
  _symtab.push();
  int arg_offset = (node->type()->name() == cdk::TYPE_TAKUM3) ? 12 : 8;
  if (node->params()) {
    for (size_t i = 0; i < node->params()->size(); i++) {
      auto decl = dynamic_cast<p6::variable_declaration_node *>(node->params()->node(i));
      auto param_sym = std::make_shared<p6::symbol>(decl->vartype(), decl->name(), arg_offset,
                                                    false, true, false, 0);
      _symtab.insert(decl->name(), param_sym);
      arg_offset += param_sym->type()->size();
    }
  }

  auto old_return = _currentReturnType;
  _currentReturnType = node->type();
  _allocating_offsets = true;
  _local_offset = 0;
  node->body()->accept(this, lvl);
  _allocating_offsets = false;

  _pf.TEXT();
  _pf.ALIGN();
  if (node->qualifier() == tPUBLIC)
    _pf.GLOBAL(id, _pf.FUNC());
  _pf.LABEL(id);
  _pf.ENTER(-_local_offset);

  node->body()->accept(this, lvl);
  _currentReturnType = old_return;
  if (node->type()->name() == cdk::TYPE_VOID) {
    _pf.LEAVE();
    _pf.RET();
  } else if (node->type()->name() == cdk::TYPE_TAKUM3) {
    _pf.TAKUM3(cdk::takum3_type::value_type("0.0"));
    _pf.LOCV(8);
    _pf.STTAKUM3();
    _pf.LOCV(8);
    _pf.STFVAL32I();
    _pf.LEAVE();
    _pf.RET();
  } else {
    int size = node->type()->size();
    if (size == 4) {
      _pf.INT(0);
      _pf.STFVAL32I();
    } else if (size == 8) {
      _pf.BALANCED3(cdk::balanced3_type::value_type(0));
      _pf.STFVAL64I();
    }
    _pf.LEAVE();
    _pf.RET();
  }

  _symtab.pop();
}

//---------------------------------------------------------------------------

void p6::postfix_writer::do_balanced3_node(cdk::balanced3_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  _pf.BALANCED3(node->value());
}

void p6::postfix_writer::do_takum3_node(cdk::takum3_node *const node, int lvl) {
  ASSERT_SAFE_EXPRESSIONS;
  if (_allocating_offsets)
    return;

  _pf.TAKUM3(node->value());
}

void p6::postfix_writer::do_posit3_node(cdk::posit3_node *const node, int lvl) {
  // EMPTY
}
