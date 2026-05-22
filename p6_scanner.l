%option c++ prefix="p6_scanner_" outfile="p6_scanner.cpp"
%option stack noyywrap yylineno 8bit debug
%{ 
// make relevant includes before including the parser's tab file
#include <string>
#include <cdk/ast/sequence_node.h>
#include <cdk/ast/expression_node.h>
#include <cdk/ast/lvalue_node.h>
#include "p6_parser.tab.h"
#include <cerrno>
#include <cstdlib>

// don't change this
#define yyerror LexerError

int comment_nesting = 0;
%}
%x X_STRING
%x X_COMMENT
%x X_STRING_IGNORE
%%
  yydebug=1; set_debug(1);

  /* Comments (ignored) */
"//".*                        ;
"/*"                          { yy_push_state(X_COMMENT); comment_nesting = 1; }
<X_COMMENT>"/*"               comment_nesting++;
<X_COMMENT>"*/"               { comment_nesting--; if (comment_nesting == 0) yy_pop_state(); }
<X_COMMENT><<EOF>>            yyerror("Unterminated comment");
<X_COMMENT>.|\n               ;

  /* Operators */
[-+*/%<>~?=!()\[\]{},;]       return *yytext; // Single-character tokens
"->"                          return tARROW;
"=="                          return tEQ;
"!="                          return tNE; 
"<="                          return tLE; 
">="                          return tGE;
"&&"                          return tAND;
"||"                          return tOR;
"!!"                          return tPRINT_NL;

  /* Keywords */ 
"int"                         return tINT;
"real"                        return tREAL;
"string"                      return tSTRING_TYPE;
"void"                        return tVOID;
"extern"                      return tEXTERN;
"forward"                     return tFORWARD;
"public"                      return tPUBLIC;
"auto"                        return tAUTO;
"if"                          return tIF;
"elif"                        return tELIF;
"else"                        return tELSE;
"while"                       return tWHILE;
"stop"                        return tSTOP;
"next"                        return tNEXT;
"return"                      return tRETURN;
"input"                       return tREAD;
"null"                        return tNULL;
"sizeof"                      return tSIZEOF;
"begin"                       return tBEGIN;
"end"                         return tEND;

  /* Identifiers */
[A-Za-z][A-Za-z0-9]*          yylval.s = new std::string(yytext); return tIDENTIFIER;

  /* String Literals */
\"                            yy_push_state(X_STRING); yylval.s = new std::string("");
<X_STRING>\\n                 *yylval.s += "\n";
<X_STRING>\\t                 *yylval.s += "\t";
<X_STRING>\\r                 *yylval.s += "\r";
<X_STRING>\\\\                *yylval.s += "\\";
<X_STRING>\\\"                *yylval.s += "\"";
<X_STRING>\n                  yyerror("newline in string");
<X_STRING>\"                  yy_pop_state(); return tSTRING;

  /* Octal escape sequences */
<X_STRING>\\[0-7]{1,3}        {
                                char c = (char)strtol(yytext + 1, nullptr, 8);
                                if (c == '\0') yy_push_state(X_STRING_IGNORE);
                                else *yylval.s += c;
                              }

  /* EOF, NUL, and Invalid Escapes */
<X_STRING><<EOF>>             yyerror("Unterminated string"); 
<X_STRING>\0                  yyerror("ASCII NUL character in string");
<X_STRING>\\[^0-7ntr\\"\n]    yyerror("Invalid escape sequence in string");

  /* End of string */
<X_STRING>.                   *yylval.s += yytext;

  /* ignore all after \0 */
<X_STRING_IGNORE>\"           { yy_pop_state(); yy_pop_state(); return tSTRING; }
<X_STRING_IGNORE>\n           yyerror("newline in string");
<X_STRING_IGNORE>\0           yyerror("ASCII NUL character in string");
<X_STRING_IGNORE><<EOF>>      yyerror("Unterminated string");
<X_STRING_IGNORE>\\.          ;
<X_STRING_IGNORE>.            ;

  /* Numeric Literals */
  /* Float & Exponential Notation Literals. */
[0-9]*\.[0-9]+([eE][-+]?[0-9]+)?  { yylval.s = new std::string(yytext); return tREAL_LITERAL; }
[0-9]+\.[0-9]*([eE][-+]?[0-9]+)?  { yylval.s = new std::string(yytext); return tREAL_LITERAL; }
[0-9]+[eE][-+]?[0-9]+             { yylval.s = new std::string(yytext); return tREAL_LITERAL; }

  /* Base 3 literals */
0t[-+0]+            {
                      long long val = 0;
                      const long long MAX_TRIT_VAL = 6078832729528464400LL; // (3^40 - 1) / 2
                      bool overflow = false;
                      for (int i = 2; yytext[i] != '\0'; i++) {
                        if (val > MAX_TRIT_VAL / 3 + 1 || val < -MAX_TRIT_VAL / 3 - 1) {
                            overflow = true;
                            break;
                        }

                        val *= 3;
                        if (yytext[i] == '+')      val += 1;
                        else if (yytext[i] == '-') val -= 1;

                        if (val > MAX_TRIT_VAL || val < -MAX_TRIT_VAL) {
                            overflow = true;
                            break;
                        }
                      }
                      if (overflow) yyerror("Integer literal out of range (exceeds 40 trits)");

                      yylval.i = val;
                      return tINTEGER;
                    }

  /* Base 9 literals */
0[0-8]*9[0-9]*      yyerror("Invalid base-9 literal (contains 9)");
0[0-8]*             {
                        errno = 0;
                        yylval.i = strtol(yytext, nullptr, 9); 
                        if (errno == ERANGE) yyerror("Integer literal out of range");
                        return tINTEGER; 
                    }

  /* Base 10 literals */
[1-9][0-9]*         { errno = 0; yylval.i = strtol(yytext, nullptr, 10); if (errno == ERANGE) yyerror("Integer literal out of range"); return tINTEGER; }

  /* ignore whitespace */
[ \t\n\r]+          ;

  /* catch-all for unknown characters */
.                   yyerror("Unknown character");

%%
