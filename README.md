# prongc

## Usage


```bash
# Build
make
# Clean up
make clean

# Run
./build/prong --files="file1.c;file2.c;..." --functions="func1(arg1, arg2);func2(arg1, arg2);..." [OPTIONS]
```
Command line function name format:

```c
foo(arg1, arg2, shared_arg1);

bar(shared_arg1, arg3);
```
Make sure that the shared arguments are of the same type, since prongc doesn't check this in runtime.

Make sure that each non-shared arguments in both function calls has a unique name, otherwise it's
going to be treated as a shared argument by prongc.
## Project Inspiration

While digging through Linux kernel driver code, I kept getting lost in messy, tangled functions 
that silently shared and modified the same data across files. This caused some pain when trying to
reorder their function calls. That pain inspired me to build this tool. 
The solution was a simple static analyzer using libclang that 
checks whether two or more functions touch the same data.
