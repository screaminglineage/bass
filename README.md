# Bass

An extremely simple assembly-like interpreted language.  

## Building and Running
- Run `gcc src/*.c -O3 -o bass` in the root directory and use the executable generated as `./bass <filename>.bass`
- Try running some examples such as `./bass examples/fact.bass`

## Hello World
Hello World is as simple as 

```asm
println "hello world"
```

## Example Programs

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
    println r1
    add r3 r1 r2
    move r1 r2
    move r2 r3

    cmp r0 #10
    jumpz end    ; jump if zero (only jumps when last cmp result was 0)
    jump loop
end:
```

## Bassics

`bass` is designed to mimic the look and feel of assembly. Each program consists of a sequence of labels, which act as anchors to jump to, and opcodes along with their respective operands. The basic structure of a program is like: 
```asm
<LABEL>:
    OPCODE <OPERAND_1> <OPERAND_2> ...
    ...
```

Each operand to an opcode can be an immediate value (only integers are supported for now), a register, or a memory address. Immediate values are prefixed with a `#`, registers with an `r` and memory addresses with an `@`.

Character literals (delimited by `'`) and string literals (delimited by `"`) can only be used in `print` and `println` opcodes.

Bare identifiers can only be used in the jump instructions to signify the target label. Also note that labels are followed by a `:`. All whitespace/indentation is ignored and is optional.

```asm
loop:                    ; marks position as `loop`
    ; ...
    jump loop            ; jumps to the opcode next to the label `loop`
```

The special syntax `@r[0-7]` can be used as a shortcut for indirect addressing.

This instructs the operation to use the value in the specified register as a memory address.
For example, the following program stores, *100* into the memory address, *40* using this syntax, and then prints it
```asm
move r0 #40
move @r0 @100
println @40
```

### Registers
There are 8 registers, `r0` to `r7`, which can be used for direct operations. All registers are initialized to 0 at the program start. There are two special registers, the program counter and stack pointer which are inaccessible through `bass` for now. Another flag variable stores the result of the last comparison (can be 0, -1 or 1) and is also inaccessible through `bass`.

### Memory 
There is a total of 4MB of addressable memory available, which is also initialized to 0 at program start. All addresses are simply an index from the start of the memory. When storing integers into memory, make sure to properly align them to 4 bytes (or whatever `sizeof(int)` is) to prevent unexpected behaviour. For example, storing elements at `@0`, `@4`, and `@8` simultaneously should be fine, but trying to access or store elements at `@5` will instead create a view into the middle of integers in the memory.


## Opcodes

A common pattern with any opcode that stores some value is that, the first operand is the location where the result is stored.

For example,
```asm
move r0 #16                ; r0 := 16
add r1 r0 #1               ; r1 := r0 + 1
pop @90                    ; @90 := [STACK_TOP] 
```


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
add r0 r0 #1             ; ro := r0 + #1
mul @45 #90 r5           ; @45 := #90 + r5  
```

### Memory Operations
- `move`                 - move value into register or memory
- `store`                - move value into memory using indirect addressing
- `load`                 - load value from memory using indirect addressing

Examples
```asm
move @250 #12            ; @250 := 12

move r0 #12
store r0 #1000           ; @r0 := 1000 (store 1000 at memory address 12 (value of register r0))
```


### Printing
- `print`                - print registers, memory, numbers, characters, strings
- `println`              - same as `print` but puts a newline at the end

Examples
```asm
print r1
print #23
print 'a'
print '\n'
print "hello"
```

### Stack Operations
- `push`                 - push value into stack
- `pop`                  - pop value from stack into register or memory

Examples
```asm
push #12                 ; [STACK_TOP] := 12
push r0                  ; [STACK_TOP] := r0  
pop @45                  ; @45 = [STACK_TOP]
```


### Compare and Jumps
- `cmp`                  - sets the comparison flag (0 if equal, 1 if first is greater, -1 if first is less)
- `jump`                 - unconditional jump to label
- `jumpz`                - jump if comparsion flag is 0
- `jumpg`                - jump if comparison flag is 1
- `jumpl`                - jump if comparision flag is -1

Examples
```asm
start:
    cmp r0 #12
    jumpz end            ; jump to label `end` if the last comparison resulted in equals
    add r0 r0 #1
    jump start           ; always jump to label `start` at this point 
end:
```

For more examples, check out the [examples](./examples) directory.
