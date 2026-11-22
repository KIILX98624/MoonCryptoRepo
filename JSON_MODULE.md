# MoonCrypto JSON Module

**Arquivo:** `JSON_MODULE.md`  
**Módulo proposto:** `json`  
**Status:** Especificação inicial, ainda não implementada no interpretador.

Este documento propõe uma API JSON para a MoonCrypto com foco em interoperabilidade entre linguagens, leitura e gravação de arquivos e integração futura ao interpretador C++20.

> **Importante:** as funções descritas aqui são uma proposta de API. O HoldMap fornecido não indica suporte atual a `use json`, `JsonValue` ou operações de arquivos. O exemplo `.monc` que acompanha este documento demonstra a sintaxe pretendida e não deve ser considerado executável no protótipo atual até que o módulo seja implementado.

---

## 1. Objetivos

O módulo deve permitir que programas MoonCrypto:

- interpretem texto JSON;
- convertam valores JSON em texto;
- leiam arquivos `.json`;
- gravem arquivos `.json`;
- consultem e alterem objetos e arrays JSON;
- troquem dados com programas escritos em Python, C++, JavaScript, Rust e outras linguagens.

O formato deve seguir a especificação JSON (RFC 8259), usando UTF-8 para arquivos e texto serializado.

## 2. Importação e sintaxe

A importação proposta segue o padrão de módulo qualificado:

```monc
use json
```

Todas as chamadas usam colchetes, seguindo a regra atual da MoonCrypto:

```monc
json.parse[texto]
json.stringify[valor]
json.read["dados.json"]
json.write["dados.json", valor]
```

Não introduzir chamadas com parênteses no módulo.

## 3. Tipo principal: `JsonValue`

O módulo deve oferecer um tipo opaco chamado `JsonValue`.

Ele representa qualquer valor JSON válido:

| Tipo JSON | Representação conceitual |
|---|---|
| `null` | valor nulo |
| `true` / `false` | booleano |
| número | número JSON |
| string | texto Unicode |
| array | sequência ordenada de `JsonValue` |
| objeto | coleção de pares chave/valor |

`JsonValue` não deve ser confundido com `String`. Uma string que contém `"{\"x\":1}"` ainda é texto comum até passar por `json.parse`.

A representação interna pode usar uma biblioteca C++ consolidada, mas detalhes de implementação não devem fazer parte da API pública da linguagem.

## 4. API pública proposta

### 4.1 Interpretar texto

```monc
let valor: JsonValue = json.parse[texto];
```

- Recebe uma string contendo um documento JSON.
- Retorna um `JsonValue`.
- Deve rejeitar JSON inválido com diagnóstico que informe, quando possível, a posição do erro.
- Deve validar UTF-8 e a estrutura do documento.

### 4.2 Serializar

```monc
let texto: String32 = json.stringify[valor];
```

- Converte um `JsonValue` em texto JSON válido.
- Deve escapar aspas, barras invertidas e caracteres de controle corretamente.
- Deve produzir UTF-8.
- A ordem das chaves de objetos não deve ser considerada significativa.
- A serialização compacta deve ser o padrão.

Uma opção de formatação pode ser adicionada depois, por exemplo `json.stringify_pretty[valor, 4]`. Não é necessária para a primeira versão.

### 4.3 Ler arquivo

```monc
let dados: JsonValue = json.read["conversor.json"];
```

- Lê o arquivo em modo binário e interpreta seu conteúdo como UTF-8.
- O caminho é relativo ao diretório de trabalho do processo, a menos que seja absoluto.
- Deve informar erros de arquivo inexistente, falta de permissão, falha de leitura, UTF-8 inválido e JSON inválido.
- Deve impor um limite configurável de tamanho para evitar consumo excessivo de memória.

### 4.4 Gravar arquivo

```monc
json.write["conversor.json", dados];
```

- Serializa o valor e grava UTF-8.
- Deve substituir o conteúdo do arquivo quando ele já existir.
- Deve informar erros de caminho, permissão e gravação.
- Recomenda-se gravar primeiro em arquivo temporário no mesmo diretório e depois substituir o destino, reduzindo o risco de deixar um arquivo parcialmente escrito.
- A primeira versão não deve prometer gravação atômica em todos os sistemas de arquivos.

### 4.5 Criar valores

Construtores propostos:

```monc
let objeto: JsonValue = json.object[];
let lista: JsonValue = json.array[];
let texto: JsonValue = json.string["MoonCrypto"];
let numero: JsonValue = json.number[42];
let booleano: JsonValue = json.boolean[true];
let nulo: JsonValue = json.null[];
```

A conversão numérica deve rejeitar valores não finitos (`NaN`, infinito positivo e infinito negativo), pois eles não são números JSON válidos.

### 4.6 Consultar e alterar objetos

```monc
json.set[objeto, "nome", json.string["MoonCrypto"]];
let nome: JsonValue = json.get[objeto, "nome"];
let existe: Bool = json.has[objeto, "nome"];
json.remove[objeto, "nome"];
```

Regras:

- `json.set` insere uma chave nova ou substitui seu valor.
- `json.get` falha com erro claro quando a chave não existe.
- `json.has` retorna `false` quando a chave não existe.
- `json.remove` remove a chave, ou informa erro se a chave não existir (a decisão deve ser consistente e documentada).
- Chaves de objetos são strings.

### 4.7 Consultar e alterar arrays

