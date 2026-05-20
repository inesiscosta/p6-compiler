%{
//-- don't change *any* of these: if you do, you'll break the compiler.
#include <algorithm>
#include <memory>
#include <cstring>
#include <cdk/compiler.h>
#include <cdk/types/types.h>
#include ".auto/all_nodes.h"
#define LINE                         compiler->scanner()->lineno()
#define yylex()                      compiler->scanner()->scan()
#define yyerror(compiler, s)         compiler->scanner()->error(s)
//-- don't change *any* of these --- END!
%}

%parse-param {std::shared_ptr<cdk::compiler> compiler}
%expect 6

%union {
  //--- don't change *any* of these: if you do, you'll break the compiler.
  YYSTYPE() : type(cdk::primitive_type::create(0, cdk::TYPE_VOID)) {}
  ~YYSTYPE() {}
  YYSTYPE(const YYSTYPE &other) { *this = other; }
  YYSTYPE& operator=(const YYSTYPE &other) { type = other.type; return *this; }

  std::shared_ptr<cdk::basic_type> type;        /* expression type */
  //-- don't change *any* of these --- END!

  long long            i;           /* integer value */
  std::string          *s;          /* symbol name or string literal */
  cdk::basic_node      *node;       /* node pointer */
  cdk::sequence_node   *sequence;
  cdk::expression_node *expression; /* expression nodes */
  cdk::lvalue_node     *lvalue;
  std::vector<std::shared_ptr<cdk::basic_type>> *types_vector;
};

// Payloads
%token <i> tINTEGER
%token <s> tIDENTIFIER tSTRING tREAL_LITERAL

// Multi-char Operators
%token tAND tOR tEQ tNE tLE tGE

// Keywords
%token tINT tREAL tSTRING_TYPE tVOID tAUTO                       // Types
%token tEXTERN tFORWARD tPUBLIC                                  // Function modifiers
%token tIF tELIF tELSE tWHILE tSTOP tNEXT tRETURN                // Control flow
%token tREAD tPRINT_NL tNULL tSIZEOF tBEGIN tEND tUNARY tARROW   // Others

// Resolve the dangling else problem
%nonassoc tIFX
%nonassoc tELIF
%nonassoc tELSE

// Operator precedence and associativity
%right '='
%left tOR
%left tAND
%left tEQ tNE
%left tGE tLE '>' '<'
%left '+' '-'
%left '*' '/' '%'
%nonassoc tUNARY '~' '?'
%left '[' ']'

// AST node types
%type <node> stmt prgm elif decl vardl fundl fundf var
%type <sequence> exprs decls vars inner_decls inner_stmts
%type <types_vector> types
%type <expression> expr
%type <lvalue> lval
%type <s> str
%type <type> type vart funt

%{
//-- The rules below will be included in yyparse, the main parsing function.
%}
%%

/* File & Program */
file : decls                                 { compiler->ast($1); }
     | decls prgm                            { cdk::sequence_node* seq = new cdk::sequence_node(LINE, $2, $1); compiler->ast(seq); }
     ;

prgm : tBEGIN inner_decls inner_stmts tEND   { $$ = new p6::program_node(LINE, new p6::block_node(LINE, $2, $3)); }
     ;

/* Declarations */
decls: /* empty */                           { $$ = new cdk::sequence_node(LINE); }
     | decls decl                            { $$ = new cdk::sequence_node(LINE, $2, $1); }
     ;

decl : vardl ';'                             { $$ = $1; }
     | fundl ';'                             { $$ = $1; } 
     | fundf                                 { $$ = $1; }
     ;

vardl: vart tIDENTIFIER                      { $$ = new p6::variable_declaration_node(LINE, 0, $1, *$2, nullptr); delete $2; }
     | tPUBLIC vart tIDENTIFIER              { $$ = new p6::variable_declaration_node(LINE, tPUBLIC, $2, *$3, nullptr); delete $3; }
     | tFORWARD vart tIDENTIFIER             { $$ = new p6::variable_declaration_node(LINE, tFORWARD, $2, *$3, nullptr); delete $3; }
     | vart tIDENTIFIER '=' expr             { $$ = new p6::variable_declaration_node(LINE, 0, $1, *$2, $4); delete $2; }
     | tPUBLIC vart tIDENTIFIER '=' expr     { $$ = new p6::variable_declaration_node(LINE, tPUBLIC, $2, *$3, $5); delete $3; }
     | tAUTO tIDENTIFIER '=' expr            { $$ = new p6::variable_declaration_node(LINE, 0, nullptr, *$2, $4); delete $2; }
     | tPUBLIC tAUTO tIDENTIFIER '=' expr    { $$ = new p6::variable_declaration_node(LINE, tPUBLIC, nullptr, *$3, $5); delete $3; }
     | tPUBLIC tIDENTIFIER '=' expr          { $$ = new p6::variable_declaration_node(LINE, tPUBLIC, nullptr, *$2, $4); delete $2; }
     ;

