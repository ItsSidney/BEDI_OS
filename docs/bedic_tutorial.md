# Bedi-C Quick Start Guide

Bedi-C is a compact C-subset for BEDI OS.

## 1. The Basics
Programs can use a `void main() { ... }` function or just a sequence of statements. Semicolons are optional!

**Script Style:**
```c
print("Hello BEDI OS!", endl)
exit(0)
```

**Function Style:**
```c
void main() {
    print("Hello", endl)
    exit(0)
}
```

## 2. Variables & Data Types
Bedi-C supports `int` (32-bit signed), `double` (fixed-point decimal), and `string`.

### Integers & Math
```c
int a = 10
int b = -5 // Supports negative numbers
int sum = a + b  
a++ // Increment
b-- // Decrement
print("Sum: ", sum, endl)
```

### Decimals (`double`)
Bedi-C supports `double` using fixed-point arithmetic (internal precision is 6 decimal places). You can use decimal literal notation.
```c
double side_a = 8.0
double side_b = 5.0
double ratio = side_a / side_b
print("Ratio: ", ratio, endl) // Prints: 1.600000
```

### Strings
You can store text in variables and reassign them.
```c
string greeting = "Hello, Sidney!"
print(greeting, endl)

string name = "Bedi-C"
greeting = "Welcome to "
print(greeting, name, "!", endl)
```

## 3. Flow Control & Input
Bedi-C supports `if-else`, `while`, `for` loops, and interactive input.

### If-Else
```c
if (a > b) {
    print("A is greater", endl)
} else {
    print("B is greater or equal", endl)
}
```

### User Input (`input`)
You can read values from the user. The `input()` function automatically handles parsing based on the variable's type.
```c
print("Enter a whole number: ")
int code
input(code)
print("You entered: ", code, endl)

print("Enter a decimal: ")
double measurement
input(measurement)
print("You entered: ", measurement, endl)
```

### Loops
```c
// WHILE Loop
int i = 0
while (i < 3) {
    print("i: ", i, endl)
    i++
}
```

## 4. Language Reference

| Feature | Syntax |
| :--- | :--- |
| **Types** | `int`, `double`, `string`, `bool` |
| **Arithmetic** | `+`, `-`, `*`, `/`, `++`, `--` |
| **Comparison** | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| **Logical** | `&&`, `||` |
| **Built-ins** | `print(...)`, `input(...)`, `exit(n)`, `endl` |

## 5. Workflow
1. **Write:** `bdim file.bc`
2. **Compile:** `bcc file.bc` (outputs `file.bin`)
3. **Run:** `brun file.bin`
