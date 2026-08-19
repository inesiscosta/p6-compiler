# P6 compiler

A compiler for **P6**, a small imperative language whose integers are balanced ternary, written for the Compilers course at Instituto Superior Técnico (2025/2026). It compiles P6 to 32-bit x86 assembly, can dump the syntax tree as XML, and, as a demonstration, compiles a full game of [Tetris written in P6](tetris/tetris.p6).

```p6
extern int<> argc;
extern string<int> argv;
extern int<string> atoi;

factorial(int n) -> int {
  if (n > 1) return n * factorial(n - 1);
  return 1;
}

begin
  auto n = 5;
  if (argc() == 2) n = atoi(argv(1));
  n, "! = ", factorial(n)!!
  return 0;
end
```

## The language

P6 reads like C with a few twists: types come in brackets, functions declare their return type with `->`, printing is a postfix `!`, and every number is ternary.

### Types

| Type | Meaning |
|---|---|
| `int` | 40-trit **balanced ternary** integer (digits -1, 0, +1), packed in 64 bits |
| `real` | 80-trit ternary real in the Takum format, packed in 128 bits |
| `string` | NUL-terminated array of 8-bit characters |
| `[T]` | pointer to `T`, so `[int]` is C's `int*` and `[[int]]` is `int**`; `[void]` is a generic pointer |
| `T<A, B>` | function taking `A` and `B` and returning `T`, so `int<int>` is a function from `int` to `int` |
| `void` | no return value (and the target of generic pointers) |

`auto` infers a variable's type from its initial value. Integers convert to reals implicitly, and `null` and `[void]` are compatible with any pointer type. No other conversions happen.

### Literals

- Integers: decimal `42`, base 9 with a leading zero (`086` is 78), or balanced ternary with `0t` and the digits `-`, `0`, `+` (`0t+0-0` is 24 and `0t-0+0` is -24).
- Reals: as in C, such as `3.14`, `1E3` or `12.34e-24`.
- Strings: `"..."` with `\n`, `\t`, `\r`, `\"`, `\\` and octal escapes such as `\012`. Adjacent strings are joined, so `"ab" "cd"` is `"abcd"`.
- Pointers: `null` is the only pointer literal.

There are no negative literals: `-5` is the `-` operator applied to `5`. Balanced-ternary literals are the exception, since a ternary number can be negative on its own.

Comments are `// to end of line` and `/* block */`. Block comments nest.

### Declarations and modules

```p6
int i = 3;                 // private to this file
public real r = 7.0;       // visible to other modules
public r2 = 7.0;           // same, type inferred
forward real r3;           // defined in another P6 module
forward int<int> factorial;
extern [void]<int> malloc; // a C function, with C types
```

A program can span several files. Exactly one of them has the main program, the statements between `begin` and `end`. Its `return` value is the process's exit code.

### Functions

```p6
max(int a, int b) -> int {
  if (a > b) return a;
  return b;
}

greet() -> void {
  "hello"!!
}
```

Arguments are passed by value using the C calling convention (cdecl), so P6 code can call C functions declared with `extern`. The program's arguments come from `argc()`, `argv(n)` and `envp(n)`, declared as `extern`.

### Statements

| Statement | Meaning |
|---|---|
| `e1, e2, ...!` | print the values with nothing between them |
| `e1, e2, ...!!` | print the values, then a newline |
| `if (c) s elif (c) s else s` | conditional |
| `while (c) s` | loop |
| `stop;` / `stop n;` | leave the innermost loop, or the n-th one out (C's `break`) |
| `next;` / `next n;` | restart the innermost loop, or the n-th one out (C's `continue`) |
| `return e;` | return from a function |
| `{ declarations statements }` | block with its own local variables |

`stop`, `next` and `return` must be the last statement of their block.

### Expressions

From highest to lowest precedence:

| Operators | Meaning |
|---|---|
| `( )`, `p[i]`, `[n]` | grouping, pointer indexing, allocate `n` objects on the stack |
| `+x`, `-x`, `x?` | identity, negation, address of a left-value (C's `&x`) |
| `~` | logical not |
| `*`, `/`, `%` | multiplication, division, remainder (`%` is integers only) |
| `+`, `-` | addition and subtraction, also pointer offsets and pointer differences |
| `<`, `>`, `<=`, `>=` | comparison |
| `==`, `!=` | equality |
| `&&` | logical and, short-circuit |
| `\|\|` | logical or, short-circuit |
| `=` | assignment (right-associative) |

Logic follows Kleene's three-valued logic. Zero is false and anything else is true.

Two other expressions are `input`, which reads an integer or a real depending on what is expected, and `sizeof(e)`, which gives a size in bytes.

```p6
[real] v = [5];   // five reals on the stack
v[0] = 1.5;
[real] p = v[2]?; // address of v[2]
int n = input;
```

## Extension: interactive built-ins

Beyond the course specification, this compiler adds three integer built-ins so P6 programs can be interactive:

- `key(ms)` waits up to `ms` milliseconds for a key press, without needing Enter. A negative `ms` waits forever. It returns:
  - the character code of the key;
  - a negative code for an arrow key (-1 up, -2 down, -3 right, -4 left);
  - 0 if no key was pressed in time;
  - 4 when input has ended.
- `clock` is the number of milliseconds since the program first read it.
- `random(n)` is a random integer in `[0, n)`.

They compile to calls into [`runtime/p6.c`](runtime/p6.c). Build it with `make -C runtime` and link `-lp6` before `-lrts`. A program that uses `key` gets its terminal restored whenever the main program ends.

## Tetris

[`tetris/tetris.p6`](tetris/tetris.p6) is a complete Tetris in about 550 lines of P6. It has a 7-bag randomizer, wall kicks, a ghost piece and incremental redraw with ANSI escape codes. Because every integer operation goes through a software ternary ALU, the game is written to avoid division altogether.

```sh
make && make -C tetris && ./tetris/tetris  # on Linux with the toolchain installed
./tetris/play.sh                           # anywhere with Docker
```

The controls are the arrows or WASD to move, Up or W to rotate, Space to drop, P to pause and Q to quit.

## Building

The compiler is built on the course's two libraries:

- **CDK21** (Compiler Development Kit): the base syntax tree classes, the type system, the Postfix code emitter and the compiler driver.
- **RTS7** (Run-Time System): the runtime that compiled programs link against. It provides startup code, input and output, memory, and the software ALUs for ternary integers and reals.

Both install to `~/compiladores/root`, which is where the Makefiles look. You also need `g++` with C++20, `flex`, `bison`, `yasm` and 32-bit `binutils`.

```sh
make                         # builds ./p6
```

On a machine without the toolchain, [`tetris/play.sh`](tetris/play.sh) builds everything inside a Docker container.

## License

[MIT](LICENSE) © 2026 Inês Costa and Diogo Santos.
