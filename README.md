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

`nop`                     - does nothing

### Arithmetic Operations

All of the following perform some arithmetic operation and store the result

- `add`
- `sub` 
- `mul` 
- `div` 
- `mod`

Examples 
```asm
add r0 r0 #1                ; ro := r0 + #1
mul @45 #90 r5              ; @45 := #90 + r5  
```

### Memory Operations
- `move`                 - move value into register or memory
- `store`                - move value into memory using indirect addressing
- `load`                 - load value from memory using indirect addressing

### Printing
- `print`                - print registers, memory, numbers, characters, strings
- `println`              - same as `print` but puts a newline at the end

### Stack Operations
- `push`                 - push value into stack
- `pop`                  - pop value from stack into register or memory 

### Compare and Jumps
- `cmp`                  - sets the comparison flag (0 if equal, 1 if first is greater, -1 if first is less)
- `jump`                 - unconditional jump to label
- `jumpz`                - jump if comparsion flag is 0
- `jumpg`                - jump if comparison flag is 1
- `jumpl`                - jump if comparision flag is -1    
