# P6 compiler

At the beginning of development, the repository contains source code for the Simple language in which all references to Simple have been replaced by P6.

The steps to develop a full P6 compiler imply the adaptation of:
* the scanner (`p6_scanner.l`)
* the parser (`p6_parser.y`)
* the symbol (`targets/symbol.h`)
* the type checker (`targets/type_checker.cpp`)
* the XML writer (for the middle delivery: `targets/xml_writer.cpp`)
* the Postfix writer (for the final delivery: `targets/postfix_writer.cpp`)

## Extension: interactive built-ins

Beyond the course specification, this compiler adds three integer built-ins so P6 programs can be interactive (see `tetris/`):

* `key(ms)` waits up to `ms` milliseconds (forever if negative) for a key press, without Enter, and returns its character code, a negative code for an arrow key (-1 up, -2 down, -3 right, -4 left), 0 if none came, or 4 when input has ended;
* `clock` is the number of milliseconds since the program first read it;
* `random(n)` is a random integer in `[0, n)`.

They compile to calls into `runtime/p6.c` (build it with `make -C runtime` and link `-lp6` before `-lrts`). A program that uses `key` gets its terminal restored wherever the main program ends.

## Tests

The course test suite is the `tests` submodule (the private `p6-tests` repo, which also holds the CDK21 and RTS7 sources). Fetch it with `git submodule update --init`, then, from `tests/` with the compiler linked as `./p6`, run `./check-parser.sh` and `./check-expected.sh`. The `tests` GitHub Action does all of this on every push, fetching the submodule with the `P6_TESTS_SSH_KEY` deploy key.
