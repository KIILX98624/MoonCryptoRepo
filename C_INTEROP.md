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

Os aliases preservam compatibilidade de representação. A checagem de tipos e as conversões numéricas no interpretador ainda não estão implementadas nesta etapa.

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
2. **AST:** reconhece `#external "header" { playCodeC { ... } }` e registra protótipos C simples.
3. **Interpretador:** ainda precisa armazenar valores float, avaliar operações aritméticas e validar `Float32`/ `Float64`/ aliases.
4. **ABI/linker:** ainda precisa compilar ou vincular as bibliotecas nativas, resolver símbolos e converter argumentos/retornos com segurança.
5. **Chamadas:** a sintaxe de chamadas C com parênteses será habilitada para símbolos declarados em `playCodeC`; as chamadas normais da MonC continuam usando colchetes.

## Limitações atuais

O parser desta primeira etapa aceita protótipos simples com tipos e nomes de parâmetros. Não aceita ainda ponteiros, qualificadores (`const`), structs C, variádicas, macros, typedefs, callbacks, arrays C ou declarações complexas. A declaração de um protótipo não implica que o símbolo esteja disponível na linkedição.
