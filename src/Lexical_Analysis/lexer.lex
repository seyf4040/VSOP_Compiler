%{
    /* Includes */
    #include <string>

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
            if (yytext[i] == '\n') {
                loc.lines(1);
                loc.end.column = 1;
            } 
            else {
                loc.columns(1);
            }
        }
    }
%}

    /* Definitions */

blankspace					[ \t\n\r]
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

%%
%{
    // Code run each time yylex is called.
    loc.step();
%}

    /* Rules */
{whitespace}                /* */
{single_line_comment}       /* */
{type_identifier}			return Parser::make_TYPE_IDENTIFIER(yytext, loc);
{object_identifier}		    return Parser::make_OBJECT_IDENTIFIER(yytext, loc);

{integer_literal} {
    int res = stringToInt(yytext);
    if (res < 0) 
        print_error(loc.begin, "integer overflow: " + string(yytext));   
    else
        return Parser::make_INTEGER_LITERAL(res, loc);}

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
"<"         return Parser::make_LOWER(loc);
"<="        return Parser::make_LOWER_EQUAL(loc);
"<-"        return Parser::make_ASSIGN(loc);


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
