# Bass (Basic ASSembly)

A simple assembly-like language

## Sample program

The following example performs some basic arithmetic operations and prints the result.

```asm
move r0 #25
move r1 #0x10
add r2 r0 r1
mul r3 r2 #9
print r3
```

A more complicated program that prints the Fibonacci series
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
