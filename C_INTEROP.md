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

O parser AST nesta etapa reconhece o cabeçalho, o bloco `playCodeC`, os nomes das funções, tipos de retorno e parâmetros. As declarações são armazenadas em `ExternalNode::c_functions` e ainda não são vinculadas nem executadas.

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
4. **Interpretador:** chamadas e conversões adicionais ainda precisam ser concluídas.
5. **ABI/linker:** ainda precisa compilar ou vincular as bibliotecas nativas, resolver símbolos e converter argumentos/retornos com segurança.
6. **Chamadas:** a sintaxe de chamadas C com parênteses será habilitada para símbolos declarados em `playCodeC`; as chamadas normais da MonC continuam usando colchetes.

## Limitações atuais

O parser desta primeira etapa aceita protótipos simples com tipos e nomes de parâmetros. Não aceita ainda ponteiros, qualificadores (`const`), structs C, variádicas, macros, typedefs, callbacks, arrays C ou declarações complexas. A declaração de um protótipo não implica que o símbolo esteja disponível na linkedição.
