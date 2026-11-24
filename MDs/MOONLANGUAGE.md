# Especificação de linguagem MonC v0.2

> Guias complementares: [Guia para iniciantes](GUIA_INICIANTE.md) · [Guia técnico avançado](GUIA_AVANCADO.md)

## Decisões semânticas

| Tema | Regra |
|---|---|
| Chamadas | Usam colchetes: `somar[1, 2]`, `print[texto]`, `crypto.sha256[texto]`. |
| Indexação | Usa colchetes: `bytes[0]`. O parser resolve chamada ou índice pelo símbolo: função ou variável indexável. |
| Declaração | `let nome: Tipo = expressão;`; o tipo pode ser inferido: `let nome = expressão;`. |
| Bytes | `Bytes[1, 2, 255]`; texto convertido para bytes usa UTF-8. |
| Imports | `use crypto` importa um módulo com namespace. `#external "arquivo"` carrega funções sem prefixo e deve falhar em colisões futuras. |
| Erros | APIs que podem falhar deverão retornar `Result[T, E]`; falha de autenticação não pode virar texto vazio. |
| Overflow | Inteiros comuns reportam erro em overflow. Operações modulares criptográficas usarão APIs e tipos explícitos. |
| Conversão | Conversões são explícitas. Concatenação de texto usa `+` e converte valores para texto somente nesse contexto. |

Anotações de tipo são verificadas no interpretador atual. Por exemplo, uma string não pode inicializar `Int32`, e uma atribuição posterior deve manter o tipo declarado.

## Interoperabilidade com C disponível no runtime

A MonC já integra protótipos C simples por meio de `#external "math.h"` e `#external "stdlib.h"`. As chamadas declaradas em `playCodeC` usam parênteses, enquanto funções nativas da MonC usam colchetes.

| Biblioteca | Funções/assinaturas disponíveis |
|---|---|
| `math.h` | `sqrt(double)`, `pow(double, double)`, `sin(double)`; runtime aceita assinaturas simples de `float`/`double` com até dois parâmetros do mesmo tipo. |
| `stdlib.h` | `abs(int)`, `atoi(char*)`/ `atoi(const char*)`, `rand(void)`, `srand(unsigned int)`, `malloc(size_t)`, `free(void*)` e `NULL`. |

`malloc` retorna um `void*` representado como ponteiro no runtime. `free(NULL)` é aceito. A linguagem ainda não possui ponteiros gerais, desreferência, aritmética de ponteiros ou verificação automática de use-after-free/double-free.

Ainda não estão implementadas `atof`, `atol` e `atoll`. Também não há suporte geral a macros, typedefs, structs C, callbacks, funções variádicas, parâmetros mistos ou bibliotecas arbitrárias. Consulte [C_INTEROP.md](C_INTEROP.md) para protótipos e limitações detalhadas.

## Tipos atuais e planejados

| Tipo | Estado | Descrição |
|---|---|---|
| `Int32`, `Int64` | Implementado | Inteiros assinados com validação de faixa; tipos menores e sem sinal também são verificados quando declarados. |
| `Float32`, `Float64`, `Double32`, `Double64` | Implementado no runtime inicial | `Float32`/`Double32` usam binary32; `Float64`/`Double64` usam binary64. Literais decimais e exponenciais, armazenamento, operações aritméticas básicas e validação de anotações estão implementados. |
| `String`, `String32`, `String64` | Implementado | Texto UTF-8; os sufixos ainda aguardam semântica definitiva. |
| `Char` | Implementado | Caractere. |
| `Bool` | Implementado | Valores `true` e `false`; condições aceitam `&&`, `||` e `!`, sem conversão implícita de inteiros. |
| `Byte`, `UInt8`, `UInt32`, `UInt64` | Parcialmente implementado | Anotações sem sinal e limites de largura são verificadas; aliases e operações bitwise ainda estão em evolução. |
| `Bytes` | Parcialmente implementado | `Bytes[1, 2, 255]` cria bytes e `dados[0]` indexa um byte. |
| `JsonValue` | Implementado no interpretador | Valor JSON tipado, com parser, serializador, objetos, arrays, leitura e escrita de arquivos. |
| `Result[T, E]` | Parcialmente implementado | `ok[valor]` e `err[erro]`, com `is_ok[]`, `unwrap[]` e `error[]`; a parametrização completa ainda está em evolução. |
| `SecretKey`, `Hash256`, `Ciphertext` | Especificado | Tipos opacos de criptografia. |

## Operadores

