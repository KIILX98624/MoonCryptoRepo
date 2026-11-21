# MoonCrypto Language Support for VS Code

Language support for MoonCrypto source files (`.mncyt`): syntax highlighting for control flow including `if`, `else`, `while` and `for`, declarations such as `const`, `struct` and `class`, `//` and `/* */` comments, bracket matching, automatic indentation, snippets for `main[]`, functions, `const`, `struct`, `class`, `Bool`, `Bytes`, `ifelse`, `while`, `for`, and `print`, plus the MoonCrypto logo in the extension details.

## Install with the packaged extension (recommended)

From the project root, run:

```powershell
  code --install-extension .\editors\vscode\mooncrypto-language-0.1.2.vsix --force
```

Then run `Developer: Reload Window` from the VS Code command palette (`Ctrl+Shift+P`).

## Install from this folder

Copy this `vscode` folder to your VS Code extensions directory and rename it to `mooncrypto-language-0.1.2`, then reload VS Code. On Windows, the extensions directory is `%USERPROFILE%\.vscode\extensions`.

Alternatively, once packaged as a VSIX, install it with:

```powershell
code --install-extension .\mooncrypto-language-0.1.0.vsix
```

This extension provides editor support only. It does not include the MoonCrypto compiler or language diagnostics.
