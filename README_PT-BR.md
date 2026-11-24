# MonC

**MonC é uma linguagem de programação experimental com um interpretador/runtime desenvolvido em C++20.** Sua sintaxe utiliza funções tipadas, chamadas com colchetes, módulos e um conjunto crescente de estruturas de dados nativas. O projeto está sendo desenvolvido com foco em sintaxe clara, interoperabilidade com bibliotecas nativas e conceitos de segurança de memória.

> **Estado do projeto:** atualmente, MonC é um protótipo interpretado em evolução. O repositório contém um runtime em C++20 que analisa e executa um subconjunto dos arquivos `.monc`. **Ainda não é um compilador para código nativo.** A compilação nativa, a verificação de ownership pelo compilador e vários recursos planejados continuam em desenvolvimento.

[English](README.md) · [Português (Brasil)](README_PT-BR.md) · [Español](README.es.md)

## Conteúdo

- [Como é a sintaxe da MonC](#como-é-a-sintaxe-da-monc)
- [Recursos atuais](#recursos-atuais)
- [Início rápido](#início-rápido)
- [Compilar o projeto](#compilar-o-projeto)
- [Executar um programa](#executar-um-programa)
- [Fundamentos da linguagem](#fundamentos-da-linguagem)
- [Módulos nativos](#módulos-nativos)
- [Interoperabilidade com C](#interoperabilidade-com-c)
- [Exemplos e testes](#exemplos-e-testes)
- [Estrutura do projeto](#estrutura-do-projeto)
- [Documentação](#documentação)
- [Limitações atuais](#limitações-atuais)

## Como é a sintaxe da MonC

Um programa simples:

```monc
// A função main é o ponto de entrada do programa.
fn main[] -> Int32 {
    // Declara uma variável inteira.
    let primeiro: Int32 = 20;
    let segundo: Int32 = 22;

    // Exibe o resultado usando a função nativa print.
    print["A resposta é: " + (primeiro + segundo)];

    // Retorna o código de saída do processo.
    return 0;
}
```

Uma regra importante: **as chamadas de funções MonC usam colchetes**:

```monc
somar[10, 20];
print["Olá, MonC!"];
crypto.sha256["mensagem"];
```

Funções C declaradas por meio de `playCodeC` usam a sintaxe de chamada da linguagem C, com parênteses.

## Recursos atuais

A tabela descreve o protótipo atual, não o projeto final da linguagem.

| Recurso | Estado atual |
|---|---|
| Funções tipadas e ponto de entrada `main` | Implementados no interpretador |
| Variáveis locais com `let` e constantes com `const` | Implementadas |
| Tipos inteiros e strings, caracteres e booleanos | Implementados em diferentes níveis; consulte a especificação |
| `if`, `else`, `while`, `for`, `switch`, `continue` | Implementados conforme a cobertura dos exemplos e testes |
| Funções do usuário e módulos de código com `#external` | Implementados |
| Módulo JSON nativo | Implementado |
| `Bytes`, hashes criptográficos, HMAC e bytes aleatórios | Implementados no runtime atual, com backends específicos por plataforma |
| `ByteArray` mutável e seus métodos documentados | Implementados no branch de desenvolvimento atual |
| Algumas funções C de `math.h` e `stdlib.h` | Interoperabilidade inicial implementada |
| Geração de executáveis nativos | Planejada, ainda não implementada |
| Ownership e borrowing verificados pelo compilador | Planejados, ainda não implementados |
| Compatibilidade completa com os built-ins do Python | Não é o objetivo do protótipo; apenas um pequeno subconjunto foi implementado |

Consulte a [especificação da linguagem](MDs/MOONLANGUAGE.md) e a documentação de [interoperabilidade com C](MDs/C_INTEROP.md) para detalhes e limitações.

## Início rápido

### Requisitos

- CMake 3.20 ou mais recente.
- Um compilador compatível com C++20.
- No Windows, uma toolchain MinGW-w64/MSYS2 compatível ou outra toolchain C++20.
- No Linux, um compilador C++20 e as bibliotecas de desenvolvimento exigidas pelo projeto.
- Os arquivos de desenvolvimento do OpenSSL são opcionais em sistemas que não sejam Windows. Quando encontrados, o CMake habilita o backend criptográfico OpenSSL.

Verifique se as ferramentas estão disponíveis:

```sh
cmake --version
g++ --version
```

### Compilar o projeto

Execute na raiz do repositório:

```sh
cmake -S . -B build
cmake --build build --config Release
```

O executável normalmente fica em:

- Windows: `build/Release/monc.exe` ou `build/monc.exe`, dependendo do gerador.
- Linux: `build/monc`.

### Executar um programa

Passe um arquivo-fonte ao interpretador:

```sh
./build/monc run "Code MonC/Examples/others/if_else.monc"
```

No Windows, por exemplo:

```powershell
.\build\Release\monc.exe run "Code MonC\Examples\others\if_else.monc"
```

O caminho exato do executável depende do gerador do CMake. Também é possível adicionar a pasta do executável ao `PATH` para chamar `monc` de outros diretórios.

### Executar os testes automatizados

```sh
ctest --test-dir build -C Release --output-on-failure
```

No Windows, o repositório também contém um script PowerShell de smoke test que compila o runtime com MinGW e executa os fixtures:

```powershell
.\Code\MonC\SmokeTest\smoke.ps1
```

Execute-o na raiz do repositório, em um PowerShell com o compilador necessário disponível.

## Fundamentos da linguagem

### Funções e variáveis

Parâmetros de funções são declarados entre colchetes, e o tipo de retorno vem depois de `->`:

```monc
// Recebe dois inteiros e retorna a soma deles.
fn somar[a: Int32, b: Int32] -> Int32 {
    return a + b;
}

fn main[] -> Int32 {
    // Declara uma variável com tipo explícito.
    let resultado: Int32 = somar[12, 30];

    // Exibe o valor calculado.
    print[resultado];
    return 0;
}
```

Em alguns casos, o tipo pode ser inferido:

```monc
let mensagem = "Olá";
let valores = Array[1, 2, 3];
```

### Condições e laços

```monc
fn main[] -> Int32 {
    let contador: Int32 = 0;

    // Repete enquanto contador for menor que três.
    while [contador < 3] {
        print[contador];
        contador = contador + 1;
    }

    // Escolhe um bloco de acordo com a condição.
    if (contador == 3) {
        print["Concluído"];
    } else {
        print["Não concluído"];
    }

    return 0;
}
```

### Arrays

O runtime atual inclui o tipo `Array` e operações para trabalhar com arrays:

```monc
fn main[] -> Int32 {
    // Cria um array com três números inteiros.
    let numeros: Array<Int32> = Array[10, 20, 30];

    // Adiciona um valor ao final do array.
    array.push[numeros, 40];

    // Lê o item da posição zero.
    print[array.get[numeros, 0]];

    // Exibe a quantidade de itens do array.
    print[array.length[numeros]];

    return 0;
}
```

### ByteArray

`ByteArray` é uma sequência mutável de bytes, em que cada valor vai de 0 a 255. Ela é diferente do tipo `Bytes`, utilizado para dados binários e operações criptográficas.

```monc
fn main[] -> Int32 {
    // Cria um ByteArray a partir de um Array.
    let dados = bytearray[Array[10, 20, 30]];

    // Altera o byte que está no índice zero.
    dados[0] = 15;

    // Adiciona dois bytes ao final.
    dados.append[40, 50];

    // Consulta a quantidade atual de bytes.
    print[dados.length];

    // Exibe todos os valores armazenados.
    print[dados[]];

    return 0;
}
```

O branch atual também implementa métodos como `pop`, `vpop`, `insert`, `clear`, `copy`, `reverse`, `contains`, `index`, `count`, `extend`, `slice` e `fill`. As regras exatas dos argumentos estão em [MOONLANGUAGE.md](MDs/MOONLANGUAGE.md).

## Módulos nativos

### JSON

O módulo nativo `json` permite analisar, criar, ler, gravar e serializar valores JSON:

```monc
#external "json"

fn main[] -> Int32 {
    // Converte um texto JSON em um JsonValue.
    let documento: JsonValue = json.parse["{\"nome\":\"MonC\",\"versao\":2}"];

    // Obtém os campos e converte para os tipos MonC esperados.
    let nome: String32 = json.get[documento, "nome"].as_string[];
    let versao: Int32 = json.get[documento, "versao"].as_int[];

    // Exibe os valores extraídos.
    print["Nome: " + nome];
    print["Versão: " + versao];

    return 0;
}
```

Veja outros exemplos em `Code MonC/Examples/json_external`.

### Criptografia

O runtime atual oferece SHA-256, HMAC-SHA-256 e geração de bytes aleatórios seguros. Exemplo de hash:

```monc
use crypto

fn main[] -> Int32 {
    // Armazena o texto que será processado.
    let mensagem: String32 = "MonC";

    // Calcula o digest SHA-256.
    let digest = crypto.sha256[mensagem];

    // Converte o digest para hexadecimal e exibe o resultado.
    print["SHA-256: " + digest.hex[]];

    return 0;
}
```

Não considere APIs de cifragem planejadas como disponíveis apenas porque aparecem em exemplos antigos de design. Confira o código e a especificação atuais antes de depender de uma operação criptográfica.

## Interoperabilidade com C

MonC pode declarar e chamar um conjunto limitado de funções C nativas por meio de `#external` e `playCodeC`:

```monc
#external "stdlib.h" {
    playCodeC {
        int atoi(const char* texto);
        int rand(void);
        void srand(unsigned int seed);
    }
}

fn main[] -> Int32 {
    // Converte um texto decimal em inteiro usando a biblioteca C.
    let numero: Int32 = atoi("123");

    // Define a semente antes de solicitar um valor pseudoaleatório.
    srand(123);
    let valor: Int32 = rand();

    print[numero];
    print[valor];
    return 0;
}
```

Essa integração é limitada. Ela não significa que toda a biblioteca padrão C ou qualquer header arbitrário seja suportado. Consulte [MDs/C_INTEROP.md](MDs/C_INTEROP.md) para ver assinaturas e restrições.

## Exemplos e testes

Os programas de exemplo e demonstração estão em:

`Code MonC/Examples/`

| Pasta | Conteúdo |
|---|---|
| `array` | Criação e operações com arrays |
| `bytearray` | Arrays mutáveis de bytes e built-ins |
| `crypto` | Hashes e sintaxe de criptografia |
| `externalLib` | Interoperabilidade com bibliotecas C |
| `functions` | Funções do usuário e funções importadas |
| `json_external` | Análise, serialização e dados JSON externos |
| `math` | Funções matemáticas |
| `modules` | Divisão do código em arquivos MonC |
| `others` | Strings, caracteres, constantes, condições, laços e demonstrações diversas |

Alguns arquivos são experimentos ou demonstrações de design e podem utilizar sintaxe ainda não suportada pelo runtime atual. Para verificar comportamentos cobertos pela suíte automatizada, prefira os fixtures em `Code MonC/SmokeTest/fixtures`.

## Estrutura do projeto

```text
.
├── src/                         # Interpretador e runtime em C++20
├── Code MonC/
│   ├── Examples/                # Programas de exemplo em MonC
│   └── SmokeTest/
│       ├── fixtures/            # Programas usados nos testes
│       └── smoke.ps1            # Executor de smoke tests para Windows
├── MDs/                         # Documentação detalhada da linguagem
├── editors/vscode/              # Suporte da linguagem no VS Code
└── CMakeLists.txt               # Configuração do build com CMake
```

## Documentação

- [Guia para iniciantes](MDs/GUIA_INICIANTE.md)
- [Guia técnico avançado](MDs/GUIA_AVANCADO.md)
- [Especificação da linguagem](MDs/MOONLANGUAGE.md)
- [Interoperabilidade com C](MDs/C_INTEROP.md)
- [Roteiro do projeto](MDs/ROADMAP_MOONCRYPTO.md)
- [Extensão para VS Code](editors/vscode/README.md)

## Limitações atuais

MonC ainda é um protótipo. Entre as limitações importantes:

- O executável atual interpreta código MonC; ainda não gera executáveis nativos a partir de arquivos `.monc`.
- A especificação completa e o comportamento do runtime continuam evoluindo.
- A interoperabilidade com C aceita apenas um conjunto limitado de bibliotecas e assinaturas.
- Operações gerais com ponteiros e ownership verificado pelo compilador ainda não estão implementados.
- Alguns exemplos são experimentos antigos e podem não funcionar com o parser atual.
- Os built-ins inspirados no Python formam um subconjunto pequeno, não uma compatibilidade completa com Python.

Contribuições, relatos de bugs e exemplos objetivos são bem-vindos. Ao relatar um problema, inclua o arquivo-fonte, o comando utilizado, a saída esperada e a saída obtida.
