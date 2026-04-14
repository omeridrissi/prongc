# prongc

## Project Inspiration

While digging through Linux kernel driver code, I kept getting lost in messy, tangled functions 
that silently shared and modified the same data across files. 
That pain inspired me to build this tool. The solution was a simple static analyzer using libclang that 
checks whether two or more functions touch the same data.
