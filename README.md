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
- For simplicity it uses mostly the stack, not registers.
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

## Possible Syntax
```rust
struct Rect2D {
	top_left: vec2,
	size: vec2,
}

struct Player {
	collider: Rect2D,
	hp: int,
	texture_id: int,
}

fn damage(self: *Player, value: int) -> bool {
	self.hp -= value;
	self.hp = max(self.hp, 0);
	return self.hp > 0;
}

fn main() {
	let player = Player{
		.hp = random_int(1, 20),
	};
	let alive_after_damage = damage(&player, 5);

	if alive_after_damage {
		println("Player is alive");
		exit(0);
	} else {
		println("Player was killed");
		exit(1);
	}
}
```