# Instruções do projeto MonC

## Contexto

MonC é uma linguagem de programação com arquivos-fonte `.monc`, inspirada em C++ e com foco em segurança, criptografia, portabilidade e interoperabilidade com C/C++.

O projeto ainda está em fase inicial. Não trate o protótipo atual como uma implementação completa da linguagem.

## Objetivos técnicos

- Compilação nativa.
- Suporte planejado para x64 e ARM64.
- Ownership verificado pelo compilador.
- Portabilidade entre Windows, Linux e macOS como prioridade.
- Interoperabilidade principalmente por ABI C e adaptadores para bibliotecas C++.
- Biblioteca criptográfica oficial usando implementações consolidadas.
- Dependências obtidas por Git e fixadas em lockfile.
- Sintaxe própria, inspirada em C++, com melhorias de segurança e legibilidade. A declaração de função usa colchetes para os parâmetros, por exemplo `fn main[] -> Int32`.
- Laços usam colchetes para a cláusula de controle: `while [condição] {}` e `for [inicialização; condição; incremento] {}`.
- Criptografia usa o módulo explícito `use crypto`; `crypto.sha256[...].hex[]` já funciona no Windows via BCrypt. `crypto.key_from`, `crypto.aes_encrypt` e `crypto.aes_decrypt` continuam planejadas e devem usar bibliotecas consolidadas e criptografia autenticada.
- `const` já é reconhecido e não pode ser reatribuído; `struct` e `class` são reconhecidos como declarações iniciais. Ownership está especificado como objetivo, mas ainda aguarda verificação no compilador.

## Estado atual

O executável `moon` é um protótipo em C++20. O comando global é usado com o nome real do arquivo, sem os símbolos `<` e `>`:

```powershell
monc examples\hello\main.monc
```

O binário fica em `C:\Program Files\MonC\monc.exe`.

O console do Windows deve ser configurado para UTF-8 antes da saída de texto, para preservar acentos e outros caracteres Unicode.

Atualmente ele:

- Localiza `main.monc` na raiz do projeto.
- Caso não encontre `main.monc`, procura um único arquivo `.monc` cujo nome contenha `init` na raiz ou em `src/`.
- Reporta erro quando não há candidato ou quando há mais de um.
- Executa o comando `monc run` com suporte a funções (`fn`), parâmetros, retorno de valores, chamada de funções como instruções ou em expressões, importação de módulos com `#external`, `print[...]`, inteiros (`Int32`/`Int64`), strings (`String32`/`String64`/`String`), caracteres (`Char`), reatribuições, condicionais `if`/`else`, laços `for` e `while` com `break` e `continue`.
- Ainda não possui lexer, parser, verificador de tipos, ownership checker ou geração de código nativo.

### Interoperabilidade C já disponível

O runtime reconhece `math.h` e `stdlib.h` com assinaturas limitadas. Já estão disponíveis: `sqrt`, `pow`, `sin`, `abs`, `atoi`, `rand`, `srand`, `malloc`, `free` e `NULL`. `atof`, `atol` e `atoll` ainda não foram implementadas. Os detalhes e limites estão em `C_INTEROP.md`.

## Estrutura principal

```text
src/main.cpp                         CLI do protótipo
src/project.cpp                     Localização do ponto de entrada
src/project.hpp                     API de resolução do projeto
examples/hello/main.monc            Exemplo inicial
editors/vscode/                      Extensão de linguagem para VS Code
editors/vscode/package.json          Manifesto da extensão
editors/vscode/mooncrypto-icon.png   Ícone da extensão
CMakeLists.txt                       Configuração CMake
README.md                            Especificação técnica e estado do projeto
```

## Compilação do protótipo

Com CMake instalado:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

O CMake vincula BCrypt automaticamente no Windows.

No Windows com g++ disponível:

```powershell
g++ -std=c++20 -Wall -Wextra -Wpedantic -Isrc src/main.cpp src/project.cpp -o monc.exe
.\monc.exe entry examples\hello
.\monc.exe run examples\hello\main.monc
```

O comando `run` deve imprimir:

```text
Hello, MonC!
```

Também existe configuração inicial em `CMakeLists.txt`.

## Extensão do VS Code

A extensão fica em `editors/vscode/` e reconhece `.monc`.

Ela atualmente fornece:

- Realce de sintaxe.
- Comentários `//` e `/* */`.
- Fechamento automático de chaves, colchetes, parênteses e strings.
- Indentação básica.
- Snippets para `main`, `fn`, `struct` e `print`.
- Ícone da extensão em `mooncrypto-icon.png`.

Para empacotar no PowerShell, use `npx.cmd` caso `npx` seja bloqueado pela política de execução:

```powershell
cd editors\vscode
npx.cmd @vscode/vsce package --allow-missing-repository --out mooncrypto-language-0.1.1.vsix
code --install-extension .\mooncrypto-language-0.1.1.vsix --force
```

O campo `icon` do `package.json` controla o ícone da extensão na tela de extensões. Ele não define automaticamente um ícone para arquivos `.monc` no Explorer; isso exigiria um tema de ícones separado.

## Regras de desenvolvimento

- Sempre que uma funcionalidade da linguagem for adicionada ou alterada, atualizar na mesma mudança: compilador/interpretador, extensão do VS Code, exemplos relevantes, `README.md`, `MOONHOLDMAP.md` e este arquivo quando necessário.
- Execute `Test\\smoke.ps1` depois de alterar o interpretador; os fixtures usam a sintaxe atual com colchetes.
- Preserve as decisões registradas no `README.md` e atualize o documento quando uma decisão for tomada.
- Não implemente criptografia própria. Use bibliotecas consolidadas e APIs de alto nível por padrão.
- Não introduza ponteiros inseguros, conversões implícitas perigosas ou gerenciamento manual de memória sem justificar a decisão.
- Mantenha o tratamento de erros explicitamente em aberto até uma decisão do projeto.
- Prefira mudanças pequenas, compiláveis e fáceis de revisar.
- Use C++20 e mantenha os avisos do compilador habilitados.
- Preserve a compatibilidade com Windows, Linux e macOS sempre que possível.
- Não sobrescreva arquivos do usuário sem verificar o conteúdo existente.
- Ao adicionar uma funcionalidade, atualize o `README.md` e forneça um exemplo `.monc` quando fizer sentido.

## Próximas etapas sugeridas

1. Criar lexer para tokens de `.monc`.
2. Criar parser para funções, blocos, chamadas `print` e controle de fluxo.
3. Definir uma árvore sintática abstrata.
4. Criar diagnóstico de erros com linha e coluna.
5. Separar o interpretador experimental do futuro compilador nativo.
6. Projetar o modelo de ownership antes de implementar tipos complexos.
7. Adicionar integração do compilador com tarefas e diagnósticos do VS Code.
