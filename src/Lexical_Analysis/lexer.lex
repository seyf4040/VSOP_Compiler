%{
    /* Includes */
    #include <string>
    #include <stack>

    #include "utils.hpp"

    #include "parser.hpp"
    #include "driver.hpp"
%}

    /* Flex options
     * - noyywrap: yylex will not call yywrap() function
     * - nounput: do not generate yyunput() function
     * - noinput: do not generate yyinput() function
     * - batch: tell Flex that the lexer will not often be used interactively
     */
%option noyywrap nounput noinput batch

%{
    /* Code to include at the beginning of the lexer file. */
    using namespace std;
    using namespace VSOP;

    // Print an lexical error message.
    static void print_error(const position &pos,
                            const string &m);

    // Code run each time a pattern is matched.
    #define YY_USER_ACTION  loc_update();


    // Global variable used to maintain the current location.
    location loc;

    void loc_update() {
        loc.step();
        for (int i = 0; i < yyleng; ++i) {
            if (yytext[i] == '\n' || yytext[i] == '\f') {
                loc.lines(1);
                loc.end.column = 1;
            } 
            else {
                loc.columns(1);
            }
        }
    }

    // Stack to keep track of nested comments
    std::stack<location> locStack;

    void loc_push() {
        locStack.push(loc);
    }

    void loc_pop() {
        if (!locStack.empty()) {
            loc = locStack.top();
            locStack.pop();
        }
    }

    void loc_pop_all() {
        while (!locStack.empty()) {
            cout << locStack.empty() << endl;
            loc = locStack.top();  
            locStack.pop();
        }
    }
%}

    /* Definitions */

blankspace					[ \t\n\r\f]
whitespace					{blankspace}+

lowercase_letter    [a-z]
uppercase_letter    [A-Z]
letter      {lowercase_letter}|{uppercase_letter}
base_identifier		{letter}|{digit}|_
type_identifier	    {uppercase_letter}{base_identifier}*
object_identifier	{lowercase_letter}{base_identifier}*

digit [0-9]
hex_digit {digit}|[a-f]|[A-F]

hex_prefix "0x"
base10_number {digit}+
base16_number {hex_digit}+

integer_literal	{base10_number}|{hex_prefix}{base16_number}

/* 
regular_char				[^\0\n\"\\]
escaped_char				[btnr\"\\]|x{hex_digit}{2}|(\n[ \t]*)
escape_sequence				\\{escaped_char}
 */

single_line_comment			"\/\/"[^\0\n]*

%x COMMENT STRING

%%
%{
    // Code run each time yylex is called.
    loc.step();
%}

    /* Rules */
{whitespace}                /* */
{single_line_comment}       /* */

    /* KeyWords */
"and"       return Parser::make_AND(loc);
"bool"      return Parser::make_BOOL(loc);
"class"     return Parser::make_CLASS(loc);
"do"        return Parser::make_DO(loc);
"else"      return Parser::make_ELSE(loc);
"extends"   return Parser::make_EXTENDS(loc);
"false"     return Parser::make_FALSE(loc);
"if"        return Parser::make_IF(loc);
"in"        return Parser::make_IN(loc);
"int32"     return Parser::make_INT32(loc);
"isnull"    return Parser::make_ISNULL(loc);
"let"       return Parser::make_LET(loc);
"new"       return Parser::make_NEW(loc);
"not"       return Parser::make_NOT(loc);
"self"      return Parser::make_SELF(loc);
"string"    return Parser::make_STRING(loc);
"then"      return Parser::make_THEN(loc);
"true"      return Parser::make_TRUE(loc);
"unit"      return Parser::make_UNIT(loc);
"while"     return Parser::make_WHILE(loc);

{type_identifier}			return Parser::make_TYPE_IDENTIFIER(yytext, loc);
{object_identifier}		    return Parser::make_OBJECT_IDENTIFIER(yytext, loc);

{integer_literal} {
    int res = stringToInt(yytext);
    if (res < 0) 
        print_error(loc.begin, "integer overflow: " + string(yytext));   
    else
        return Parser::make_INTEGER_LITERAL(res, loc);}

"(*"						loc_push(); BEGIN(COMMENT);
\"                          loc_push(); BEGIN(STRING);
                        
    /* Operators */
"{"         return Parser::make_LBRACE(loc);
"}"         return Parser::make_RBRACE(loc);
"("         return Parser::make_LPAR(loc);
")"         return Parser::make_RPAR(loc);
":"         return Parser::make_COLON(loc);
";"         return Parser::make_SEMICOLON(loc);
","         return Parser::make_COMMA(loc);
"+"         return Parser::make_PLUS(loc);
"-"         return Parser::make_MINUS(loc);
"*"         return Parser::make_TIMES(loc);
"/"         return Parser::make_DIV(loc);
"^"         return Parser::make_POW(loc);
"."         return Parser::make_DOT(loc);
"="         return Parser::make_EQUAL(loc);
"<="        return Parser::make_LOWER_EQUAL(loc);
"<-"        return Parser::make_ASSIGN(loc);
"<"         return Parser::make_LOWER(loc);

<COMMENT>"(*"       loc_push();
<COMMENT>"*)"       loc_pop(); if (locStack.empty()) BEGIN(INITIAL);
<COMMENT>[^\0]		    /* */
<COMMENT><<EOF>>    {
                        loc_pop_all();
                        print_error(loc.begin, "lexical error: Reached end of file while multiline comment not terminated." );
                        return Parser::make_YYerror(loc);
                    }



<STRING>\"         loc_pop();
<STRING>\"       loc_pop(); if (locStack.empty()) BEGIN(INITIAL);
<STRING><<EOF>>    {
                        loc_pop_all();
                        print_error(loc.begin, "lexical error: Reached end of file inside string-literal." );
                        return Parser::make_YYerror(loc);
                    }


    /* Invalid characters */
.           {
                print_error(loc.begin, "invalid character: " + string(yytext));
                return Parser::make_YYerror(loc);
            }
    
    /* End of file */
<<EOF>>     return Parser::make_YYEOF(loc);
%%

    /* User code */

static void print_error(const position &pos, const string &m)
{
    cerr << *(pos.filename) << ":"
         << pos.line << ":"
         << pos.column << ":"
         << " lexical error: "
         << m
         << endl;
}

void Driver::scan_begin()
{
    loc.initialize(&source_file);

    if (source_file.empty() || source_file == "-")
        yyin = stdin;
    else if (!(yyin = fopen(source_file.c_str(), "r")))
    {
        cerr << "cannot open " << source_file << ": " << strerror(errno) << '\n';
        exit(EXIT_FAILURE);
    }
}

void Driver::scan_end()
{
    fclose(yyin);
}