| Grupo | Operadores | Estado |
|---|---|---|
| Aritmética | `+`, `-`, `*`, `/`, `%` | Implementado para inteiros e ponto flutuante básico. |
| Comparação | `==`, `!=`, `<`, `>`, `<=`, `>=` | Implementado. |
| Lógico | `&&`, `||`, `!` | Implementado em condições. |
| Bitwise | `&`, `|`, `^`, `~`, `<<`, `>>` | Implementado para inteiros; deslocamentos inválidos produzem erro. |

Precedência planejada: chamadas e índices, unários, `* / %`, `+ -`, shifts, comparação, igualdade, bitwise, `&&`, `||`.

## Gramática EBNF

```ebnf
program        = { import | declaration } ;
import         = "use" identifier | external ;
external       = "#" "external" string [ ";" | "{" "playCodeC" "{" { c_function } "}" "}" ] ;
c_function     = c_type identifier "(" [ c_parameters ] ")" [ ";" ] ;
c_parameters   = c_parameter { "," c_parameter } ;
c_parameter    = c_type identifier ;
c_type         = identifier ;
declaration    = function | variable | constant | struct | class ;
function       = "fn" identifier "[" [ parameters ] "]" "->" type block ;
parameters     = parameter { "," parameter } ;
parameter      = identifier ":" type ;
variable       = "let" identifier [ ":" type ] "=" expression ";" ;
constant       = "const" identifier ":" type "=" expression ";" ;
block          = "{" { statement } "}" ;
statement      = variable | assignment | print | if | while | for | switch | break | continue | return | expression ";" ;
assignment     = identifier "=" expression ";" ;
print          = "print" "[" expression "]" ";" ;
if             = "if" "(" expression ")" block [ "else" ( if | block ) ] ;
while          = "while" "[" expression "]" block ;
for            = "for" "[" variable expression ";" assignment "]" block ;
switch         = "switch" "[" expression "]" "{" { "case" expression block } [ "default" block ] "}" ;
expression     = primary { operator primary } ;
primary        = literal | identifier | call | index | "(" expression ")" ;
call           = identifier "[" [ arguments ] "]" ;
index          = identifier "[" expression "]" ;
arguments      = expression { "," expression } ;
literal        = integer | float | string | char | "true" | "false" | bytes ;
float          = digit { digit } "." digit { digit } [ ( "e" | "E" ) [ "+" | "-" ] digit { digit } ]
               | digit { digit } ( "e" | "E" ) [ "+" | "-" ] digit { digit } ;
bytes          = "Bytes" "[" [ integer { "," integer } ] "]" ;
```


## Arrays nativos (`Array<T>`)


> **Nota de modificação (2026-11-21):** foi adicionada a primeira implementação de arrays nativos ao interpretador. Arrays armazenam valores em sequência, compartilham o armazenamento interno entre cópias de `Value` no interpretador e oferecem criação, indexação e operações básicas. A verificação de `Array<T>` valida os elementos na declaração e nas operações de mutação; não representa ainda generics completos nem ownership verificado pelo compilador.

```monc
fn main[] -> Int32 {
    let numeros: Array<Int32> = Array[10, 20, 30];
    array.push[numeros, 40];
    print[numeros[0]];
    print[array.length[numeros]];
    array.set[numeros, 0, 100];
    array.remove[numeros, 1];
    let ultimo: Int32 = array.pop[numeros];
    array.clear[numeros];
    return 0;
}
```

API: `Array[...]`, `array.length[array]`, `array.push[array, valor]`, `array.pop[array]`, `array.get[array, indice]`, `array.set[array, indice, valor]`, `array.remove[array, indice]` e `array.clear[array]`. Índices começam em zero. Índices inválidos, remoção de array vazio e tipos incompatíveis fazem a operação falhar no interpretador.

## Módulo JSON nativo

O interpretador inclui o módulo `json`. Ele é ativado com `#external "json"` e não exige arquivo `.monc` separado nem dependência externa.

```monc
#external "json"

fn main[] -> Int32 {
    let documento: JsonValue = json.parse["{\"nome\":\"MonC\",\"versao\":2,\"ativo\":true}"];
    let nome: String32 = json.get[documento, "nome"].as_string[];
    let versao: Int32 = json.get[documento, "versao"].as_int[];

    print["Nome: " + nome];
    print["Versão: " + versao];
    print[json.stringify[documento]];
    return 0;
}
```

Funções disponíveis:

