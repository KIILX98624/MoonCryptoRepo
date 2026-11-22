# Interoperabilidade C da MonC

## Objetivo

A interoperabilidade C permitirá declarar funções de cabeçalhos C e chamá-las pela MonC, mantendo o código C declarado dentro de um bloco explícito `playCodeC`.

## Sintaxe planejada

```monc
#external "math.h" {
    playCodeC {
        double sqrt(double value);
        double pow(double base, double exponent);
        double sin(double value);
    }
}

let resultado: Float64 = sqrt(25.0);
print[resultado];
```

O parser AST e o runtime reconhecem o cabeçalho, o bloco `playCodeC`, os nomes das funções, tipos de retorno e parâmetros. A implementação mapeia `math.h` para a biblioteca matemática nativa e `stdlib.h` para a biblioteca C padrão do sistema. Chamadas `float`/`double` e, agora, `int` com até dois argumentos homogêneos são resolvidas dinamicamente.

## Tipos numéricos

| MonC | Representação | Tipo C correspondente |
|---|---|---|
| `Float32` | IEEE-754 binary32 | `float` |
| `Double32` | Alias de binary32 | `float` |
| `Float64` | IEEE-754 binary64 | `double` |
| `Double64` | Alias de binary64 | `double` |

Os aliases preservam compatibilidade de representação. O runtime já armazena e opera com valores de ponto flutuante; a validação automatizada de compilação está sendo adicionada.

## Mapeamento C inicial

| Tipo C | Tipo MonC proposto |
|---|---|
| `void` | `Void` |
| `float` | `Float32` |
| `double` | `Float64` |
| `char` | `Char` |
| `int` | `Int32` (sujeito à validação da ABI da plataforma) |

## Estado da implementação

1. **Lexer:** reconhece literais decimais e notação exponencial como tokens `float`.
2. **Runtime Float:** armazena valores binary32/binary64, aplica os aliases e executa `+`, `-`, `*`, `/` e `%` em expressões de ponto flutuante. A suíte de smoke test inclui literais, aliases, operações e notação exponencial.
3. **AST:** reconhece `#external "header" { playCodeC { ... } }` e registra protótipos C simples.
4. **Runtime C inicial:** chamadas com parênteses para protótipos `float`/`double` de `math.h`, com zero a dois parâmetros do mesmo tipo, são resolvidas por `LoadLibrary`/`GetProcAddress` no Windows ou `dlopen`/`dlsym` em sistemas POSIX.
5. **Próximas melhorias da ABI:** suporte a `int`, `void`, parâmetros mistos, mais argumentos, bibliotecas definidas pelo usuário, ponteiros e validação mais abrangente da ABI.

## Limitações atuais

A etapa atual suporta `math.h` com retorno/parâmetros `float` ou `double`, e `stdlib.h` com retorno/parâmetros `int`, sempre com zero a dois parâmetros homogêneos. Ainda não aceita retorno `void`, ponteiros, qualificadores (`const`), structs C, funções variádicas, macros, typedefs, callbacks, arrays C, parâmetros mistos ou bibliotecas arbitrárias.