```monc
json.push[lista, json.number[10]];
let item: JsonValue = json.at[lista, 0];
let quantidade: UInt64 = json.length[lista];
json.remove_at[lista, 0];
```

Regras:

- Índices começam em zero.
- `json.at` e `json.remove_at` devem validar os limites.
- `json.length` retorna a quantidade de elementos de arrays ou membros de objetos. Para strings, deve ser definida separadamente e não deve contar bytes como se fossem caracteres.

## 5. Conversões tipadas

Para evitar conversões implícitas e erros silenciosos, oferecer funções explícitas:

```monc
let nome: String32 = json.as_string[json.get[dados, "nome"]];
let idade: Int32 = json.as_int32[json.get[dados, "idade"]];
let ativo: Bool = json.as_bool[json.get[dados, "ativo"]];
let preco: Float64 = json.as_float64[json.get[dados, "preco"]];
```

Essas conversões devem falhar se o tipo JSON não for compatível ou se o número estiver fora da faixa do tipo de destino.

**Dependência de tipos:** o HoldMap atual lista `Int32`, `Int64`, strings e `Bool` em verificações de tipos, mas não confirma `Float64` como tipo implementado. `Float64` aparece acima como tipo desejado para interoperabilidade numérica e depende de suporte numérico correspondente na linguagem. Até lá, os valores numéricos podem permanecer como `JsonValue`.

Também podem existir verificações sem conversão:

```monc
json.is_object[valor]
json.is_array[valor]
json.is_string[valor]
json.is_number[valor]
json.is_bool[valor]
json.is_null[valor]
```

## 6. Erros e segurança

O HoldMap e o roadmap indicam que o modelo formal de erros da MoonCrypto ainda precisa ser definido. Portanto, a primeira integração pode emitir diagnósticos de runtime padronizados, sem fingir que existe um tipo `Result` implementado.

Erros mínimos:

- JSON malformado;
- tipo inesperado em uma operação;
- chave inexistente;
- índice fora dos limites;
- arquivo inexistente ou inacessível;
- falha de leitura ou gravação;
- codificação UTF-8 inválida;
- documento ou arquivo acima do limite permitido;
- número incompatível com o tipo solicitado.

Quando a linguagem tiver um modelo de erros formal, as operações que podem falhar devem migrar para um retorno explícito, como `Result<JsonValue, JsonError>` ou equivalente aprovado pelo design da linguagem.

O módulo não deve executar conteúdo, interpretar campos como código ou buscar arquivos pela rede. Ler JSON deve ser apenas ler e interpretar dados.

## 7. Interoperabilidade

Para garantir compatibilidade com Python:

```python
import json

dados = {
    "n1": 10.5,
    "n2": 5.5
}

with open("conversor.json", "w", encoding="utf-8") as arquivo:
    json.dump(dados, arquivo, ensure_ascii=False, indent=4)
```

A MoonCrypto deve conseguir ler esse arquivo com:

```monc
use json

fn main[] -> Int32 {
    let dados: JsonValue = json.read["conversor.json"];
    print[json.stringify[dados]];
    return 0;
}
```

O arquivo JSON não pertence ao Python nem à MoonCrypto. É um formato de intercâmbio compartilhado.

## 8. Recomendações para a implementação em C++20

1. Escolher uma biblioteca C++ JSON consolidada e compatível com a licença do projeto.
2. Encapsular a biblioteca atrás de `JsonValue`; não expor seus tipos C++ ao usuário da MoonCrypto.
3. Registrar `json` como módulo nativo no mecanismo de módulos do interpretador.
4. Adicionar os builtins e validar quantidade e tipos dos argumentos.
5. Implementar leitura e gravação com `std::filesystem` e fluxos binários, respeitando UTF-8.
6. Converter exceções da biblioteca em diagnósticos MoonCrypto estáveis, sem encerrar o processo abruptamente.
7. Aplicar limites de tamanho e profundidade de aninhamento.
8. Criar testes de ida e volta (`parse` → `stringify` → `parse`), arquivos inválidos, Unicode, arrays, objetos, números e erros de I/O.

A biblioteca escolhida deve ser avaliada quanto a manutenção, portabilidade, desempenho, licença e facilidade de integração. A implementação não deve criar um parser JSON próprio sem uma necessidade técnica justificada.

## 9. Plano incremental

| Etapa | Entrega | Prioridade |
|---|---|---|
| 1 | Tipo opaco `JsonValue` e `json.parse` | Alta |
| 2 | `json.stringify` | Alta |
| 3 | `json.read` e `json.write` | Alta |
| 4 | Construtores de valores | Alta |
| 5 | Acesso e alteração de objetos | Alta |
| 6 | Acesso e alteração de arrays | Média |
| 7 | Conversões tipadas e verificações | Média |
| 8 | Modelo formal de erros | Conforme evolução da linguagem |

## 10. Critérios de aceitação

A primeira versão será considerada utilizável quando:

- um arquivo produzido por `json.dump` do Python puder ser lido;
- a MoonCrypto puder serializar novamente os dados sem perder sua estrutura;
- a MoonCrypto puder gravar um arquivo que o Python consiga abrir com `json.load`;
- strings Unicode, números, booleanos, `null`, arrays e objetos forem preservados;
- erros de sintaxe e de I/O gerarem diagnósticos claros;
- testes automatizados cobrirem os casos válidos e inválidos.

---

**Status final deste documento:** proposta técnica para discussão e implementação. Não representa um recurso já disponível no protótipo.
