# Guia técnico avançado da MonC

> Referência técnica para quem já programa e deseja entender as regras atuais da linguagem, o comportamento do interpretador e os limites da implementação.

[Voltar à documentação](README.md) · [Guia para iniciantes](GUIA_INICIANTE.md) · [Especificação formal](MOONLANGUAGE.md)

## 1. Escopo e estado da implementação

MonC é atualmente um protótipo interpretado. O runtime é implementado em C++20 e executa um subconjunto da linguagem descrito nos arquivos `.monc`.

Não confunda três camadas:

1. **Sintaxe aceita:** o que lexer/parser conseguem reconhecer.
2. **Comportamento do runtime:** o que o interpretador realmente executa.
3. **Projeto futuro:** recursos descritos em roadmap ou especificações que ainda não estão implementados.

Esta documentação prioriza o comportamento observado no código desta branch. A geração de código nativo, ownership verificado estaticamente e um sistema completo de tipos continuam fora do escopo atual.

## 2. Pipeline de execução

O fluxo conceitual do runtime é:

```text
arquivo .monc
    |
    v
lexer (tokens)
    |
    v
parser / AST
    |
    v
registro de funções, imports e declarações externas
    |
    v
interpretador
    |
    v
efeitos: saída, chamadas C suportadas e operações nativas
```

Comandos úteis:

```powershell
monc lex caminho\para\arquivo.monc
monc ast caminho\para\arquivo.monc
monc run caminho\para\arquivo.monc
```

- `lex` apresenta tokens e posições.
- `ast` apresenta a estrutura reconhecida pelo parser.
- `run` executa o arquivo ou projeto.

A AST inicial expõe funções, parâmetros e tipos de retorno, mas não representa ainda uma IR de compilador nativo.

## 3. Gramática de funções

A forma recomendada para declarar uma função é:

```ebnf
function    = "fn" identifier "[" [ parameters ] "]" [ "->" type ] block ;
parameters  = parameter { "," parameter } ;
parameter   = identifier ":" type ;
```

Exemplos:

```monc
fn identity_int[value: Int32] -> Int32 {
    return value;
}

fn identity_float[value: Float64] -> Float64 {
    return value;
}

fn identity_string[value: String32] -> String32 {
    return value;
}

fn is_valid[value: Bool] -> Bool {
    return value;
}

fn log_message[message: String32] {
    print[message];
}
```

O parser atual também aceita parênteses na lista de parâmetros de uma definição em alguns caminhos internos, mas a sintaxe canônica/documentada da MonC usa colchetes. Chamadas de funções MonC também usam colchetes.

Sem uma cláusula `->`, o tipo de retorno registrado é `Void`.

## 4. Parâmetros, argumentos e retorno

### 4.1 Ordem e aridade

Os argumentos são associados aos parâmetros por posição. O runtime verifica a quantidade de argumentos em chamadas de funções MonC. Uma quantidade diferente da declarada resulta em erro.

```monc
fn combinar[a: Int32, b: Int32] -> Int32 {
    return a + b;
}

fn main[] -> Int32 {
    print[combinar[10, 20]];
    return 0;
}
```

Não há suporte documentado nesta versão para parâmetros nomeados, valores padrão, parâmetros variádicos ou sobrecarga por assinatura.

### 4.2 Tipos de parâmetro e retorno

Tipos usados nas assinaturas devem corresponder aos nomes reconhecidos pelo runtime:

| Família | Tipos |
|---|---|
| Inteiros | `Int8`, `Int16`, `Int32`, `Int64`, `UInt8`, `UInt32`, `UInt64`, `Byte` |
| Ponto flutuante | `Float32`, `Float64`, `Double32`, `Double64` |
| Texto | `String`, `String32`, `String64` |
| Escalar | `Bool`, `Char` |
| Sequências | `Bytes`, `ByteArray`, `Array<T>` |
| Estruturados | `JsonValue`, `Result` |
| Outros | `Pointer` e aliases de ponteiro usados na interoperabilidade C |

Os tipos numéricos e aliases têm regras de largura/faixa descritas em [MOONLANGUAGE.md](MOONLANGUAGE.md).

### 4.3 Estado real da verificação de assinaturas

Este ponto é especialmente importante ao escrever APIs:

- O parser armazena o nome e o tipo declarado de cada parâmetro e o tipo de retorno da função.
- Na chamada, o runtime verifica a aridade e associa cada argumento à variável de parâmetro correspondente.
- Na implementação atual, o tipo declarado do parâmetro é aplicado ao valor recebido, mas a chamada não faz uma validação completa de compatibilidade antes dessa associação.
- O valor de `return` é avaliado e transportado para o chamador, mas não há uma verificação geral de que ele corresponda ao tipo declarado após `->`.
- Em contraste, `let nome: Tipo = expressão;` e reatribuições a variáveis tipadas passam por `value_matches_type`.

