- Add a Conditional Move operand (See examples/fib_rec.bass for what it might improve) [Eg: cmov from x86_64]
- Maybe add a Conditional Call as well (??) 
- Rewrite `rule110` example
- Compile into actual assembly
- Convert to 64 bits
    - Currently to read from memory, the register needs to read into the lower 32 bits.
    - This is quite annoying to check for each time in the compiler source code
    - If the entire thing was in 64 bits, it could always read into the entire 64 bit register, without issues

