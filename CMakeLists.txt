cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "-Wall -Wextra")
project(Compiler)

add_executable(comp
src/CodeGeneration/CG_Binop.c
src/CodeGeneration/CG_Expression.c
src/CodeGeneration/CG_NativeOP.c
src/CodeGeneration/CG_Statement.c
src/CodeGeneration/CG_UnOp.c

src/Parser/P_BinOp.c
src/Parser/P_Expression.c
src/Parser/P_Statement.c
src/Parser/P_Type.c
src/Parser/P_UnOp.c

src/Compiler.c
src/Data.c
src/Error.c
src/Flags.c
src/Function.c
src/Main.c
src/Optimizer.c
src/Outfile.c
src/Lexer.c
src/Register.c
src/Stack.c
src/Token.c
src/Util.c
src/Value.c
src/Variables.c
src/Preprocessor.c
src/Type.c
src/Scope.c
src/GenericList.c
)
target_link_libraries(comp m)
set_property(TARGET comp PROPERTY C_STANDARD 11)
