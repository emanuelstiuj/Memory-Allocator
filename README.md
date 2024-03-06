# Memory Allocator

Built a minimalistic memory allocator that can be used to manually manage virtual memory.
The goal is to have a reliable library that accounts for explicit allocation, reallocation, and initialization of memory.
An efficient implementation that keeps data aligned, keeps track of memory blocks and reuses freed blocks.
This can be further improved by reducing the number of syscalls and block operations.
