# MonC — Linguagem de programação v1.0.0

> A especificação atual de sintaxe, tipos, EBNF, imports, overflow, bytes, erros e ownership está em [MOONLANGUAGE.md](MOONLANGUAGE.md).

> **Sintaxe atual:** chamadas usam colchetes: `print[texto]`, `somar[1, 2]` e `crypto.sha256[texto]`. Parênteses para chamadas são sintaxe desatualizada e geram erro.

**Status:** protótipo interpretado funcional em evolução. O runtime já executa um subconjunto da linguagem MonC, inclui módulos nativos e interoperabilidade inicial com `math.h` e `stdlib.h`. Geração de executáveis nativos e verificação de ownership pelo compilador continuam em desenvolvimento.

> **Estado do repositório:** protótipo nativo em C++20. `monc` resolve o arquivo de entrada e `monc` interpreta o subconjunto validado da linguagem `.monc`; o comando `monc run caminho\\para\\main.monc` é a interface recomendada.

### VS Code

O suporte inicial ao editor fica em [`editors/vscode`](editors/vscode/README.md). A extensão empacotada `editors/vscode/mooncrypto-language-1.5.4.vsix` (mais atual) pode ser instalada com `code --install-extension .\\editors\\vscode\\mooncrypto-language-1.5.4.vsix --force`. Ela adiciona realce de sintaxe, comentários, pares de colchetes e snippets para arquivos `.monc`. É suporte de edição; não oferece ainda diagnósticos do compilador.

### Compilar o protótipo

Com um compilador C++20 disponível:

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -Isrc src/main.cpp src/project.cpp src/lexer.cpp src/ast.cpp -o monc
./monc run caminho/para/main.monc
```

No Windows, use `monc.exe run caminho\\para\\main.monc`.

### Comando global `monc`

Use o nome real do arquivo, sem escrever os símbolos `<` e `>`:

```powershell
monc run caminho\para\programa.monc 
```

O binário deve estar em `C:\Program Files\MonC\monc.exe`, e essa pasta foi adicionada ao PATH do usuário. Abra um novo terminal para o comando `monc` ser reconhecido. Nesta fase, o comando executa o subconjunto interpretado da linguagem; a geração de executáveis nativos será adicionada posteriormente.

O executável configura a saída do console do Windows como UTF-8, permitindo imprimir caracteres como `é` corretamente.

### `if` e `else` no protótipo

O interpretador inicial aceita condições booleanas (`true` e `false`) e comparações inteiras (`==`, `!=`, `<`, `>`, `<=`, `>=`):

```cpp
fn main[] -> Int32 {
    if (2 > 1) {
        print["condição verdadeira"];
    } else {
        print["condição falsa"];
    }
    return 0;
}
```

Teste o exemplo com:

```powershell
.\monc.exe run Examples\Others\if_else.monc 
```

O resultado deve ser `if funcionou`. A implementação ainda é experimental e aceita apenas declarações `let` com valores inteiros e comparações simples.

### Variáveis e tipos no protótipo

O interpretador aceita variáveis locais com `let`, reatribuições e comparações. Os tipos suportados incluem:
- Inteiros (`Int32`, `Int64`, etc.)
- Textos (`String32`, `String64`, `String`)
- Caracteres (`Char` com aspas simples, ex: `'A'`)

```cpp
fn main[] -> Int32 {
    let titulo: String32 = "MonC";
    let descricao: String64 = "Linguagem nativa com foco em segurança";
    let letra: Char = 'M';
    let a: Int32 = 10;

    print["Nome: " + titulo + " | Letra: " + letra];
    print[descricao];

    if (a >= 5) {
        print["a é maior ou igual a 5"];
    }

    return 0;
}
```

O comando `print[...]` permite imprimir literais, variáveis diretamente (`print[titulo];`), concatenações de textos e números (`print["Valor: " + a];`) e caracteres.

Tipos inteiros com largura fixa validam sua faixa no interpretador: por exemplo, `UInt8` aceita apenas valores de `0` a `255`, e conversões que excedam a faixa produzem erro.

### Sintaxe de laços

Para manter a linguagem distinta de C++, os laços usam colchetes na declaração da condição:

```cpp
while [condição] {
    print["executando enquanto a condição for verdadeira"];
}

