# MonC on Android (Termux)

This guide targets the stable `main` branch. It builds the existing C++20 interpreter directly inside Termux; it does not use an Android Studio project, Java, or an APK.

## Requirements

Install Termux from a maintained distribution (F-Droid or the official GitHub releases), open it, then update packages:

```sh
pkg update && pkg upgrade
pkg install git clang cmake make pkg-config openssl
```

The build uses Termux's native Clang toolchain and CMake. OpenSSL is used for the runtime's SHA-256, HMAC, and secure-random backend when detected.

## Get the Android branch

```sh
git clone --branch port/android-termux https://github.com/KIILX98624/MoonCryptoRepo.git
cd MoonCryptoRepo
```

## Build

Run the helper script:

```sh
bash scripts/build-termux.sh
```

Or configure and build manually:

```sh
cmake -S . -B build-termux \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PREFIX" \
  -DOPENSSL_ROOT_DIR="$PREFIX"
cmake --build build-termux --parallel "$(nproc)"
```

The executable should be at `build-termux/monc`.

## Run MonC programs

From the repository root:

```sh
./build-termux/monc --help
./build-termux/monc run "Code MonC/Examples/others/if_else.monc"
./build-termux/monc lex "Code MonC/Examples/others/if_else.monc"
./build-termux/monc ast "Code MonC/Examples/others/if_else.monc"
```

You can run a source file stored elsewhere by passing its full or relative path. Android storage is not automatically accessible: if you want to use files under shared storage, run `termux-setup-storage` once and grant the Android permission, then use paths under `~/storage/shared`.

## Tests

```sh
ctest --test-dir build-termux --output-on-failure
```

The current CTest suite includes runtime and C interoperability fixtures. A successful Linux/Termux build does not guarantee that every optional native-library declaration from desktop environments is available on Android.

## Android-specific notes and limitations

- Termux is a Linux user-space environment running on Android's Linux kernel. This port builds a Termux executable, not an APK.
- The runtime uses POSIX dynamic loading (`dlopen`/`dlsym`) for its limited C interoperability. Android's Bionic C library and available shared libraries differ from glibc; only functions and libraries actually present on the device can be used.
- The CMake project links `libdl` where the platform exposes it and detects OpenSSL through the Termux prefix.
- Windows-only BCrypt code remains guarded by `_WIN32`; Termux uses the non-Windows backend.
- Native C library interoperability is limited. Do not assume that all Linux headers, shared-library names, or desktop examples work on Android.
- This branch starts from `main`. It intentionally does not include the separate experimental development branch.

## Troubleshooting

**CMake cannot find OpenSSL**

Check that the package is installed, then configure with the Termux prefix explicitly:

```sh
pkg install openssl
cmake -S . -B build-termux -DCMAKE_PREFIX_PATH="$PREFIX" -DOPENSSL_ROOT_DIR="$PREFIX"
```

**The compiler or CMake is missing**

```sh
pkg install clang cmake make
```

**Permission denied when accessing shared files**

```sh
termux-setup-storage
```

Grant the requested Android permission and reopen Termux if necessary.

**A native C function cannot be found**

This may be an Android/Bionic availability difference, not a MonC parser issue. Check the exact library and symbol available on Android before using that declaration.