/* Function Declarations & Definitions */
fundl: tEXTERN funt tIDENTIFIER              { $$ = new p6::function_declaration_node(LINE, tEXTERN, $2, *$3, nullptr); delete $3; }
     | tFORWARD funt tIDENTIFIER             { $$ = new p6::function_declaration_node(LINE, tFORWARD, $2, *$3, nullptr); delete $3; }
     ;

fundf: tIDENTIFIER '(' ')' tARROW type '{' inner_decls inner_stmts '}'               { $$ = new p6::function_definition_node(LINE, 0, $5, *$1, new cdk::sequence_node(LINE), new p6::block_node(LINE, $7, $8)); delete $1; }
     | tIDENTIFIER '(' vars ')' tARROW type '{' inner_decls inner_stmts '}'          { $$ = new p6::function_definition_node(LINE, 0, $6, *$1, $3, new p6::block_node(LINE, $8, $9)); delete $1; }
     | tPUBLIC tIDENTIFIER '(' ')' tARROW type '{' inner_decls inner_stmts '}'       { $$ = new p6::function_definition_node(LINE, tPUBLIC, $6, *$2, new cdk::sequence_node(LINE), new p6::block_node(LINE, $8, $9)); delete $2; }
     | tPUBLIC tIDENTIFIER '(' vars ')' tARROW type '{' inner_decls inner_stmts '}'  { $$ = new p6::function_definition_node(LINE, tPUBLIC, $7, *$2, $4, new p6::block_node(LINE, $9, $10)); delete $2;}
     ;

/* Declarations & Statements inside code blocks */
inner_decls: /* empty */                     { $$ = new cdk::sequence_node(LINE); }
           | inner_decls vardl ';'           { $$ = new cdk::sequence_node(LINE, $2, $1); }
           ;

inner_stmts: /* empty */                     { $$ = new cdk::sequence_node(LINE); }
           | inner_stmts stmt                { $$ = new cdk::sequence_node(LINE, $2, $1); }
           ;

/* Variables */
var  : vart tIDENTIFIER                      { $$ = new p6::variable_declaration_node(LINE, 0, $1, *$2, nullptr); delete $2; }
     ;

vars : var                                   { $$ = new cdk::sequence_node(LINE, $1); }
     | vars ',' var                          { $$ = new cdk::sequence_node(LINE, $3, $1); }
     ;

/* Types */
types: type                                  { $$ = new std::vector<std::shared_ptr<cdk::basic_type>>(); $$->push_back($1); }
     | types ',' type                        { $$ = $1; $$->push_back($3); }
     ;

type : vart  { $$ = $1; }
     | funt { $$ = $1; }
     ;

vart : tINT                                  { $$ = cdk::balanced3_type::create(); }
     | tREAL                                 { $$ = cdk::takum3_type::create(); }
     | tSTRING_TYPE                          { $$ = cdk::primitive_type::create(4, cdk::TYPE_STRING); }
     | tVOID                                 { $$ = cdk::primitive_type::create(0, cdk::TYPE_VOID); }
     | '[' type ']'                          { $$ = cdk::reference_type::create(4, $2); }
     ;

funt : type '<' '>'                          { $$ = cdk::functional_type::create($1); }
     | type '<' types '>'                    { $$ = cdk::functional_type::create(*$3, $1); delete $3; }
     ;

/* Statements */
stmt : expr ';'                              { $$ = new p6::evaluation_node(LINE, $1); }
     | exprs '!'                             { $$ = new p6::print_node(LINE, $1, false); }
     | exprs tPRINT_NL                       { $$ = new p6::print_node(LINE, $1, true); }
     | tWHILE '(' expr ')' stmt              { $$ = new p6::while_node(LINE, $3, $5); }
     | tIF '(' expr ')' stmt %prec tIFX      { $$ = new p6::if_node(LINE, $3, $5); }
     | tIF '(' expr ')' stmt tELSE stmt      { $$ = new p6::if_else_node(LINE, $3, $5, $7); }
     | tIF '(' expr ')' stmt elif            { $$ = new p6::if_else_node(LINE, $3, $5, $6); }
     | '{' inner_decls inner_stmts '}'       { $$ = new p6::block_node(LINE, $2, $3); }
     | tSTOP ';'                             { $$ = new p6::stop_node(LINE, 1); }
     | tSTOP tINTEGER ';'                    { $$ = new p6::stop_node(LINE, $2); }
     | tNEXT ';'                             { $$ = new p6::next_node(LINE, 1); }
     | tNEXT tINTEGER ';'                    { $$ = new p6::next_node(LINE, $2); }
     | tRETURN ';'                           { $$ = new p6::return_node(LINE, nullptr); }
     | tRETURN expr ';'                      { $$ = new p6::return_node(LINE, $2); }
     ;

