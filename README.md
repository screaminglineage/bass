# Bass (Basic ASSembly)

A simple assembly-like language

## Building and Running
- Run `gcc src/*.c -O3 -o bass` in the root directory and use the executable generated as `./bass <filename>.bass`
- Try running some examples such as `./bass examples/fact.bass`

## Sample program

The following example performs some basic arithmetic operations and prints the result.

```asm
move r0 #25
move r1 #0x10
add r2 r0 r1
mul r3 r2 #9
print r3
```

A more complicated program that prints the Fibonacci series.
```asm
; Prints first 10 numbers of the Fibonacci Series
move r0 #0
move r1 #0
move r2 #1

loop:
    add r0 r0 #1
    print r1
    add r3 r1 r2
    move r1 r2
    move r2 r3

    cmp r0 #10
    jumpz end    ; jump if zero (only jumps when last cmp result was 0)
    jump loop
end:
```

## Available Opcodes

nop                     - does nothing
add, sub, mul, div, mod - performs arithmetic operation and stores the result
move                    - move value into register or memory
load, store             - move value into memory using indirect addressing
print, println          - print registers, memory, numbers, characters, strings
push, pop               - access the stack
cmp                     - sets the comparison flag (0 if equal, 1 if first is greater, -1 if first is less)
jump                    - unconditional jump to label
jumpz, jumpg, jumpl     - jump is comparsion flag is 0, 1, -1 respectively
