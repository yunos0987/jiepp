%top{
#include "constfold_parser.hpp"
}
%{
#include "constfold/constfold_internal.hpp"
#include "env/issue.hpp"
extern CfValue cflval;  // defined in constfold_parser.cpp (bison %code section)
#include <cctype>
#include <cstring>
#include <string>
#include <stdexcept>
%}

%option noyywrap nodefault 8bit nounistd never-interactive
%option prefix="cf"

DIGIT   [0-9]
DIGITS  [0-9](_?[0-9])*
ALNUM   [0-9A-Za-z_]
ALPHA   [A-Za-z_]

%%

[ \t\n\r\f\v]+   { /* skip whitespace */ }

"\\not\\"   { return cf::CfParser::token::CF_LNOT; }
"\\and\\"   { return cf::CfParser::token::CF_LAND; }
"\\or\\"    { return cf::CfParser::token::CF_LOR; }
"\\xor\\"   { return cf::CfParser::token::CF_LXOR; }

[0-9]+#[+-]?[0-9A-Za-z][0-9A-Za-z_]* {
    const char* p = yytext;
    int base = 0;
    while (*p != '#') { base = base * 10 + (*p - '0'); ++p; }
    ++p; // skip '#'
    if (base < 2 || base > 36) {
        ISSUE(INVALID_EXPRESSION, std::string("invalid base: ") + yytext);
        cflval = CfValue::int_value(0);
        return cf::CfParser::token::CF_INT;
    }
    bool neg = false;
    if (*p == '-') { neg = true; ++p; }
    else if (*p == '+') { ++p; }
    int64_t ival = 0;
    for (; *p; ++p) {
        char ch = *p;
        if (ch == '_') continue;
        int d = std::isdigit((unsigned char)ch) ? (ch - '0') :
                std::islower((unsigned char)ch) ? (ch - 'a' + 10) : (ch - 'A' + 10);
        if (d >= base) {
            ISSUE(INVALID_EXPRESSION, std::string("invalid digit for base: ") + yytext);
            cflval = CfValue::int_value(0);
            return cf::CfParser::token::CF_INT;
        }
        ival = ival * base + d;
    }
    if (neg) ival = -ival;
    cflval = CfValue::int_value(ival);
    return cf::CfParser::token::CF_INT;
}

{DIGITS}"."{DIGITS}([eE][+-]?{DIGITS})?  {
    // I2: IEC 61131-3 digit separators are allowed in plain decimal/float
    // literals too, not only based ones (mirrors the based-literal rule
    // above); strip them before handing the text to stod.
    std::string clean;
    clean.reserve(yyleng);
    for (const char* p = yytext; *p; ++p)
        if (*p != '_') clean.push_back(*p);
    double fval = 0.0;
    try {
        fval = std::stod(clean);
    } catch (const std::out_of_range&) {
        ISSUE(INVALID_EXPRESSION, std::string("float literal overflow: ") + yytext);
    }
    cflval = CfValue::float_value(fval);
    return cf::CfParser::token::CF_FLOAT;
}

{DIGITS}  {
    // I2: strip digit separators before stoll, mirroring the based-literal rule.
    std::string clean;
    clean.reserve(yyleng);
    for (const char* p = yytext; *p; ++p)
        if (*p != '_') clean.push_back(*p);
    int64_t ival = 0;
    try {
        ival = std::stoll(clean);
    } catch (const std::out_of_range&) {
        ISSUE(INVALID_EXPRESSION, std::string("integer literal overflow: ") + yytext);
    }
    cflval = CfValue::int_value(ival);
    return cf::CfParser::token::CF_INT;
}

"true"|"TRUE"                                 { return cf::CfParser::token::CF_TRUE; }
"false"|"FALSE"                               { return cf::CfParser::token::CF_FALSE; }
[nN][oO][tT]                                  { return cf::CfParser::token::CF_NOT; }
[aA][nN][dD]                                  { return cf::CfParser::token::CF_AND; }
[oO][rR]                                      { return cf::CfParser::token::CF_OR; }
[xX][oO][rR]                                  { return cf::CfParser::token::CF_XOR; }
[mM][oO][dD]                                  { return cf::CfParser::token::CF_MOD; }
[bB][oO][oO][lL]                              { cflval.text = "BOOL";  return cf::CfParser::token::CF_TYPE_KW; }
[bB][yY][tT][eE]                              { cflval.text = "BYTE";  return cf::CfParser::token::CF_TYPE_KW; }
[dD][wW][oO][rR][dD]                          { cflval.text = "DWORD"; return cf::CfParser::token::CF_TYPE_KW; }
[wW][oO][rR][dD]                              { cflval.text = "WORD";  return cf::CfParser::token::CF_TYPE_KW; }
[lL][wW][oO][rR][dD]                          { cflval.text = "LWORD"; return cf::CfParser::token::CF_TYPE_KW; }

[A-Za-z_][A-Za-z0-9_]*  {
    cflval = CfValue::int_value(0);
    cflval.text = yytext;
    return cf::CfParser::token::CF_IDENT;
}

"<<"    { return cf::CfParser::token::CF_SHL; }
">>"    { return cf::CfParser::token::CF_SHR; }
"<>"    { return cf::CfParser::token::CF_NE; }
"<="    { return cf::CfParser::token::CF_LE; }
">="    { return cf::CfParser::token::CF_GE; }
"<"     { return cf::CfParser::token::CF_LT; }
">"     { return cf::CfParser::token::CF_GT; }
"="     { return cf::CfParser::token::CF_EQ; }
"("     { return cf::CfParser::token::CF_LPAREN; }
")"     { return cf::CfParser::token::CF_RPAREN; }
"+"     { return cf::CfParser::token::CF_PLUS; }
"-"     { return cf::CfParser::token::CF_MINUS; }
"*"     { return cf::CfParser::token::CF_STAR; }
"/"     { return cf::CfParser::token::CF_SLASH; }
"&"     { return cf::CfParser::token::CF_AMP; }
"#"     { return cf::CfParser::token::CF_HASH; }

.   { return cf::CfParser::token::CF_UNKNOWN; }

%%
