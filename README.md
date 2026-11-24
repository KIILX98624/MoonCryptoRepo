# MonC

**MonC is an experimental programming language with a C++20-based interpreter/runtime.** Its syntax uses typed functions, square-bracket calls, modules, and a growing set of built-in data structures. The project is being developed with a focus on clear syntax, native-library interoperability, and memory-safety ideas.

> **Project status:** MonC is currently an evolving interpreted prototype. The repository contains a C++20 runtime that parses and executes a supported subset of `.monc` files. It is **not yet a native-code compiler**. Native compilation, compiler-enforced ownership, and several planned language features are still under development.

[English](README.md) · [Português (Brasil)](README_PT-BR.md) · [Español](README.es.md)

## Contents

- [What MonC looks like](#what-monc-looks-like)
- [Current capabilities](#current-capabilities)
- [Quick start](#quick-start)
- [Build from source](#build-from-source)
- [Run a program](#run-a-program)
- [Language basics](#language-basics)
- [Built-in modules](#built-in-modules)
- [C interoperability](#c-interoperability)
- [Examples and tests](#examples-and-tests)
- [Project structure](#project-structure)
- [Documentation](#documentation)
- [Current limitations](#current-limitations)

## What MonC looks like

A small MonC program:

```monc
// The main function is the entry point of this program.
fn main[] -> Int32 {
    // Declare an integer variable.
    let first: Int32 = 20;
    let second: Int32 = 22;

    // Call the built-in print function and display the result.
    print["The answer is: " + (first + second)];

    // Return the process exit code.
    return 0;
}
```

A notable syntax rule is that **MonC function calls use square brackets**:

```monc
sum[10, 20];
print["Hello, MonC!"];
crypto.sha256["message"];
```

C functions declared through `playCodeC` use the C calling syntax with parentheses. See [C interoperability](#c-interoperability).

## Current capabilities

The following table describes the prototype, not the long-term language design.

| Feature | Current status |
|---|---|
| Typed functions and `main` entry point | Implemented in the interpreter |
| Local variables with `let` and constants with `const` | Implemented |
| Integer and string types, characters, booleans | Implemented to varying extents; see the language specification |
| `if`, `else`, `while`, `for`, `switch`, `continue` | Implemented to the extent covered by the examples and tests |
| User-defined functions and `#external` source modules | Implemented |
| Native JSON module | Implemented |
| `Bytes`, cryptographic hashes, HMAC, random bytes | Implemented in the current runtime, with platform-specific backends |
| Mutable `ByteArray` and its documented methods | Implemented in the current development branch |
| Selected C functions from `math.h` and `stdlib.h` | Initial interoperability implemented |
| Native executable generation | Planned, not implemented |
| Compiler-checked ownership and borrowing | Planned, not implemented |
| Complete Python built-in compatibility | Not a goal of the current prototype; only a small selected subset is implemented |

Feature details and exact limitations can change as the runtime evolves. The [language specification](MDs/MOONLANGUAGE.md) and [C interoperability notes](MDs/C_INTEROP.md) are the more detailed references.

## Quick start

### Requirements

- CMake 3.20 or newer.
- A C++20 compiler.
- On Windows, a supported MinGW-w64/MSYS2 toolchain or another compatible C++20 toolchain.
- On Linux, a C++20 compiler and the system development libraries required by the project.
- OpenSSL development files are optional on non-Windows systems. When available, CMake enables the OpenSSL crypto backend.

Check that the tools are available:

```sh
cmake --version
g++ --version
```

### Build from source

Run these commands from the repository root:

```sh
cmake -S . -B build
cmake --build build --config Release
```

The executable is normally located at:

- Windows: `build/Release/monc.exe` or `build/monc.exe`, depending on the generator.
- Linux: `build/monc`.

### Run a program

Pass a source file to the interpreter:

```sh
./build/monc run "Code MonC/Examples/others/if_else.monc"
```

On Windows, for example:

```powershell
.\build\Release\monc.exe run "Code MonC\Examples\others\if_else.monc"
```

The exact executable path depends on the CMake generator. You can also add the executable's directory to your `PATH` to call `monc` from another folder.

### Run the automated tests

```sh
ctest --test-dir build -C Release --output-on-failure
```

On Windows, the repository also contains a PowerShell smoke-test script that compiles the runtime with MinGW and exercises the example fixtures:

```powershell
.\Code\MonC\SmokeTest\smoke.ps1
```

Run the script from the repository root in a PowerShell environment with the required compiler available.

## Language basics

### Functions and variables

Functions declare parameters inside square brackets and specify a return type after `->`:

```monc
// Add two integers and return their sum.
fn add[a: Int32, b: Int32] -> Int32 {
    return a + b;
}

fn main[] -> Int32 {
    // Create a variable whose type is explicitly declared.
    let result: Int32 = add[12, 30];

    // Display the calculated value.
    print[result];
    return 0;
}
```

A variable's type can sometimes be inferred:

```monc
let message = "Hello";
let values = Array[1, 2, 3];
```

### Conditions and loops

```monc
fn main[] -> Int32 {
    let count: Int32 = 0;

    // Repeat while count is less than three.
    while [count < 3] {
        print[count];
        count = count + 1;
    }

    // Select a branch based on a condition.
    if (count == 3) {
        print["Finished"];
    } else {
        print["Not finished"];
    }

    return 0;
}
```

### Arrays

The current runtime includes an `Array` type and array operations. The following example uses the array module API:

```monc
fn main[] -> Int32 {
    // Create an array containing three integers.
    let numbers: Array<Int32> = Array[10, 20, 30];

    // Append a value to the end of the array.
    array.push[numbers, 40];

    // Read an item by its zero-based index.
    print[array.get[numbers, 0]];

    // Print the number of items in the array.
    print[array.length[numbers]];

    return 0;
}
```

### ByteArray

`ByteArray` is a mutable sequence of byte values (0 through 255). It is distinct from the immutable or value-oriented `Bytes` type used for binary and cryptographic data.

```monc
fn main[] -> Int32 {
    // Create a mutable byte array from an Array.
    let data = bytearray[Array[10, 20, 30]];

    // Change the byte at index zero.
    data[0] = 15;

    // Append two bytes to the end.
    data.append[40, 50];

    // Read the current number of bytes.
    print[data.length];

    // Print all byte values.
    print[data[]];

    return 0;
}
```

The current branch also implements methods such as `pop`, `vpop`, `insert`, `clear`, `copy`, `reverse`, `contains`, `index`, `count`, `extend`, `slice`, and `fill`. Their exact argument rules are documented in [MOONLANGUAGE.md](MDs/MOONLANGUAGE.md).

## Built-in modules

### JSON

The built-in JSON module parses, creates, reads, writes, and serializes JSON values:

```monc
#external "json"

fn main[] -> Int32 {
    // Parse JSON text into a JsonValue.
    let document: JsonValue = json.parse["{\"name\":\"MonC\",\"version\":2}"];

    // Retrieve fields and convert them to their expected MonC types.
    let name: String32 = json.get[document, "name"].as_string[];
    let version: Int32 = json.get[document, "version"].as_int[];

    // Display the extracted values.
    print["Name: " + name];
    print["Version: " + version];

    return 0;
}
```

See the JSON examples under `Code MonC/Examples/json_external` for more operations.

### Cryptography

The runtime currently provides SHA-256, HMAC-SHA-256, and secure random-byte generation through its crypto functionality. For example:

```monc
use crypto

fn main[] -> Int32 {
    // Store the text that will be hashed.
    let message: String32 = "MonC";

    // Calculate the SHA-256 digest.
    let digest = crypto.sha256[message];

    // Convert the digest to hexadecimal text and display it.
    print["SHA-256: " + digest.hex[]];

    return 0;
}
```

Do not treat planned encryption APIs as available merely because they appear in older design examples. Check the current source and specification before relying on a cryptographic operation.

## C interoperability

MonC can declare and call a selected set of native C functions using `#external` and `playCodeC`:

```monc
#external "stdlib.h" {
    playCodeC {
        int atoi(const char* text);
        int rand(void);
        void srand(unsigned int seed);
    }
}

fn main[] -> Int32 {
    // Convert decimal text into an integer using the C standard library.
    let number: Int32 = atoi("123");

    // Set the seed before requesting a pseudo-random value.
    srand(123);
    let value: Int32 = rand();

    print[number];
    print[value];
    return 0;
}
```

The currently documented integration is limited. It does not mean that the entire C standard library or arbitrary C headers are supported. See [MDs/C_INTEROP.md](MDs/C_INTEROP.md) for supported signatures and restrictions.

## Examples and tests

Runnable and illustrative programs are stored in:

`Code MonC/Examples/`

The examples are grouped by topic:

| Folder | Topics |
|---|---|
| `array` | Array creation and operations |
| `bytearray` | Mutable byte arrays and built-ins |
| `crypto` | Hashing and crypto syntax |
| `externalLib` | C library interoperability |
| `functions` | User-defined and imported functions |
| `json_external` | JSON parsing, serialization, and external data |
| `math` | Mathematical functions |
| `modules` | Splitting code across MonC files |
| `others` | Strings, characters, constants, conditions, loops, and other demonstrations |

Some files are experiments or design demonstrations and may use syntax that is not supported by the current runtime. Prefer the automated fixtures in `Code MonC/SmokeTest/fixtures` when checking behavior guaranteed by the test suite.

## Project structure

```text
.
├── src/                         # C++20 interpreter and runtime
├── Code MonC/
│   ├── Examples/                # MonC example programs
│   └── SmokeTest/
│       ├── fixtures/            # Programs used by smoke tests
│       └── smoke.ps1            # Windows/PowerShell smoke-test runner
├── MDs/                         # Detailed language and design documentation
├── editors/vscode/              # VS Code language support
└── CMakeLists.txt               # CMake build configuration
```

## Documentation

- [Language specification](MDs/MOONLANGUAGE.md)
- [C interoperability](MDs/C_INTEROP.md)
- [Project roadmap](MDs/ROADMAP_MOONCRYPTO.md)
- [VS Code extension](editors/vscode/README.md)

## Current limitations

MonC is still a prototype. Important limitations include:

- The current executable interprets MonC source; it does not yet generate native executables from `.monc` files.
- The full language specification and runtime behavior are still evolving.
- C interoperability supports a limited set of signatures and libraries.
- General-purpose pointer operations and compiler-enforced memory ownership are not implemented.
- Some examples are historical experiments and may not run with the current parser.
- The implemented Python-inspired built-ins are a small subset, not full Python compatibility.

Contributions, bug reports, and focused examples are welcome. Please include the source file, the command used, the expected output, and the actual output when reporting a problem.
