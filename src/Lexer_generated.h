#pragma once
#include "Token.h"
#include "Util.h"
#include <stdbool.h>

bool IsNonIDChar(char c);

size_t TokenizeSwitch(char* code, TokenType* token, void** tokenData, size_t i)
{
    // Everything in this following switch is auto-generated using util/GenerateParser_main.c.
    // While the code obviously looks terrible, this is a fast and easy way to parse.
    switch (code[i + 0])
    {
        case '!':
            if (code[i + 1] == '=')
            {
                *token = NotEquals;
                return i + 2;
            }
            *token = LogicalNOT;
            return i + 1;
        case '%':
            if (code[i + 1] == '=')
            {
                *token = AssignmentMod;
                return i + 2;
            }
            *token = Percent;
            return i + 1;
        case '&':
            switch (code[i + 1])
            {
                case '&':
                    *token = LogicalAND;
                    return i + 2;
                case '=':
                    *token = AssignmentAND;
                    return i + 2;
            }
            *token = Ampersand;
            return i + 1;
        case '(':
            *token = RBrOpen;
            return i + 1;
        case ')':
            *token = RBrClose;
            return i + 1;
        case '*':
            if (code[i + 1] == '=')
            {
                *token = AssignmentMul;
                return i + 2;
            }
            *token = Star;
            return i + 1;
        case '+':
            switch (code[i + 1])
            {
                case '+':
                    *token = Increment;
                    return i + 2;
                case '=':
                    *token = AssignmentAdd;
                    return i + 2;
            }
            *token = Plus;
            return i + 1;
        case ',':
            *token = Comma;
            return i + 1;
        case '-':
            switch (code[i + 1])
            {
                case '-':
                    *token = Decrement;
                    return i + 2;
                case '=':
                    *token = AssignmentSub;
                    return i + 2;
                case '>':
                    *token = Arrow;
                    return i + 2;
            }
            *token = Minus;
            return i + 1;
        case '.':
            if (code[i + 1] == '.')
            {
                if (code[i + 2] == '.')
                {
                    *token = DotDotDot;
                    return i + 3;
                }
            }
            *token = Dot;
            return i + 1;
        case '/':
            if (code[i + 1] == '=')
            {
                *token = AssignmentDiv;
                return i + 2;
            }
            *token = Slash;
            return i + 1;
        case ':':
            *token = Colon;
            return i + 1;
        case ';':
            *token = Semicolon;
            return i + 1;
        case '<':
            switch (code[i + 1])
            {
                case '<':
                    if (code[i + 2] == '=')
                    {
                        *token = AssignmentShiftLeft;
                        return i + 3;
                    }
                    *token = ShiftLeft;
                    return i + 2;
                case '=':
                    *token = LessThanEq;
                    return i + 2;
            }
            *token = LessThan;
            return i + 1;
        case '=':
            if (code[i + 1] == '=')
            {
                *token = Equals;
                return i + 2;
            }
            *token = Assignment;
            return i + 1;
        case '>':
            switch (code[i + 1])
            {
                case '=':
                    *token = GreaterThanEq;
                    return i + 2;
                case '>':
                    if (code[i + 2] == '=')
                    {
                        *token = AssignmentShiftRight;
                        return i + 3;
                    }
                    *token = ShiftRight;
                    return i + 2;
            }
            *token = GreaterThan;
            return i + 1;
        case '?':
            *token = QuestionMark;
            return i + 1;
        case 'N':
            if (code[i + 1] == 'U')
            {
                if (code[i + 2] == 'L')
                {
                    if (code[i + 3] == 'L')
                    {
                        if (IsNonIDChar(code[i + 4]))
                        {
                            *token = IntLiteral;
                            int32_t* lit = xmalloc(sizeof(int32_t));
                            *lit = 0;
                            *tokenData = lit;
                            return i + 4;
                        }
                    }
                }
            }
            return 0;
        case '[':
            *token = ABrOpen;
            return i + 1;
        case ']':
            *token = ABrClose;
            return i + 1;
        case '^':
            if (code[i + 1] == '=')
            {
                *token = AssignmentXOR;
                return i + 2;
            }
            *token = BitwiseXOR;
            return i + 1;
        case 'b':
            switch (code[i + 1])
            {
                case 'o':
                    if (code[i + 2] == 'o')
                    {
                        if (code[i + 3] == 'l')
                        {
                            if (IsNonIDChar(code[i + 4]))
                            {
                                *token = UintKeyword;
                                return i + 4;
                            }
                        }
                    }
                    return 0;
                case 'r':
                    if (code[i + 2] == 'e')
                    {
                        if (code[i + 3] == 'a')
                        {
                            if (code[i + 4] == 'k')
                            {
                                if (IsNonIDChar(code[i + 5]))
                                {
                                    *token = BreakKeyword;
                                    return i + 5;
                                }
                            }
                        }
                    }
                    return 0;
            }
            return 0;
        case 'c':
            switch (code[i + 1])
            {
                case 'a':
                    if (code[i + 2] == 's')
                    {
                        if (code[i + 3] == 'e')
                        {
                            if (IsNonIDChar(code[i + 4]))
                            {
                                *token = CaseKeyword;
                                return i + 4;
                            }
                        }
                    }
                    return 0;
                case 'h':
                    if (code[i + 2] == 'a')
                    {
                        if (code[i + 3] == 'r')
                        {
                            if (IsNonIDChar(code[i + 4]))
                            {
                                *token = UintKeyword;
                                return i + 4;
                            }
                        }
                    }
                    return 0;
                case 'o':
                    if (code[i + 2] == 'n')
                    {
                        switch (code[i + 3])
                        {
                            case 's':
                                if (code[i + 4] == 't')
                                {
                                    if (IsNonIDChar(code[i + 5]))
                                    {
                                        *token = ConstKeyword;
                                        return i + 5;
                                    }
                                }
                                return 0;
                            case 't':
                                if (code[i + 4] == 'i')
                                {
                                    if (code[i + 5] == 'n')
                                    {
                                        if (code[i + 6] == 'u')
                                        {
                                            if (code[i + 7] == 'e')
                                            {
                                                if (IsNonIDChar(code[i + 8]))
                                                {
                                                    *token = ContinueKeyword;
                                                    return i + 8;
                                                }
                                            }
                                        }
                                    }
                                }
                                return 0;
                        }
                    }
                    return 0;
            }
            return 0;
        case 'd':
            switch (code[i + 1])
            {
                case 'e':
                    if (code[i + 2] == 'f')
                    {
                        if (code[i + 3] == 'a')
                        {
                            if (code[i + 4] == 'u')
                            {
                                if (code[i + 5] == 'l')
                                {
                                    if (code[i + 6] == 't')
                                    {
                                        if (IsNonIDChar(code[i + 7]))
                                        {
                                            *token = DefaultKeyword;
                                            return i + 7;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    return 0;
                case 'o':
                    if (IsNonIDChar(code[i + 2]))
                    {
                        *token = DoKeyword;
                        return i + 2;
                    }
            }
            return 0;
        case 'e':
            switch (code[i + 1])
            {
                case 'l':
                    if (code[i + 2] == 's')
                    {
                        if (code[i + 3] == 'e')
                        {
                            if (IsNonIDChar(code[i + 4]))
                            {
                                *token = ElseKeyword;
                                return i + 4;
                            }
                        }
                    }
                    return 0;
                case 'n':
                    if (code[i + 2] == 'u')
                    {
                        if (code[i + 3] == 'm')
                        {
                            if (IsNonIDChar(code[i + 4]))
                            {
                                *token = EnumKeyword;
                                return i + 4;
                            }
                        }
                    }
                    return 0;
            }
            return 0;
        case 'f':
            switch (code[i + 1])
            {
                case 'a':
                    if (code[i + 2] == 'l')
                    {
                        if (code[i + 3] == 's')
                        {
                            if (code[i + 4] == 'e')
                            {
                                if (IsNonIDChar(code[i + 5]))
                                {
                                    *token = IntLiteral;
                                    int32_t* lit = xmalloc(sizeof(int32_t));
                                    *lit = 0;
                                    *tokenData = lit;
                                    return i + 5;
                                }
                            }
                        }
                    }
                    return 0;
                case 'i':
                    if (code[i + 2] == 'x')
                    {
                        if (code[i + 3] == 'e')
                        {
                            if (code[i + 4] == 'd')
                            {
                                if (IsNonIDChar(code[i + 5]))
                                {
                                    *token = FixedKeyword;
                                    return i + 5;
                                }
                            }
                        }
                    }
                    return 0;
                case 'o':
                    if (code[i + 2] == 'r')
                    {
                        if (IsNonIDChar(code[i + 3]))
                        {
                            *token = ForKeyword;
                            return i + 3;
                        }
                    }
                    return 0;
            }
            return 0;
        case 'g':
            if (code[i + 1] == 'o')
            {
                if (code[i + 2] == 't')
                {
                    if (code[i + 3] == 'o')
                    {
                        if (IsNonIDChar(code[i + 4]))
                        {
                            *token = GotoKeyword;
                            return i + 4;
                        }
                    }
                }
            }
            return 0;
        case 'i':
            switch (code[i + 1])
            {
                case 'f':
                    if (IsNonIDChar(code[i + 2]))
                    {
                        *token = IfKeyword;
                        return i + 2;
                    }
                    return 0;
                case 'n':
                    if (code[i + 2] == 't')
                    {
                        switch (code[i + 3])
                        {
                            case '1':
                                if (code[i + 4] == '6')
                                {
                                    if (code[i + 5] == '_')
                                    {
                                        if (code[i + 6] == 't')
                                        {
                                            if (IsNonIDChar(code[i + 7]))
                                            {
                                                *token = IntKeyword;
                                                return i + 7;
                                            }
                                        }
                                    }
                                    if (IsNonIDChar(code[i + 5]))
                                    {
                                        *token = IntKeyword;
                                        return i + 5;
                                    }
                                }
                                return 0;
                            case '3':
                                if (code[i + 4] == '2')
                                {
                                    if (code[i + 5] == '_')
                                    {
                                        if (code[i + 6] == 't')
                                        {
                                            if (IsNonIDChar(code[i + 7]))
                                            {
                                                *token = Int32Keyword;
                                                return i + 7;
                                            }
                                        }
                                    }
                                    if (IsNonIDChar(code[i + 5]))
                                    {
                                        *token = Int32Keyword;
                                        return i + 5;
                                    }
                                }
                                return 0;
                        }
                        if (IsNonIDChar(code[i + 3]))
                        {
                            *token = IntKeyword;
                            return i + 3;
                        }
                    }
                    return 0;
            }
            return 0;
        case 'l':
            if (code[i + 1] == 'o')
            {
                if (code[i + 2] == 'n')
                {
                    if (code[i + 3] == 'g')
                    {
                        if (IsNonIDChar(code[i + 4]))
                        {
                            *token = Int32Keyword;
                            return i + 4;
                        }
                    }
                }
            }
            return 0;
        case 'r':
            if (code[i + 1] == 'e')
            {
                switch (code[i + 2])
                {
                    case 'g':
                        if (code[i + 3] == 'i')
                        {
                            if (code[i + 4] == 's')
                            {
                                if (code[i + 5] == 't')
                                {
                                    if (code[i + 6] == 'e')
                                    {
                                        if (code[i + 7] == 'r')
                                        {
                                            if (IsNonIDChar(code[i + 8]))
                                            {
                                                *token = RegisterKeyword;
                                                return i + 8;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        return 0;
                    case 's':
                        if (code[i + 3] == 't')
                        {
                            if (code[i + 4] == 'r')
                            {
                                if (code[i + 5] == 'i')
                                {
                                    if (code[i + 6] == 'c')
                                    {
                                        if (code[i + 7] == 't')
                                        {
                                            if (IsNonIDChar(code[i + 8]))
                                            {
                                                *token = RestrictKeyword;
                                                return i + 8;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        return 0;
                    case 't':
                        if (code[i + 3] == 'u')
                        {
                            if (code[i + 4] == 'r')
                            {
                                if (code[i + 5] == 'n')
                                {
                                    if (IsNonIDChar(code[i + 6]))
                                    {
                                        *token = ReturnKeyword;
                                        return i + 6;
                                    }
                                }
                            }
                        }
                        return 0;
                }
            }
            return 0;
        case 's':
            switch (code[i + 1])
            {
                case 'i':
                    if (code[i + 2] == 'z')
                    {
                        if (code[i + 3] == 'e')
                        {
                            switch (code[i + 4])
                            {
                                case '_':
                                    if (code[i + 5] == 't')
                                    {
                                        if (IsNonIDChar(code[i + 6]))
                                        {
                                            *token = UintKeyword;
                                            return i + 6;
                                        }
                                    }
                                    return 0;
                                case 'o':
                                    if (code[i + 5] == 'f')
                                    {
                                        if (IsNonIDChar(code[i + 6]))
                                        {
                                            *token = SizeOfKeyword;
                                            return i + 6;
                                        }
                                    }
                                    return 0;
                            }
                        }
                    }
                    return 0;
                case 't':
                    switch (code[i + 2])
                    {
                        case 'a':
                            if (code[i + 3] == 't')
                            {
                                if (code[i + 4] == 'i')
                                {
                                    if (code[i + 5] == 'c')
                                    {
                                        if (IsNonIDChar(code[i + 6]))
                                        {
                                            *token = StaticKeyword;
                                            return i + 6;
                                        }
                                    }
                                }
                            }
                            return 0;
                        case 'r':
                            if (code[i + 3] == 'u')
                            {
                                if (code[i + 4] == 'c')
                                {
                                    if (code[i + 5] == 't')
                                    {
                                        if (IsNonIDChar(code[i + 6]))
                                        {
                                            *token = StructKeyword;
                                            return i + 6;
                                        }
                                    }
                                }
                            }
                            return 0;
                    }
                    return 0;
                case 'w':
                    if (code[i + 2] == 'i')
                    {
                        if (code[i + 3] == 't')
                        {
                            if (code[i + 4] == 'c')
                            {
                                if (code[i + 5] == 'h')
                                {
                                    if (IsNonIDChar(code[i + 6]))
                                    {
                                        *token = SwitchKeyword;
                                        return i + 6;
                                    }
                                }
                            }
                        }
                    }
                    return 0;
            }
            return 0;
        case 't':
            switch (code[i + 1])
            {
                case 'r':
                    if (code[i + 2] == 'u')
                    {
                        if (code[i + 3] == 'e')
                        {
                            if (IsNonIDChar(code[i + 4]))
                            {
                                *token = IntLiteral;
                                int32_t* lit = xmalloc(sizeof(int32_t));
                                *lit = 1;
                                *tokenData = lit;
                                return i + 4;
                            }
                        }
                    }
                    return 0;
                case 'y':
                    if (code[i + 2] == 'p')
                    {
                        if (code[i + 3] == 'e')
                        {
                            if (code[i + 4] == 'd')
                            {
                                if (code[i + 5] == 'e')
                                {
                                    if (code[i + 6] == 'f')
                                    {
                                        if (IsNonIDChar(code[i + 7]))
                                        {
                                            *token = TypedefKeyword;
                                            return i + 7;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    return 0;
            }
            return 0;
        case 'u':
            switch (code[i + 1])
            {
                case 'i':
                    if (code[i + 2] == 'n')
                    {
                        if (code[i + 3] == 't')
                        {
                            switch (code[i + 4])
                            {
                                case '1':
                                    if (code[i + 5] == '6')
                                    {
                                        if (code[i + 6] == '_')
                                        {
                                            if (code[i + 7] == 't')
                                            {
                                                if (IsNonIDChar(code[i + 8]))
                                                {
                                                    *token = UintKeyword;
                                                    return i + 8;
                                                }
                                            }
                                        }
                                        if (IsNonIDChar(code[i + 6]))
                                        {
                                            *token = UintKeyword;
                                            return i + 6;
                                        }
                                    }
                                    return 0;
                                case '3':
                                    if (code[i + 5] == '2')
                                    {
                                        if (code[i + 6] == '_')
                                        {
                                            if (code[i + 7] == 't')
                                            {
                                                if (IsNonIDChar(code[i + 8]))
                                                {
                                                    *token = Uint32Keyword;
                                                    return i + 8;
                                                }
                                            }
                                        }
                                        if (IsNonIDChar(code[i + 6]))
                                        {
                                            *token = Uint32Keyword;
                                            return i + 6;
                                        }
                                    }
                                    return 0;
                            }
                            if (IsNonIDChar(code[i + 4]))
                            {
                                *token = UintKeyword;
                                return i + 4;
                            }
                        }
                    }
                    return 0;
                case 'n':
                    if (code[i + 2] == 'i')
                    {
                        if (code[i + 3] == 'o')
                        {
                            if (code[i + 4] == 'n')
                            {
                                if (IsNonIDChar(code[i + 5]))
                                {
                                    *token = UnionKeyword;
                                    return i + 5;
                                }
                            }
                        }
                    }
                    return 0;
            }
            return 0;
        case 'v':
            if (code[i + 1] == 'o')
            {
                if (code[i + 2] == 'i')
                {
                    if (code[i + 3] == 'd')
                    {
                        if (IsNonIDChar(code[i + 4]))
                        {
                            *token = VoidKeyword;
                            return i + 4;
                        }
                    }
                }
            }
            return 0;
        case 'w':
            if (code[i + 1] == 'h')
            {
                if (code[i + 2] == 'i')
                {
                    if (code[i + 3] == 'l')
                    {
                        if (code[i + 4] == 'e')
                        {
                            if (IsNonIDChar(code[i + 5]))
                            {
                                *token = WhileKeyword;
                                return i + 5;
                            }
                        }
                    }
                }
            }
            return 0;
        case '{':
            *token = CBrOpen;
            return i + 1;
        case '|':
            switch (code[i + 1])
            {
                case '=':
                    *token = AssignmentOR;
                    return i + 2;
                case '|':
                    *token = LogicalOR;
                    return i + 2;
            }
            *token = BitwiseOR;
            return i + 1;
        case '}':
            *token = CBrClose;
            return i + 1;
        case '~':
            *token = BitwiseNOT;
            return i + 1;
    }
    return 0;
}

bool ParseNext(char* code, TokenArray* t, char* sourceFileName, size_t* i, int lineNumber)
{
    TokenType token = None;
    void* tokenData = NULL;

    size_t newI = TokenizeSwitch(code, &token, &tokenData, *i);
    if (newI == 0)
        return false;

    *i = newI;

    Token_AppendArray((Token){token, tokenData}, t, lineNumber, sourceFileName);
    return true;
}