# MoonCrypto Language Support for VS Code

Language support for MoonCrypto source files (`.monc`): syntax highlighting, comments, bracket matching, automatic indentation and snippets.

> The current source extension is `.monc`. The old `.mncyt` extension is no longer used.

## Build and install the latest extension

Open a terminal in the repository root and run:

```powershell
cd .\editors\vscode
npx --yes @vscode/vsce package
code --install-extension .\mooncrypto-language-1.5.5.vsix --force
```

After installation, run **Developer: Reload Window** from the VS Code command palette (`Ctrl+Shift+P`).

## Install from this folder

You can also copy this `vscode` folder to your VS Code extensions directory (`%USERPROFILE%\.vscode\extensions`) and reload VS Code. For normal distribution, packaging as a VSIX is recommended.

This extension provides editor support only. It does not include the MoonCrypto compiler or language diagnostics.
