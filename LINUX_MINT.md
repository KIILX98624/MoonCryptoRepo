# MonC on Linux Mint

This guide builds the current `main`-based MonC interpreter on Linux Mint. It uses the repository's existing CMake configuration and does not change the language runtime or remove any project files.

## Requirements

Open a terminal and install the build tools:

```bash
sudo apt update
sudo apt install -y git build-essential cmake libssl-dev
```

The packages provide Git, GCC/G++, Make, CMake, and OpenSSL development files. MonC requires a C++20-capable compiler and CMake 3.20 or newer.

Check the installed versions:

```bash
g++ --version
cmake --version
```

## Clone this branch

```bash
git clone --branch port/linux-mint https://github.com/KIILX98624/MoonCryptoRepo.git
cd MoonCryptoRepo
```

## Build

Run the helper script:

```bash
bash scripts/build-linux-mint.sh
```

Or configure and build manually:

```bash
cmake -S . -B build-linux-mint \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENSSL_ROOT_DIR=/usr

cmake --build build-linux-mint --parallel "$(nproc)"
```

The executable will be:

```text
build-linux-mint/monc
```

## Run MonC

From the repository root:

```bash
./build-linux-mint/monc --help
./build-linux-mint/monc run "Code MonC/Examples/others/if_else.monc"
./build-linux-mint/monc lex "Code MonC/Examples/others/if_else.monc"
./build-linux-mint/monc ast "Code MonC/Examples/others/if_else.monc"
```

## Run tests

```bash
ctest --test-dir build-linux-mint --output-on-failure
```

## Notes

- This is a Linux desktop build of the existing C++20 interpreter/runtime, not an Android APK and not a native-code compiler for `.monc` programs.
- CMake already selects the POSIX dynamic-loader path on non-Windows systems. The Linux Mint helper only configures and builds that existing implementation.
- Installing `libssl-dev` allows CMake to enable the OpenSSL crypto backend. If OpenSSL is not found, CMake warns that `crypto.sha256` is unavailable for that build.
- The helper script does not run `sudo` or install packages automatically. Install the requirements above first.
- Linux Mint releases can ship different compiler versions. If your compiler does not support C++20, install a newer GCC toolchain before building.
