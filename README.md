# Compiled Language

I haven't decided on a name, and probably will never assign it a name.
This is an academic project, and for **learning purposes**.

## Goals
- Make a language that's simple in syntax and features.
- Must be compiled (no virtual machine nor interpreter).
- Must be of an imperative, procedural style.
- When the language is done, make an example program with it. I am thinking of a raycaster that writes to an image, with the help of [Computer Graphics from Scratch](https://gabrielgambetta.com/computer-graphics-from-scratch/).

## Limitations that it will have
- Only works on **Windows** OS.
- It converts the AST into MASM, and then assembles it using the MSVC assembler from the commmand line.
- For simplicity it works with the stack (no register allocation strategy).
- It increases the stack size passing a flag to the assembler.
- It does not optimize the code at all.
- No module system. Programs can only be a single file.

## Features that I will add
- Primitive data types. 
	- Enums are included in this category and are compatible with integers.
	- The name of an enum can be obtained with an intrinsic function (strings in static data).
- Casting.
	- Implicit casts only work between primitive types.
	- Explicit casts only work between pointer types.
- Strings (Null terminated, String Views, and String Builders).
- Structs, Unions.
	- No OOP.
	- No tagged unions (Just C-like unions).
- Pointers, Arrays.
	- Arrays have bounds checking and a len() function.
	- Arrays decay to pointers.
	- There's no const.
	- No function pointers.
- Functions (No methods).
	- They can only be defined at file scope.
	- Order of their definitions doesn't matter to the compiler.
- Vector types and Matrix types with operators and built-in functions.
	- vec2, vec3, vec4 (all floating-point).
	- Only mat4 (floating-point).

## Usage
The compiler does not check the extension of the source code file (as the language doesn't have a name).
Text files are ideal:

```powershell
path\to\compiled-language.exe path\to\source_code.txt
```

## Examples
Check the [examples](examples) folder, where there are sample programs available to compile.