elif : tELIF '(' expr ')' stmt %prec tIFX    { $$ = new p6::if_node(LINE, $3, $5); }
     | tELIF '(' expr ')' stmt tELSE stmt    { $$ = new p6::if_else_node(LINE, $3, $5, $7); }
     | tELIF '(' expr ')' stmt elif          { $$ = new p6::if_else_node(LINE, $3, $5, $6); }
     ;

/* Expressions */
exprs: expr                                  { $$ = new cdk::sequence_node(LINE, $1); }
     | exprs ',' expr                        { $$ = new cdk::sequence_node(LINE, $3, $1); }
     ;

expr : tINTEGER                              { $$ = new cdk::balanced3_node(LINE, cdk::balanced3_type::value_type($1)); }
     | tREAD                                 { $$ = new p6::input_node(LINE); }
     | str                                   { $$ = new cdk::string_node(LINE, $1); }
     | tNULL                                 { $$ = new p6::nullptr_node(LINE); }
     | tREAL_LITERAL                         { $$ = new cdk::takum3_node(LINE, cdk::takum3_type::value_type($1->c_str())); delete $1; }
     | '-' expr %prec tUNARY                 { $$ = new cdk::unary_minus_node(LINE, $2); }
     | '+' expr %prec tUNARY                 { $$ = new cdk::unary_plus_node(LINE, $2); }
     | '~' expr %prec tUNARY                 { $$ = new cdk::not_node(LINE, $2); }
     | lval '?' %prec tUNARY                 { $$ = new p6::address_of_node(LINE, $1); }
     | expr '+' expr                         { $$ = new cdk::add_node(LINE, $1, $3); }
     | expr '-' expr                         { $$ = new cdk::sub_node(LINE, $1, $3); }
     | expr '*' expr                         { $$ = new cdk::mul_node(LINE, $1, $3); }
     | expr '/' expr                         { $$ = new cdk::div_node(LINE, $1, $3); }
     | expr '%' expr                         { $$ = new cdk::mod_node(LINE, $1, $3); }
     | expr tEQ expr                         { $$ = new cdk::eq_node(LINE, $1, $3); }
     | expr tNE expr                         { $$ = new cdk::ne_node(LINE, $1, $3); }
     | expr '<' expr                         { $$ = new cdk::lt_node(LINE, $1, $3); }
     | expr '>' expr                         { $$ = new cdk::gt_node(LINE, $1, $3); }
     | expr tLE expr                         { $$ = new cdk::le_node(LINE, $1, $3); }
     | expr tGE expr                         { $$ = new cdk::ge_node(LINE, $1, $3); }
     | expr tAND expr                        { $$ = new cdk::and_node(LINE, $1, $3); }
     | expr tOR expr                         { $$ = new cdk::or_node(LINE, $1, $3); }
     | lval '=' expr                         { $$ = new cdk::assignment_node(LINE, $1, $3); }
     | '(' expr ')'                          { $$ = $2; }
     | '[' expr ']'                          { $$ = new p6::stack_alloc_node(LINE, $2); }
     | tSIZEOF '(' expr ')'                  { $$ = new p6::sizeof_node(LINE, $3); }
     | lval                                  { $$ = new cdk::rvalue_node(LINE, $1); }
     | tIDENTIFIER '(' ')'                   { $$ = new p6::function_call_node(LINE, *$1, new cdk::sequence_node(LINE)); delete $1; }
     | tIDENTIFIER '(' exprs ')'             { $$ = new p6::function_call_node(LINE, *$1, $3); delete $1; }
     ;

/* Strings */
str  : tSTRING                               { $$ = $1; }
     | str tSTRING                           { $$ = new std::string(*$1 + *$2); delete $1; delete $2; }
     ;

/* Left Values */
lval : tIDENTIFIER                           { $$ = new cdk::variable_node(LINE, $1); }
     | expr '[' expr ']'                     { $$ = new p6::index_node(LINE, $1, $3); }
     ;

%%
