#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Keyword
{

    char* name;
    char* tokenType;
    bool strict;

} Keyword;

Keyword keywords[] = {
    {"void", "VoidKeyword", true},
    {"struct", "StructKeyword", true},
    {"bool", "UintKeyword", true},
    {"int", "IntKeyword", true},
    {"int16", "IntKeyword", true},
    {"int16_t", "IntKeyword", true},
    {"int32", "Int32Keyword", true},
    {"long", "Int32Keyword", true},
    {"int32_t", "Int32Keyword", true},
    {"uint", "UintKeyword", true},
    {"uint16", "UintKeyword", true},
    {"uint16_t", "UintKeyword", true},
    {"size_t", "UintKeyword", true},
    {"char", "UintKeyword", true},
    {"uint32", "Uint32Keyword", true},
    {"uint32_t", "Uint32Keyword", true},
    {"fixed", "FixedKeyword", true},
    {"if", "IfKeyword", true},
    {"else", "ElseKeyword", true},
    {"while", "WhileKeyword", true},
    {"do", "DoKeyword", true},
    {"for", "ForKeyword", true},
    {"return", "ReturnKeyword", true},
    {"continue", "ContinueKeyword", true},
    {"break", "BreakKeyword", true},
    {"switch", "SwitchKeyword", true},
    {"case", "CaseKeyword", true},
    {"default", "DefaultKeyword", true},
    {"enum", "EnumKeyword", true},
    {"typedef", "TypedefKeyword", true},
    {"sizeof", "SizeOfKeyword", true},
    {"const", "ConstKeyword", true},
    {"restrict", "RestrictKeyword", true},
    {"goto", "GotoKeyword", true},
    {"register", "RegisterKeyword", true},
    {"union", "UnionKeyword", true},
    {"static", "StaticKeyword", true},
    {"NULL", "NULL", true},
    {"true", "TRUE", true},
    {"false", "FALSE", true},
    {"...", "DotDotDot", false},
    {"(", "RBrOpen", false},
    {")", "RBrClose", false},
    {"{", "CBrOpen", false},
    {"}", "CBrClose", false},
    {"[", "ABrOpen", false},
    {"]", "ABrClose", false},
    {";", "Semicolon", false},
    {"=", "Assignment", false},
    {"+=", "AssignmentAdd", false},
    {"-=", "AssignmentSub", false},
    {"*=", "AssignmentMul", false},
    {"/=", "AssignmentDiv", false},
    {"%=", "AssignmentMod", false},
    {"&=", "AssignmentAND", false},
    {"|=", "AssignmentOR", false},
    {"^=", "AssignmentXOR", false},
    {">>=", "AssignmentShiftRight", false},
    {"<<=", "AssignmentShiftLeft", false},
    {"+", "Plus", false},
    {"-", "Minus", false},
    {"*", "Star", false},
    {"/", "Slash", false},
    {"<<", "ShiftLeft", false},
    {">>", "ShiftRight", false},
    {">", "GreaterThan", false},
    {">=", "GreaterThanEq", false},
    {"<", "LessThan", false},
    {"<=", "LessThanEq", false},
    {"==", "Equals", false},
    {"!=", "NotEquals", false},
    {"&", "Ampersand", false},
    {"%", "Percent", false},
    {"^", "BitwiseXOR", false},
    {"|", "BitwiseOR", false},
    {"~", "BitwiseNOT", false},
    {"&&", "LogicalAND", false},
    {"||", "LogicalOR", false},
    {"!", "LogicalNOT", false},
    {"++", "Increment", false},
    {"--", "Decrement", false},
    {",", "Comma", false},
    {".", "Dot", false},
    {"->", "Arrow", false},
    {":", "Colon", false},
    {"?", "QuestionMark", false},
};

typedef struct Node
{
    char* string;
    struct Node* nodes[94]; // '~' - ' '
    Keyword* keyword;

} Node;

void Insert(Node* root, char* string, Keyword* keyword)
{
    size_t index = 0;
    while (strcmp(root->string, string) != 0)
    {
        assert(string[index]);

        if (root->nodes[(size_t)(string[index] - '!')] == NULL) // ! is the first ascii char used in the keywords.
            root->nodes[(size_t)(string[index] - '!')] = calloc(1, sizeof(Node));

        Node* newRoot = root->nodes[(size_t)(string[index] - '!')];

        size_t newStringLen = strlen(root->string) + 2;
        newRoot->string = malloc(newStringLen);
        strcpy(newRoot->string, root->string);
        newRoot->string[newStringLen - 2] = string[index];
        newRoot->string[newStringLen - 1] = 0;

        root = newRoot;

        index++;
    }

    root->keyword = keyword;
}

void PrintParser(Node* root, size_t depth, bool printReturn)
{
    int numChildren = 0;
    // Only print next switch if the node has ANY children.
    for (size_t i = 0; i < sizeof(root->nodes) / sizeof(Node*); i++)
        if (root->nodes[i] != NULL)
        {
            numChildren++;
        }

    printf("\n");
    if (numChildren > 1)
        printf("switch (code[i + %zu])\n{\n", depth);

    if (numChildren != 0)
        for (size_t i = 0; i < sizeof(root->nodes) / sizeof(Node*); i++)
        {
            if (root->nodes[i] != NULL)
            {
                if (numChildren > 1)
                    printf("case \'%c\':", (int)i + '!');
                else
                    printf("if (code[i + %zu] == \'%c\') {", depth, (int)i + '!');

                PrintParser(root->nodes[i], depth + 1, numChildren > 1);

                if (numChildren == 1)
                {
                    printf("}\n");
                }
            }
        }

    if (numChildren > 1)
        printf("}\n");

    if (root->keyword != NULL)
    {
        if (root->keyword->strict)
            printf("if (IsNonIDChar(code[i + %i]))\n{\n", (int)depth);

        printf("*token = %s;\n", root->keyword->tokenType);
        printf("return i + %zu;\n", depth);

        if (root->keyword->strict)
            printf("}\n");
    }

    else if (printReturn)
        printf("return 0;\n");
}

int main()
{
    Node* root = calloc(1, sizeof(Node));
    root->string = "";

    for (size_t i = 0; i < sizeof(keywords) / sizeof(Keyword); i++)
    {
        Insert(root, keywords[i].name, &keywords[i]);
    }

    PrintParser(root, 0, true);
}