| Função | Descrição |
|---|---|
| `json.parse[texto]` | Analisa uma string JSON e retorna `JsonValue`. |
| `json.stringify[valor]` | Serializa `JsonValue` em JSON compacto válido. |
| `json.read[caminho]` | Lê e analisa um arquivo JSON. |
| `json.write[caminho, valor]` | Grava um `JsonValue` em arquivo. |
| `json.get[objeto, chave]` | Obtém uma propriedade; para arrays, recebe um índice inteiro. |
| `json.has[objeto, chave]` | Verifica se uma propriedade existe. |
| `json.type[valor]` | Retorna `null`, `boolean`, `number`, `string`, `array` ou `object`. |
| `json.length[valor]` | Retorna o tamanho de array, objeto ou string. |
| `json.new_object[]` / `json.new_array[]` | Cria um objeto ou array vazio. |
| `json.null[]` | Cria o valor JSON `null`. |
| `json.from_string[texto]` | Converte uma string MonC em valor JSON string. |
| `json.from_int[numero]` | Converte um inteiro em valor JSON number. |
| `json.from_bool[bool]` | Converte um booleano em valor JSON boolean. |
| `json.set[objeto, chave, valor]` | Define ou substitui uma propriedade de objeto. |
| `json.push[array, valor]` | Adiciona um valor ao final de um array. |

Os valores retornados por `json.get` podem ser convertidos com `.as_string[]`, `.as_int[]` e `.as_bool[]`. Essas conversões falham quando o tipo JSON não corresponde ao tipo solicitado. `json.set` e `json.push` modificam o objeto/array compartilhado e retornam `Bool`.

O parser aceita os tipos JSON padrão, escapes de string, escapes Unicode `\\uXXXX`, números inteiros e decimais, arrays e objetos aninhados. A profundidade máxima é 128 níveis e strings de entrada/arquivos têm limite de 8 MiB. A serialização é compacta; a ordem das propriedades de objetos é ordenada pelas chaves. Erros de parsing ou de arquivo fazem a expressão falhar no interpretador.

## Ownership e segredos

Ownership será verificado pelo compilador. Valores terão um único dono; referências serão empréstimos temporários; referência mutável será exclusiva. Segredos terão tipos opacos, não poderão ser impressos e reduzirão cópias acidentais. Limpeza completa de memória dependerá da plataforma, otimizador e backend.

## Criptografia

```monc
use crypto

fn main[] -> Int32 {
    let texto = "MonC";
    let h = crypto.sha256[texto];
    print["SHA-256: " + h.hex[]];
    return 0;
}
```

`crypto.sha256` e `crypto.hmac_sha256[dados, chave]` estão implementados no Windows via BCrypt e usam OpenSSL quando o projeto é compilado em plataformas não Windows com OpenSSL disponível. `crypto.random_bytes[n]` usa o gerador seguro do sistema e limita cada pedido a 1 MiB. KDF, assinaturas, AES-GCM e comparação em tempo constante continuam planejados e deverão usar APIs consolidadas do backend, sem algoritmos próprios.

O modelo inicial de erro explícito usa `Result`. Um resultado bem-sucedido pode ser desembrulhado; um erro não pode ser confundido com texto vazio:

```monc
let resposta: Result = ok[42];
if (resposta.is_ok[]) {
    print[resposta.unwrap[]];
}
```

Erros do interpretador usam `arquivo:linha:coluna: error: mensagem`, permitindo que editores localizem o problema diretamente.

O valor inteiro retornado por `main[]` é usado como código de saída do processo:

```monc
fn main[] -> Int32 {
    return 7;
}
```

## Build reproduzível

```powershell
cmake -S . -B build
cmake --build build --config Release
```

No Windows, o alvo `monc` vincula BCrypt automaticamente. Em Linux e macOS, o CMake procura `OpenSSL::Crypto` e habilita o SHA-256 via EVP quando encontrado.

O lexer inicial pode ser inspecionado com:

```powershell
monc lex arquivo.monc
```

Ele mostra tipo, linha, coluna e texto de cada token. O parser AST também valida a estrutura das funções antes da interpretação normal.

A primeira AST pode ser inspecionada com:

```powershell
monc ast arquivo.monc
```

Por enquanto ela lista funções, parâmetros e tipos de retorno.


## Mudança de extensão e comando (2026-11-21)

A extensão oficial dos arquivos-fonte passou de `.mncyt` para `.monc`. A execução recomendada agora é `monc run caminho/para/main.monc`; para projetos, `monc run caminho/para/pasta` continua disponível e localiza `main.monc` ou um único arquivo `init*.monc`.