for [let i: Int32 = 0; i < 5; i = i + 1] {
    print["repetição"];
}
```

O interpretador executa o formato `for` inteiro mostrado acima e laços `while [condição]` com condições booleanas e comparações inteiras, além de suportar reatribuições de variáveis locais.

Teste o exemplo de `while` com:

```powershell
.\monc.exe run Examples\Others\while.monc 
```

### Módulos e Funções com `#external`

O interpretador permite criar funções reutilizáveis (`fn nome[params] -> Retorno { ... }`) e dividir o código em múltiplos arquivos `.monc` usando a diretiva `#external` ou `use`:

```cpp
// matematica.monc
fn somar[a: Int32, b: Int32] -> Int32 {
    return a + b;
}

fn saudar[nome: String32] -> Int32 {
    print["Olá, " + nome + "!"];
    return 0;
}

// main.monc
#external "matematica"

fn main[] -> Int32 {
    saudar["MonC"];
    let s: Int32 = somar[15, 25];
    print["Soma: " + s];
    return 0;
}
```

Teste o exemplo de módulos com:

```powershell
monc examples\modules\main.monc 
```


### Módulo JSON nativo

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


### Criptografia com `use crypto`

MonC terá um módulo criptográfico oficial. A sintaxe proposta é:

```monc
use crypto

fn main[] -> Int32 {
    let texto: String32 = "MonC";
    let h = crypto.sha256[texto];
    print["SHA-256: " + h.hex[]];
    return 0;
}
```

Para cifragem, a API deve usar criptografia autenticada:

```monc
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

`crypto.sha256` já está disponível no interpretador Windows e usa BCrypt; em Linux/macOS, o CMake usa OpenSSL EVP quando disponível. `crypto.key_from` e `crypto.aes_encrypt`/`crypto.aes_decrypt` continuam planejadas e deverão usar derivação de chave adequada e um modo AEAD como AES-GCM. Não será implementado um algoritmo criptográfico próprio.

Bytes aleatórios seguros podem ser gerados com `crypto.random_bytes[n]`; o tamanho máximo de uma chamada é 1 MiB.

Autenticação de mensagens usa `crypto.hmac_sha256[dados, chave]`, retornando um digest `Hash256`.

O interpretador também possui um modelo inicial de erro explícito com `Result`:

```monc
let resposta: Result = ok[42];
if (resposta.is_ok[]) {
    print[resposta.unwrap[]];
}
```

Comportamento implementado: prioriza `main.monc` na raiz; caso não exista, procura arquivos `.monc` com `init` no nome na raiz e em `src/`. A busca não é recursiva. Nenhum candidato gera erro, e múltiplos candidatos são listados como ambiguidade.

## Comandos disponíveis no protótipo

Com o executável `monc` compilado e acessível no PATH:

| Comando | Função |
|---|---|
| `monc run caminho/para/arquivo.monc` | Executa um arquivo MonC. |
| `monc run caminho/para/projeto` | Executa um projeto, localizando `main.monc` ou um único arquivo `init*.monc`. |
| `monc lex caminho/para/arquivo.monc` | Mostra os tokens, com tipo, linha e coluna. |
| `monc ast caminho/para/arquivo.monc` | Mostra a AST inicial reconhecida pelo parser. |

Para compilar com CMake:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Interoperabilidade com bibliotecas C

A MonC já possui integração inicial com duas bibliotecas do sistema:

- `math.h`: `sqrt`, `pow` e `sin` já estão declaradas nos exemplos/testes; o runtime aceita assinaturas simples de retorno/parâmetros `float` ou `double`.
- `stdlib.h`: `abs`, `atoi`, `rand`, `srand`, `malloc`, `free` e `NULL`.

Exemplo de declarações C:

```monc
#external "stdlib.h" {
    playCodeC {
        int atoi(const char* text);
        int rand(void);
        void srand(unsigned int seed);
        void* malloc(size_t size);
        void free(void* ptr);
    }
}

