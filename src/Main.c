

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "Compiler.h"
#include "Error.h"
#include "Function.h"
#include "Lexer.h"
#include "Outfile.h"
#include "Preprocessor.h"
#include "Token.h"

int main(int numArgs, char** args)
{
    if (!Outfile_TryOpen("out.s"))
        Error("Could not open \"./out.s\"!");

    Preprocessor_Define("CUSTOM_COMP");

    for (int i = 1; i < numArgs; i++)
    {
        TokenArray* arr = Lex(args[i]);
        Compile(*arr);
        Token_DeleteArray(arr);
        Preprocessor_Clear();
    }

    Preprocessor_End();
    Outfile_CloseFiles();
    return 0;
}
