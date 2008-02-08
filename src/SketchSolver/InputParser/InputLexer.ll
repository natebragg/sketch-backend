%{

#include "InputParser.hpp"
#include <cstdlib>
#include "InputReader.h"

%}

%option lex-compat
%option noyywrap

Digit        ([0-9])
Integer      ({Digit}+)
HexInteger   ("0x"{Integer})
Double       ({Integer}"."{Digit}*("E"[\+\-]{Digit}+)?)
String       ("\""[^\n\"]*"\"")
Identifier   ([a-zA-Z_\#][a-zA-Z_0-9]*)
Operator     ([\%\/\<\>\;\!\?\*\-\+\,\.\:\[\]\(\)\{\}\=\|\&\^\$])
WhiteSpace   ([ \t\n]*)
Comment      ("//"[^\n]*)

%%

{Comment} { string tmp(yytext);
			if( tmp.find(":") < tmp.size() )
				context = string(yytext); 
			}

{WhiteSpace}	{ /* skip */ }

{Operator} {  return yytext[0]; }

{Double}   {
				yylval.doubleConst = atof(yytext);
				return T_dbl;
			}

("###NATIVE_CODE_BEGIN"[^\#]*"###NATIVE_CODE_END") { 				
				string tmp(yytext);
				tmp = tmp.substr(20, tmp.size() - ( 20 + 18 ) );
				yylval.strConst = new string(tmp);
				return T_NativeCode;
			}

{Integer}  {							
				yylval.intConst = atoi(yytext); 				
				return T_int;
			}
{HexInteger}  {	
				yylval.intConst = atoi(yytext); 
				return T_int;
			}

"null"  {							
				yylval.intConst = 0; 				
				return T_int;
			}
{String}   {								
				string tmp(yytext);
				tmp = tmp.substr(1, tmp.size() -2);
				yylval.strConst = new string(tmp);
				return T_string;
			}

"++"		{
				return T_ppls;
			}
"true"		{
				return T_true;
			}
"false"		{
				return T_false;
			}

"\$\$"		{
				return T_twoS;
			}

"--"		{
				return T_mmns;
			}
"=="		{
				return T_eq;
			}
"!="		{
				return T_neq;
			}

"&&"		{
				return T_and;
			}

"||"		{
				return T_or;
			}


">="		{
				return T_ge;
			}

"<="		{
				return T_le;
			}

("OUTPUT_"{Integer}) { 	
						yylval.strConst = new string(yytext); 
						return T_OutIdent; }

"int"	{
				yylval.variableType = INT;
				return T_vartype;
		}
		
"bit"	{
				yylval.variableType = BIT;
				return T_vartype;
		}

"for"		{
				
				return T_For;
			}	

"new"		{				
				return T_new;
			}
"add"		{
				return T_add;
			}

"assert" {
    return T_assert;
}

"Filter"	{
				
				return T_Filter;
			}

"Pipeline" {
				
				return T_Pipeline;
			}


"SplitJoin" {
				
				return T_SplitJoin;
			}

"Table"		{
				
				return T_Table;
			}

"NativeFilter"	{				
					return T_Native;
				}
"NATIVE_METHOD" { 				
					return T_NativeMethod;
				}
"INIT"		{
				return T_Init;
			}
			
"SKETCHES"	{
				return T_Sketches;
			}
						

"WORK"		{
				return T_Work;
			}

"input_RATE" {
				return T_InRate;
			}

"output_RATE" {
				return T_OutRate;
			}

"setSplitter"	{
					return T_setSplitter;
				}
"setJoiner"	{
				return T_setJoiner;
			}

{Identifier} {
	yylval.strConst = new string(yytext);
	return T_ident;
}

<<EOF>> {
			cout<<"End of File"<<endl;
			return T_eof;
		}

%%

void Inityylex(void){
	printf("Initializing scanner\n");
}