fn main[] -> Int32 {
    let numero: Int32 = atoi("123");
    srand(123);
    let aleatorio: Int32 = rand();
    let memoria = malloc(64);
    free(memoria);
    free(NULL);
    print[numero];
    print[aleatorio];
    return 0;
}
```

Ainda não estão disponíveis `atof`, `atol` e `atoll`. A integração aceita apenas as assinaturas documentadas em [C_INTEROP.md](C_INTEROP.md), não a biblioteca C inteira.

## 1. Objetivos

MonC será uma linguagem compilada para código nativo, com sintaxe familiar a quem conhece C++, mas com regras próprias para melhorar segurança e legibilidade.

Prioridades iniciais:

1. Portabilidade entre sistemas e arquiteturas populares.
2. Interoperabilidade com C e C++.
3. Segurança de memória com ownership verificado pelo compilador.
4. APIs de criptografia seguras por padrão.
5. Builds reproduzíveis por meio de versões fixadas de dependências.

## 2. Execução e compilação

O compilador, provisoriamente chamado `monc`, traduz arquivos `.monc` para código nativo. A proposta é usar LLVM como representação intermediária e deixar a vinculação final a cargo da toolchain da plataforma.

```text
.monc → análise e verificação → LLVM IR → código objeto → linker → executável
```

O artefato será um executável nativo do sistema de destino. O compilador deve permitir builds para x64 e ARM64, inicialmente nos sistemas operacionais escolhidos para a primeira versão.

Exemplo de comandos ***planejados***:

```text
monc build
monc run
monc test
```

**Vantagens:** desempenho nativo, possibilidade de compilação cruzada e acesso ao ecossistema de bibliotecas existente.

**Limitações:** cada plataforma requer toolchain, linker e configuração próprios. Usar LLVM não elimina essas diferenças.

**Decisões pendentes:** sistemas operacionais da primeira versão; versões de LLVM; suporte a compilação cruzada e builds incrementais.

## 3. Sintaxe, tipos e memória

A sintaxe usa blocos com chaves e funções tipadas. `let` declara variáveis e `const`, valores imutáveis. Conversões que possam perder dados devem ser explícitas.

```cpp
const MAX_RETRIES: UInt32 = 3;

fn main[] -> Int32 {
    let attempts: UInt32 = 0;
    print["MonC"];
    return 0;
}
```

A memória seguirá ownership verificado pelo compilador. Cada valor tem um proprietário; empréstimos permitem acesso temporário sem transferir a posse. Referências mutáveis terão regras mais restritas. Operações que não possam ser verificadas pelo compilador, como certos usos de ponteiros, ficarão em blocos `unsafe` explícitos.

```cpp
fn checksum(data: &Bytes) -> Digest {
    return crypto.hash.sha256(data);
}

fn main[] -> Int32 {
    let data = Bytes.from("MonC");
    let digest = checksum(&data);
    print[digest.to_hex[]];
    return 0;
}
```

Tipos opcionais, como `Option<T>`, poderão representar valores ausentes em vez de um `null` universal. A biblioteca padrão deverá distinguir texto (`String`) de bytes (`Bytes`).

**Vantagens:** o compilador pode identificar muitos usos inválidos de memória antes da execução; texto e dados binários não se confundem facilmente.

**Limitações:** ownership influencia a forma de estruturar programas e exige regras claras para referências, mutabilidade e interoperabilidade.

**Decisões pendentes:** genéricos, herança, coleta de memória para tipos específicos e formato exato de referências.

## 4. Funções, módulos e erros

Funções terão parâmetros e retorno tipados. Módulos organizam código e controlam visibilidade. Condicionais e laços usarão palavras-chave familiares, como `if`, `for` e `while`.

O tratamento de erros **não está definido nesta versão**. Duas opções para avaliação posterior:

```cpp
// Opção A: erros representados no tipo de retorno
fn read_config() -> Result<Config, FileError> { ... }

