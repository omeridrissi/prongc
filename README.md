# prongC

prongC is a static analysis tool for C that detects shared variable accesses between 
function calls.

Given two or more function invocations with symbolic arguments, prongC traces how variables
are used through the program and identifies variables that are read or modified in multiple
source code locations.

## Dependencies

prongC only depends on llvm-21, which you can install here: https://apt.llvm.org/

## Usage

```bash
# Build
autoreconf -i
./configure
make

# Install
sudo make install

# Run
prongc --files="file1.c,file2.c,..." \
       --functions="foo(arg1, arg2);bar(arg1, arg2);..." \
        [OPT ARGS] [CLANG ARGS]

# Or trace individual variable
prongc --files="file.c" \
       --functions="foo(arg1)" \
       --trace="foo@arg1" \
       [OPT ARGS] [CLANG ARGS]
```
Command line function name format:

```c
foo(arg1, arg2, shared_arg);

bar(shared_arg, arg3);
```
Make sure that the shared arguments are of the same type, since prongc doesn't check this in runtime.

Make sure that each non-shared arguments in both function calls has a unique name, otherwise it's
going to be treated as a shared argument by prongc.

Recommended: Give the arguments the same names of the parameters in the function definitions.

Trace argument input format:

```bash
--trace="foo@param1;global1;..."
```
Where param1 is the parameter you want traced, and global1 is any global variable.

Make sure to include --functions argument.

## Use with compile database

```bash
prongc --files="..." --functions="..." --compdb-dir="[directory with compile_commands.json]"
```

## Linux kernel

For use on the Linux kernel, check out the [dedicated guide](docs/KERNEL_USAGE.md).

## Output example

```c
-------------------
Variable overlap:
foo(char *arg1, int arg2, bool shared_arg) → file1.c, line: 639, col: 6  │ READ : shared_arg
        if (shared_arg) {
            ^
bar(bool shared_arg, size_t arg3) → file1.c, line: 699, col: 25  │ READ : shared_arg
        var = shared_arg;
              ^
-------------------
```

## Use cases

- Detect hidden shared state between functions
- Verify whether function calls can be safely reordered
- Investigate side effects in legacy C code
- Analyze Linux kernel and driver code
- Explore data dependencies across files
- Identify overlapping reads and writes
- Understand unfamiliar codebases faster

## Why I made this

While digging through kernel staging driver code, I kept getting lost in messy, tangled functions 
that silently shared and modified the same data across files. This caused some pain when trying to
reorder their function calls. That pain inspired me to build this tool. 
The solution was a simple static analyzer using libclang that 
checks whether two or more functions touch the same data.