## ByteArray e funções built-in (Parte 1)

A branch `feature/part-1-python-builtins-bytearray` introduz o tipo mutável `ByteArray` e as primeiras funções built-in inspiradas no Python.

### Criação e acesso

```monc
let dados = bytearray[5]; // cinco bytes inicializados com zero
let outros = bytearray[Array[10, 20, 30]];

dados[0] = 12;
dados[1, 3] = 40, 50; // índices em ordem crescente
print[dados[]];       // sequência decimal separada por vírgulas
print[dados.length];  // propriedade Int32
```

Cada elemento deve ser um inteiro entre 0 e 255. Leituras e escritas fora dos limites geram erro. A atribuição com vários índices exige índices únicos em ordem estritamente crescente, quantidade igual de valores e posições já existentes. Ela não aumenta automaticamente o tamanho.

### Métodos de ByteArray

| Sintaxe | Comportamento |
|---|---|
| `dados.append[a, b, ...]` | Adiciona um ou mais bytes ao final. |
| `dados.pop[indice]` | Remove e retorna o byte naquela posição. Os seguintes avançam uma posição. |
| `dados.vpop[valor]` | Remove e retorna a primeira ocorrência do valor. Falha se não existir. |
| `dados.insert[indice, valor]` | Substitui o byte naquela posição. |
| `dados.insert[indice, +n]` | Soma `n` ao byte existente. |
| `dados.insert[indice, -n]` | Subtrai `n` do byte existente. |
| `dados.insert[indice, *n]` | Multiplica o byte existente por `n`. |
| `dados.clear[]` | Remove todos os bytes. |
| `dados.clear[indice]` | Remove o byte indicado e todos os seguintes. |
| `dados.copy[]` | Retorna uma cópia independente. |
| `dados.reverse[]` | Inverte a sequência no próprio objeto. |
| `dados.contains[valor]` | Retorna `Bool` indicando se o valor existe. |
| `dados.index[valor]` | Retorna o índice da primeira ocorrência; falha se não existir. |
| `dados.count[valor]` | Retorna a quantidade de ocorrências como `Int32`. |
| `dados.extend[origem]` | Adiciona os bytes de um `Array` ou `ByteArray` ao final. |
| `dados.extend[true, origem]` | Adiciona os bytes da origem no início. |
| `dados.slice[inicio, fim]` | Retorna um novo `ByteArray`; início inclusivo e fim exclusivo. |
| `dados.fill[valor, inicio, fim]` | Preenche o intervalo, com fim exclusivo. |

Os métodos que alteram a sequência retornam `Bool` de sucesso, exceto `pop` e `vpop`, que retornam o byte removido. `insert` limita resultados abaixo de zero a 0 e acima de 255 a 255, emitindo um aviso no terminal. A atribuição de variáveis compartilha o mesmo objeto mutável; `copy[]` cria uma sequência independente.

### Funções built-in desta etapa

| Função | Comportamento |
|---|---|
| `bool[valor]` | Converte usando as regras de truthiness. |
| `all[sequencia]` | Retorna verdadeiro se todos os elementos forem truthy; sequência vazia retorna verdadeiro. |
| `any[sequencia]` | Retorna verdadeiro se algum elemento for truthy; sequência vazia retorna falso. |
| `bin[numero]` | Retorna a representação binária com prefixo `0b`; valores fracionários são arredondados para o inteiro mais próximo, com meio afastando-se de zero. |
| `ascii[texto]` | Retorna a representação textual entre aspas, escapando caracteres não ASCII. |
| `breakpoint[]` | Atualmente é um placeholder sem interrupção do depurador; retorna 0. |

As regras de truthiness implementadas consideram falso: `false`, zero numérico, string vazia, caractere nulo, sequência vazia e JSON `null`. Os demais valores suportados são verdadeiros.

**Validação:** foi adicionado o fixture `Code MonC/SmokeTest/fixtures/bytearray_builtins.monc` e sua chamada à suíte `smoke.ps1`. A suíte CTest passou no GitHub Actions em Ubuntu e Windows (build e testes concluídos com sucesso no commit `b2d4c275`).

> **Escopo desta etapa:** este commit cobre o `ByteArray` e as built-ins `all`, `any`, `ascii`, `bin`, `bool` e o placeholder `breakpoint`. As demais built-ins da lista comparativa continuam pendentes e serão implementadas após definição de sintaxe e comportamento.