// Opção B: exceções
fn read_config() -> Config { ... }
```

Também pode ser avaliado um modelo combinado. A decisão afetará chamadas entre módulos, interoperabilidade e a biblioteca padrão, por isso permanecerá aberta na v0.1.

**Decisões pendentes:** `Result`, exceções ou combinação; assincronia; sobrecarga de funções; concorrência.

## 5. Bibliotecas e interoperabilidade

MonC terá módulos próprios e suporte a dependências obtidas por Git. O lockfile deverá registrar revisões exatas para que outra máquina possa reconstruir as mesmas dependências.

```toml
[dependencies]
crypto-utils = {
    git = "https://example.com/crypto-utils",
    rev = "a1b2c3d"
}
```

Para C, o compilador poderá oferecer declarações de funções com ABI C. Para C++, a estratégia inicial será adaptadores escritos em C que exponham interfaces estáveis. A vinculação C++ direta poderá ser avaliada depois, considerando compilador, ABI, sistema operacional e arquitetura.

Isso aproveita bibliotecas populares de C e C++ sem exigir que MonC reproduza detalhes específicos de ABI C++.

**Vantagens:** acesso amplo a bibliotecas existentes e menor acoplamento à ABI de um compilador C++ específico.

**Limitações:** adaptadores podem exigir código adicional; bibliotecas nativas podem precisar de builds e configurações diferentes em cada plataforma.

**Decisões pendentes:** plataformas suportadas na primeira versão; formato do manifesto; comportamento de scripts de build de dependências; políticas de segurança e confiança para dependências Git.

## 6. Criptografia

A linguagem terá uma biblioteca criptográfica oficial. A orientação é reutilizar implementações consolidadas e oferecer APIs de alto nível como caminho padrão. Primitivas de baixo nível ficarão separadas e poderão gerar avisos do linter.

Exemplo ilustrativo:

```cpp
import crypto.aead;
import crypto.random;

fn seal(key: SecretKey, message: Bytes) -> Result<SealedData, CryptoError> {
    let nonce = crypto.random.secure_nonce()?;
    return crypto.aead.encrypt(key, nonce, message);
}
```

A API oficial deverá considerar:

- Tipos distintos para chaves secretas, chaves públicas, nonces, assinaturas e dados cifrados.
- Geração de aleatoriedade segura fornecida pelo sistema operacional ou pela implementação criptográfica selecionada.
- Criptografia autenticada como opção padrão para proteger dados.
- APIs de alto nível para tarefas comuns, com configuração segura por padrão.
- Avisos contra usos de baixo nível e configurações potencialmente frágeis.
- Suporte cuidadoso a formatos de serialização e armazenamento de chaves.

A biblioteca escolhida, os algoritmos oferecidos e as garantias de limpeza de memória ainda precisam ser definidos. A linguagem e suas APIs podem reduzir riscos, mas não garantem, por si só, a segurança de um programa.

## 7. Ferramentas planejadas

O ecossistema poderá incluir:

- `monc`: compilador e interface de linha de comando.
- `monc pkg`: obtenção e fixação de dependências.
- `monc fmt`: formatador.
- `monc lint`: avisos sobre código inseguro, conversões e APIs criptográficas de baixo nível.
- `monc doc`: geração de documentação.
- `monc test`: execução de testes.

Essas ferramentas são propostas; seus comandos e formatos ainda não foram fechados.

## 8. Projeto e ponto de entrada

Estrutura sugerida:

```text
wallet/
├── moon.toml
├── src/
│   ├── main.monc
│   ├── init_crypto.monc
│   └── wallet.monc
├── tests/
└── moon.lock
```

Regra inicial de localização do ponto de entrada:

1. Procurar `main.monc` na raiz do projeto.
2. Se não existir, procurar arquivos `.monc` contendo `init` no nome, apenas na raiz e em `src/`.
3. Se houver exatamente um candidato, usá-lo.
4. Se houver vários, reportar ambiguidade e listar os caminhos.
5. Se não houver candidato, informar que falta um ponto de entrada.

A busca não será recursiva nesta versão. A sensibilidade a maiúsculas e minúsculas ainda precisa ser definida. Um campo `entry` no manifesto poderá permitir seleção explícita.

## 9. Decisões abertas para a próxima revisão

1. esses sistemas operacionais devem fazer parte da primeira versão para x64: Windows 10/11.
2. A busca por `main` deve incluir nomes em qualquer arquivo que termine com `.monc`.
3. regra de maiúsculas e minúsculas deve ser usada nos nomes de arquivo, por exemplo: `Main.monc` não ira ser igual a `main.monc`.
4. Para erros iremos introduzir primeiro `Result`, exceções ou algum outro modelo determinado depois.
6. Há uma biblioteca criptográfica consolidada que iremos adicionar posteriormente para a implementação oficial.


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
