/*****************************************************************************
 *  Vojnar's Army Tree Automata Library
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    The Flex grammar for Timbuk format parser
 *
 *****************************************************************************/

%option noyywrap
%option yylineno

%{
#include "timbuk.y.hh"
GCC_DIAG_OFF(unused-function)
%}



DIGIT        [0-9]
IDENTCHAR    [a-zA-Z_]|{DIGIT}
WHITE        [ \t\n]

NUMBER       {DIGIT}+
IDENT        {IDENTCHAR}+

%%

"Ops"           {return OPERATIONS;}
"Automaton"     {return AUTOMATON;}
"States"        {return STATES;}
"Final States"  {return FINAL_STATES;}  
"Transitions"   {return TRANSITIONS;}

{NUMBER}        {return NUMBER;}
{IDENT}         {return IDENTIFIER;}
{WHITE}

":"             {return COLON;}
"("             {return LPAR;}
")"             {return RPAR;}
"->"            {return ARROW;}

<<EOF>>         {return END_OF_FILE;}

%%

GCC_DIAG_ON(unused-function)