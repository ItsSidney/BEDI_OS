# BEDI C (bedic) Quick Start Guide

BEDIC is a compact scripting language for BEDI OS. It supports variables, math, strings, input/output, conditionals, loops, and boolean logic.

## 1. Program Structure

BEDIC programs can use a `void main() { ... }` function or just a sequence of top-level statements. Semicolons are optional.

**Script style:**
```c
print("Hello BEDI OS!", endl)
exit(0)
```

**Function style:**
```c
void main() {
    print("Hello", endl)
    exit(0)
}
```

## 2. Variables & Data Types

BEDIC supports four types:

| Type | Description |
| :--- | :--- |
| `int` | 32-bit signed integer |
| `double` | Fixed-point decimal (6 decimal places internally) |
| `string` | Text string |
| `bool` | Boolean (`true` / `false`) |

### Integers & Math
```c
int a = 10
int b = -5
int sum = a + b
a++        // Increment
b--        // Decrement
print("Sum: ", sum, endl)
```

### Decimals (`double`)
BEDIC supports `double` using fixed-point arithmetic. Decimal literals are supported.
```c
double side_a = 8.0
double side_b = 5.0
double ratio = side_a / side_b
print("Ratio: ", ratio, endl) // Prints: 1.600000
```

### Strings
```c
string greeting = "Hello, Sidney!"
print(greeting, endl)

string name = "Bedi-C"
greeting = "Welcome to "
print(greeting, name, "!", endl)
```

### Booleans
```c
bool active = true
bool done = false
print("Active: ", active, endl)
```

## 3. Flow Control & Input

### If-Else
```c
if (a > b) {
    print("A is greater", endl)
} else {
    print("B is greater or equal", endl)
}
```

### While Loops
```c
int i = 0
while (i < 3) {
    print("i: ", i, endl)
    i++
}
```

### User Input (`input`)
The `input()` function reads from the terminal. It automatically parses based on the variable's type.
```c
print("Enter a whole number: ")
int code
input(code)
print("You entered: ", code, endl)

print("Enter a decimal: ")
double measurement
input(measurement)
print("You entered: ", measurement, endl)

print("Enter text: ")
string name
input(name)
print("Hello, ", name, endl)
```

## 4. Operators

| Category | Operators |
| :--- | :--- |
| **Arithmetic** | `+`, `-`, `*`, `/`, `++`, `--` |
| **Comparison** | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| **Logical** | `&&`, `||` |
| **Assignment** | `=` |

## 5. Built-in Functions

| Function | Description |
| :--- | :--- |
| `print(...)` | Print values to terminal. Accepts strings, variables, and `endl`. |
| `endl` | Print a newline. Use inside `print()`. |
| `input(x)` | Read user input into variable `x`. Type is inferred from variable. |
| `exit(n)` | Exit the program with status code `n`. |

## 6. Comments

Single-line comments are supported:
```c
// This is a comment
int x = 5  // Inline comment works too
```

## 7. String Escapes

Use `\n` for newlines inside string literals:
```c
print("Line 1\nLine 2", endl)
```

## 8. Workflow

1. **Write:** Use `bdim` editor or write a `.bc` file
2. **Compile:** `bcc <file.bc>` (outputs `<file>.bin`)
3. **Run:** `brun <file.bin>`

## 9. Example Programs

### Hello World
```c
print("Hello BEDI OS!", endl)
```

### Calculator
```c
void main() {
    print("Enter first number: ")
    int a
    input(a)
    
    print("Enter second number: ")
    int b
    input(b)
    
    print("Sum: ", a + b, endl)
    print("Product: ", a * b, endl)
    
    exit(0)
}
```

### Temperature Converter
```c
void main() {
    print("Enter Celsius: ")
    double c
    input(c)
    
    double f = c * 9 / 5 + 32
    print("Fahrenheit: ", f, endl)
    
    exit(0)
}
```

### Guessing Game
```c
void main() {
    int secret = 42
    int guess = 0
    int tries = 0
    
    while (guess != secret) {
        print("Guess: ")
        input(guess)
        tries++
        
        if (guess == secret) {
            print("Correct! Tries: ", tries, endl)
        } else if (guess < secret) {
            print("Higher!", endl)
        } else {
            print("Lower!", endl)
        }
    }
    
    exit(0)
}
```

## 10. Language Reference

| Feature | Syntax |
| :--- | :--- |
| **Types** | `int`, `double`, `string`, `bool` |
| **Arithmetic** | `+`, `-`, `*`, `/`, `++`, `--` |
| **Comparison** | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| **Logical** | `&&`, `||` |
| **Control flow** | `if-else`, `while` |
| **Input/Output** | `print(...)`, `input(x)`, `exit(n)`, `endl` |
| **Comments** | `//` |
