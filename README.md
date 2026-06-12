# prongC

prongC performs call-site-driven interprocedural data flow overlap detection. Given two function
invocations with symbolic arguments and the files where the specified functions were defined, 
it identifies all shared variable accesses between them without requiring a build system, compilation
database, or whole-program analysis.

## Usage

```bash
# Build
make
# Clean up
make clean

# Run
./build/prongc --files="file1.c;file2.c;..." --functions="foo(arg1, arg2);bar(arg1, arg2);..." [OPTIONS]
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

## Project Inspiration

While digging through Linux kernel driver code, I kept getting lost in messy, tangled functions 
that silently shared and modified the same data across files. This caused some pain when trying to
reorder their function calls. That pain inspired me to build this tool. 
The solution was a simple static analyzer using libclang that 
checks whether two or more functions touch the same data.