Portanto, as assinaturas são importantes como documentação e estrutura da linguagem, mas ainda não equivalem a um sistema estático completo. Não use o tipo declarado de uma função como substituto de testes ou validação de dados.

### 4.4 Retorno e fluxo de controle

`return expressão;` avalia a expressão e marca a execução da função como encerrada. O valor é propagado ao chamador.

Se a função termina sem executar `return`, o caminho atual de chamada devolve internamente um inteiro zero. Esse detalhe não deve ser tratado como contrato definitivo de `Void`.

O runtime atual não garante que todos os caminhos de uma função com retorno tipado retornem um valor do tipo esperado.

## 5. Representação de valores e tipos

O runtime usa um valor dinâmico interno (`Value`) com uma categoria de armazenamento e um nome de tipo. Entre as categorias internas estão:

- Integer
- Float
- String
- Char
- Boolean
- Bytes
- ByteArray
- Array
- Json
- Result
- Pointer

A função interna `value_matches_type` relaciona nomes de tipos MonC às categorias de valor e, para inteiros, também verifica limites de faixa. Ela é usada nas declarações e reatribuições tipadas, além de operações específicas de coleções.

### 5.1 Inteiros

Os inteiros são armazenados como valores inteiros com verificação de faixa conforme o tipo declarado. Overflow em operações inteiras comuns deve produzir erro, conforme a especificação atual.

### 5.2 Ponto flutuante

Os valores de ponto flutuante usam armazenamento interno de precisão dupla. Para `Float32` e `Double32`, o runtime reduz a precisão para binary32 ao atribuir o valor; `Float64` e `Double64` preservam binary64.

Isso é uma decisão de representação do runtime, não uma afirmação sobre ABI de todas as plataformas.

### 5.3 Strings e caracteres

Strings são armazenadas como `std::string` e tratadas como texto UTF-8 pelo projeto. `Char` é armazenado como um `char` no runtime atual, portanto não deve ser interpretado como um sistema completo de caracteres Unicode de múltiplos bytes.

### 5.4 Coleções e mutabilidade

`Array` e `ByteArray` usam armazenamento compartilhado por `shared_ptr` entre cópias internas de `Value`. Consequentemente, atribuir uma coleção a outra variável pode compartilhar o mesmo armazenamento mutável. `ByteArray.copy[]` cria uma cópia independente.

A anotação `Array<T>` valida elementos em declarações e operações de mutação específicas, mas não constitui ainda um sistema genérico completo nem ownership verificado estaticamente.

## 6. Conversões e operadores

A regra de projeto é favorecer conversões explícitas. A concatenação textual com `+` é uma exceção contextual documentada: valores escalares suportados podem ser convertidos em texto durante a concatenação.

Operadores documentados:

| Categoria | Operadores |
|---|---|
| Aritméticos | `+`, `-`, `*`, `/`, `%` |
| Comparação | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Lógicos | `&&`, `||`, `!` |
| Bitwise | `&`, `|`, `^`, `~`, `<<`, `>>` |

A cobertura de cada operador depende das categorias de valores e dos caminhos de avaliação implementados. Consulte a especificação e os testes antes de assumir que uma combinação arbitrária de tipos é aceita.

### 6.1 Built-ins numéricos e de sequências

O runtime implementa um subconjunto de funções built-in inspiradas no Python. Elas são reconhecidas como chamadas nativas e usam a sintaxe MonC com colchetes.

| Função | Argumentos aceitos | Resultado e observações |
|---|---|---|
| `float[valor]` | Inteiro, float, booleano ou string numérica completa | Retorna `Float64`. Booleanos viram `1.0` ou `0.0`. |
| `int[valor]` | Inteiro, float finito, booleano ou string inteira decimal completa | Retorna `Int64`. Floats são truncados em direção a zero. |
| `hex[numero]` | Inteiro | Retorna string com prefixo `0x`; negativos preservam o sinal. |
| `hash[valor]` | Inteiro, float, string, caractere, booleano ou `Bytes` | Retorna hash FNV-1a determinístico limitado a 63 bits. Não é compatível com o hash do Python e não é criptográfico. |
| `len[valor]` | String UTF-8, `Array`, `ByteArray`, `Bytes` ou `JsonValue` string/array/objeto | Retorna `Int32`. Strings contam pontos de código UTF-8. |
| `max[sequencia]` / `max[a, b, ...]` | `Array`, `ByteArray`, `Bytes` ou argumentos comparáveis | Retorna o maior valor original, preservando seu tipo. |
| `min[sequencia]` / `min[a, b, ...]` | Mesmos tipos de `max` | Retorna o menor valor original, preservando seu tipo. |

Exemplo:

```monc
fn main[] -> Int32 {
    let decimal: Float64 = float["12.5"];
    let inteiro: Int64 = int[3.9];
    let codigo: String32 = hex[255];
    let valores = Array[8, 2, 15, 4];
    let maior: Int32 = max[valores];
    let menor: Int32 = min[valores];
    let quantidade: Int32 = len[valores];
    print[decimal];
    print[inteiro];
    print[codigo];
    print[maior];
    print[menor];
    print[quantidade];
    return 0;
}
```

Restrições relevantes:

- `float` e `int` rejeitam strings vazias, texto parcialmente numérico e categorias não suportadas.
- `int` rejeita valores float não finitos e valores fora da faixa de `Int64`.
- `hex` recebe somente valores inteiros.
- `hash` é determinístico nesta implementação; não deve ser usado como substituto de SHA-256, HMAC ou outro hash criptográfico.
- `len` falha para tipos sem comprimento definido, como números e booleanos.
- `max` e `min` falham para sequências vazias e elementos que não possam ser comparados entre si.

## 7. Módulos e resolução de código

A diretiva `#external "nome"` carrega outro arquivo MonC ou um módulo reconhecido pelo runtime. Funções importadas são registradas no ambiente de execução.

```monc
#external "math"

fn main[] -> Int32 {
    print[somar[2, 3]];
    return 0;
}
```

A resolução de arquivos e os caminhos de busca dependem da estrutura do projeto. Consulte os exemplos e o README principal para os comandos atuais de execução.

## 8. Interoperabilidade C

A interoperabilidade usa uma declaração `#external` com bloco `playCodeC`:

```monc
#external "math.h" {
    playCodeC {
        double sqrt(double value);
        double pow(double base, double exponent);
    }
}

fn main[] -> Int32 {
    let raiz: Float64 = sqrt(81.0);
    let potencia: Float64 = pow(2.0, 5.0);
    print[raiz];
    print[potencia];
    return 0;
}
```

Há uma diferença intencional de sintaxe:

- Função MonC: `somar[1, 2]`.
- Função C declarada em `playCodeC`: `sqrt(81.0)`.

O runtime atual reconhece apenas `math.h` e `stdlib.h` nas combinações de assinatura descritas em [C_INTEROP.md](C_INTEROP.md). Não interprete a declaração de um protótipo como suporte automático à biblioteca inteira.

## 9. Módulos nativos

### JSON

O módulo `json` oferece parsing, serialização, leitura e escrita de arquivos, acesso e mutação de objetos/arrays e conversões tipadas. Veja [MOONLANGUAGE.md](MOONLANGUAGE.md) para a API disponível.

### Criptografia

O módulo `crypto` inclui SHA-256, HMAC-SHA-256 e geração de bytes aleatórios seguros no runtime atual, com backend dependente da plataforma. Não confunda as funções implementadas com as APIs criptográficas planejadas no roadmap.

## 10. Erros e diagnóstico

Erros do interpretador seguem o formato:

```text
arquivo:linha:coluna: error: mensagem
```

O formato permite que editores localizem a origem do erro. O lexer e o parser também podem ser executados separadamente com `monc lex` e `monc ast`.

## 11. Testes e fixtures

A suíte SmokeTest usa fixtures MonC e scripts de verificação. Ao documentar uma nova funcionalidade, é recomendável:

1. Criar um exemplo pequeno em `Code MonC/Examples`.
2. Criar um fixture de regressão em `Code MonC/SmokeTest/fixtures`.
3. Adicionar uma verificação ao script SmokeTest correspondente.
4. Executar CMake build e CTest em ambiente local.
5. Confirmar os resultados do CI nos sistemas suportados.

Um exemplo visual não substitui um teste automatizado. Da mesma forma, um fixture aprovado cobre apenas os comportamentos exercitados nele.

## 12. Limitações técnicas conhecidas

- Não há geração de executáveis nativos.
- Não há type checker estático completo.
- A validação de parâmetros e valores de retorno de funções MonC ainda é incompleta.
- Generics são limitados e não equivalem a um sistema completo de tipos parametrizados.
- Ownership e borrowing não são verificados pelo compilador.
- Ponteiros gerais, desreferência e aritmética de ponteiros não estão disponíveis.
- A interoperabilidade C aceita apenas assinaturas específicas.
- Macros, structs, unions, callbacks e funções C variádicas não são suportados de forma geral.
- A compatibilidade completa com Python não é objetivo desta etapa.

## 13. Referências internas

- [Especificação da linguagem e EBNF](MOONLANGUAGE.md)
- [Interoperabilidade C](C_INTEROP.md)
- [Módulo JSON](JSON_MODULE.md)
- [Mapa de desenvolvimento](MOONHOLDMAP.md)
- [Roadmap](ROADMAP_MOONCRYPTO.md)
- [Guia para iniciantes](GUIA_INICIANTE.md)
