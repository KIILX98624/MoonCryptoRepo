# MoonCrypto — Holdmap da linguagem

Este documento resume os recursos disponíveis no protótipo atual do MoonCrypto (`.mncyt`). O projeto usa um interpretador experimental em C++20, com lexer e validação AST integrados; a geração de código nativo completo e o ownership checker ainda estão em evolução.

As decisões formais da linguagem estão em [MOONLANGUAGE.md](MOONLANGUAGE.md).

**Regra atual:** chamadas usam `[]`. Exemplos antigos com `print(...)`, `crypto.sha256(...)` ou `.hex()` devem ser lidos como históricos; a sintaxe válida é `print[...]`, `crypto.sha256[...]` e `.hex[]`.

## Mapa rápido

| Recurso | Sintaxe principal | Estado |
|---|---|---|
| Função | `fn nome[parâmetros] -> Tipo { }` | Implementado |
| Ponto de entrada | `fn main[] -> Int32 { }` | Implementado |
| Variável | `let nome: Tipo = valor;` | Implementado |
| Reatribuição | `nome = expressão;` | Implementado |
| Saída | `print[expressão];` | Implementado |
| Condicional | `if (condição) { } else { }` | Implementado |
| Laço `while` | `while [condição] { }` | Implementado |
| Laço `for` | `for [let i: Int32 = 0; i < 5; i = i + 1] { }` | Implementado |
| Seleção múltipla | `switch [valor] { case ... default ... }` | Implementado |
| Interrupção de laço | `break;` e `continue;` | Implementado |
| Retorno | `return expressão;` | Implementado |
| Módulo externo | `#external "nome"` | Implementado |
| Lexer | `mooncyt lex arquivo.mncyt` | Implementado inicialmente |
| Comentário de linha | `// comentário` | Editor / interpretador |
| Comentário de bloco | `/* comentário */` | Editor / interpretador |
| Constantes | `const nome: Tipo = valor;` | Implementado |
| Faixa de inteiros | `UInt8`, `Int32`, etc. | Implementado |
| Classes e structs | `class`, `struct` | Declaração reconhecida; tipos planejados |
| Ownership | referências e empréstimos | Especificado; verificação planejada |
| Criptografia oficial | `use crypto` e `crypto.*` | SHA-256 implementado; AES planejado |

## Funções

Funções usam colchetes para a lista de parâmetros. A função `main` não possui parâmetros e é escrita como `main[]`.

```mncyt
fn somar[a: Int32, b: Int32] -> Int32 {
    return a + b;
}

fn main[] -> Int32 {
    let resultado: Int32 = somar[10, 20];
    print[resultado];
    return 0;
}
```

Chamadas de função também usam colchetes: `somar[10, 20]`.

## Variáveis e tipos

```mncyt
let idade: Int32 = 18;
let nome: String32 = "MoonCrypto";
let inicial: Char = 'M';

idade = idade + 1;
```

Constantes não podem ser reatribuídas:

```mncyt
const LIMITE: Int32 = 100;
```

Estruturas e classes já são palavras reservadas e suas declarações são reconhecidas como base de sintaxe:

```mncyt
struct Usuario {
    nome: String32;
}

class Carteira {
    saldo: Int64;
}
```

O layout, instanciação, métodos e visibilidade desses tipos ainda serão definidos.

## Ownership

O objetivo do MoonCrypto é verificar ownership no compilador: cada valor terá um proprietário, referências serão empréstimos temporários e referências mutáveis terão regras exclusivas. A implementação atual ainda não faz essa verificação; código inseguro deverá ser explicitamente marcado quando o modelo for implementado.

Tipos atualmente usados pelo interpretador:

- Inteiros: `Int32`, `Int64` e tipos inteiros compatíveis.
- Texto: `String`, `String32`, `String64`.
- Caractere: `Char`.

## Saída com `print`

```mncyt
print["Olá, MoonCrypto!"];
print[nome];
print["Idade: " + idade];
```

O `print` aceita literais, variáveis e concatenação de texto, números e caracteres.

## Condicionais

```mncyt
if (idade >= 18) {
        print["maior de idade"];
} else {
        print["menor de idade"];
}
```

Comparadores disponíveis incluem `==`, `!=`, `<`, `>`, `<=` e `>=`.

Também é possível encadear condições:

```mncyt
if (valor == 1) {
    print["um"];
} else if (valor == 2) {
    print["dois"];
} else {
    print["outro"];
}
```

## Laços

Os laços usam colchetes para destacar a cláusula de controle.

```mncyt
while [idade < 21] {
    idade = idade + 1;
}

for [let i: Int32 = 0; i < 5; i = i + 1] {
    print["i = " + i];
}
```

Dentro dos laços:

```mncyt
if (i == 2) {
    continue;
}

if (i == 4) {
    break;
}
```

## `switch`, `case` e `default`

```mncyt
switch [dia] {
    case 1 {
        print["segunda-feira"];
    }
    case 2 {
        print["terça-feira"];
    }
    default {
        print["outro dia"];
    }
}
```

## Módulos externos

```mncyt
#external "matematica"

fn main[] -> Int32 {
    let valor: Int32 = somar[2, 3];
    print[valor];
    return 0;
}
```

O arquivo externo deve estar disponível no projeto conforme as regras documentadas no `README.md`.

## Criptografia

A API de criptografia será disponibilizada pelo módulo explícito `crypto`. Ela deverá usar uma biblioteca consolidada por baixo, com chaves opacas, aleatoriedade segura e formatos autenticados. A implementação ainda não está ligada ao interpretador.

### Hash SHA-256

```mncyt
use crypto

fn main[] -> Int32 {
    let texto: String32 = "MoonCrypto";
    let h = crypto.sha256[texto];
    print["SHA-256: " + h.hex[]];
    return 0;
}
```

Também será permitido o uso direto em uma expressão:

```mncyt
print[crypto.sha256[nome].hex[]];
```

### Criptografia autenticada

```mncyt
use crypto

fn main[] -> Int32 {
    let chave = crypto.key_from["senha-forte", 32];
    let cifrado = crypto.aes_encrypt["segredo", chave];
    print["Cifrado: " + cifrado.hex[]];

    let original = crypto.aes_decrypt[cifrado, chave];
    print["Decifrado: " + original];
    return 0;
}
```

`crypto.aes_encrypt` representa uma API de criptografia autenticada, preferencialmente AES-GCM ou outro modo AEAD aprovado. A API não deve expor AES sem autenticação como caminho padrão. Chaves, nonces e dados cifrados deverão ter tipos próprios e não devem ser tratados como strings comuns.

### Estado

- `crypto.sha256`: implementada no Windows usando BCrypt, com saída hexadecimal via `.hex[]`.
- `crypto.random_bytes[n]`: implementada usando o gerador seguro do sistema, com limite de 1 MiB por chamada.
- `crypto.hmac_sha256[dados, chave]`: implementada com BCrypt/OpenSSL e retorna `Hash256`.
- `crypto.key_from`: API planejada; deverá usar derivação de chave adequada, e não transformar senha diretamente em chave.
- `crypto.aes_encrypt` / `crypto.aes_decrypt`: API planejada para AEAD.
- `hex()`: serialização planejada para hashes e dados cifrados.
- Implementação própria de algoritmos: não permitida; usar biblioteca consolidada ou API criptográfica do sistema.

## Comentários

```mncyt
// comentário de uma linha

/* comentário
   com várias linhas */
```

## Execução atual

```powershell
mooncyt caminho\para\main.mncyt
```

Ou, a partir do projeto:

```powershell
moon run examples\while
```

## Próximos recursos

1. Compilação nativa para x64 e ARM64.
2. Ownership verificado pelo compilador.
3. Tipos opcionais e tratamento de erros formal.
4. `const`, `struct`, `class`, genéricos e interfaces.
5. Biblioteca criptográfica oficial com APIs de alto nível.
6. Diagnósticos completos no VS Code.

## Arrays nativos


> **Nota de modificação (2026-11-21):** o protótipo agora inclui a API inicial de arrays `Array<T>`. É possível criar com `Array[...]`, acessar por índice (`itens[0]`) e usar `array.push`, `array.pop`, `array.get`, `array.set`, `array.remove`, `array.clear` e `array.length`. A tipagem genérica é validada no interpretador, mas generics completos e ownership ainda não estão implementados.

Exemplo: `let numeros: Array<Int32> = Array[10, 20, 30];`

## Bytes

O protótipo já cria e indexa sequências de bytes:

```mncyt
let dados = Bytes[1, 2, 255];
print["primeiro=" + dados[0]];
```

## Código de saída

O valor inteiro retornado por `main[]` vira o código de saída do processo:

```mncyt
fn main[] -> Int32 {
    return 7;
}
```

## Lexer

O comando `mooncyt lex` lista tokens com linha e coluna. Essa camada já existe, mas o interpretador principal ainda está em migração gradual para consumir uma AST baseada nela.

## AST inicial

`mooncyt ast arquivo.mncyt` usa o lexer para reconhecer funções, parâmetros e tipos de retorno. A AST ainda não representa todas as expressões e instruções.

## Verificação de tipos

Anotações como `Int32`, `String32`, `Char`, `Bool` e `Bytes` são verificadas nas declarações e atribuições. Valores incompatíveis produzem diagnóstico com arquivo, linha e coluna.
