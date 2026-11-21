# Especificação de linguagem MoonCrypto v0.2

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

## Tipos atuais e planejados

| Tipo | Estado | Descrição |
|---|---|---|
| `Int32`, `Int64` | Implementado | Inteiros assinados com validação de faixa; tipos menores e sem sinal também são verificados quando declarados. |
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
| Aritmética | `+`, `-`, `*`, `/`, `%` | Implementado para inteiros. |
| Comparação | `==`, `!=`, `<`, `>`, `<=`, `>=` | Implementado. |
| Lógico | `&&`, `||`, `!` | Implementado em condições. |
| Bitwise | `&`, `|`, `^`, `~`, `<<`, `>>` | Implementado para inteiros; deslocamentos inválidos produzem erro. |

Precedência planejada: chamadas e índices, unários, `* / %`, `+ -`, shifts, comparação, igualdade, bitwise, `&&`, `||`.

## Gramática EBNF

```ebnf
program        = { import | declaration } ;
import         = "use" identifier | "#external" string ;
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
literal        = integer | string | char | "true" | "false" | bytes ;
bytes          = "Bytes" "[" [ integer { "," integer } ] "]" ;
```


## Módulo JSON nativo

O interpretador inclui o módulo `json`. Ele é ativado com `#external "json"` e não exige arquivo `.mncyt` separado nem dependência externa.

```mncyt
#external "json"

fn main[] -> Int32 {
    let documento: JsonValue = json.parse["{\"nome\":\"MoonCrypto\",\"versao\":2,\"ativo\":true}"];
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
| `json.from_string[texto]` | Converte uma string MoonCrypto em valor JSON string. |
| `json.from_int[numero]` | Converte um inteiro em valor JSON number. |
| `json.from_bool[bool]` | Converte um booleano em valor JSON boolean. |
| `json.set[objeto, chave, valor]` | Define ou substitui uma propriedade de objeto. |
| `json.push[array, valor]` | Adiciona um valor ao final de um array. |

Os valores retornados por `json.get` podem ser convertidos com `.as_string[]`, `.as_int[]` e `.as_bool[]`. Essas conversões falham quando o tipo JSON não corresponde ao tipo solicitado. `json.set` e `json.push` modificam o objeto/array compartilhado e retornam `Bool`.

O parser aceita os tipos JSON padrão, escapes de string, escapes Unicode `\\uXXXX`, números inteiros e decimais, arrays e objetos aninhados. A profundidade máxima é 128 níveis e strings de entrada/arquivos têm limite de 8 MiB. A serialização é compacta; a ordem das propriedades de objetos é ordenada pelas chaves. Erros de parsing ou de arquivo fazem a expressão falhar no interpretador.

## Ownership e segredos

Ownership será verificado pelo compilador. Valores terão um único dono; referências serão empréstimos temporários; referência mutável será exclusiva. Segredos terão tipos opacos, não poderão ser impressos e reduzirão cópias acidentais. Limpeza completa de memória dependerá da plataforma, otimizador e backend.

## Criptografia

```mncyt
use crypto

fn main[] -> Int32 {
    let texto = "MoonCrypto";
    let h = crypto.sha256[texto];
    print["SHA-256: " + h.hex[]];
    return 0;
}
```

`crypto.sha256` e `crypto.hmac_sha256[dados, chave]` estão implementados no Windows via BCrypt e usam OpenSSL quando o projeto é compilado em plataformas não Windows com OpenSSL disponível. `crypto.random_bytes[n]` usa o gerador seguro do sistema e limita cada pedido a 1 MiB. KDF, assinaturas, AES-GCM e comparação em tempo constante continuam planejados e deverão usar APIs consolidadas do backend, sem algoritmos próprios.

O modelo inicial de erro explícito usa `Result`. Um resultado bem-sucedido pode ser desembrulhado; um erro não pode ser confundido com texto vazio:

```mncyt
let resposta: Result = ok[42];
if (resposta.is_ok[]) {
    print[resposta.unwrap[]];
}
```

Erros do interpretador usam `arquivo:linha:coluna: error: mensagem`, permitindo que editores localizem o problema diretamente.

O valor inteiro retornado por `main[]` é usado como código de saída do processo:

```mncyt
fn main[] -> Int32 {
    return 7;
}
```

## Build reproduzível

```powershell
cmake -S . -B build
cmake --build build --config Release
```

No Windows, o alvo `mooncyt` vincula BCrypt automaticamente. Em Linux e macOS, o CMake procura `OpenSSL::Crypto` e habilita o SHA-256 via EVP quando encontrado.

O lexer inicial pode ser inspecionado com:

```powershell
mooncyt lex arquivo.mncyt
```

Ele mostra tipo, linha, coluna e texto de cada token. O parser AST também valida a estrutura das funções antes da interpretação normal.

A primeira AST pode ser inspecionada com:

```powershell
mooncyt ast arquivo.mncyt
```

Por enquanto ela lista funções, parâmetros e tipos de retorno.
