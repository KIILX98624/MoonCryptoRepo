# MoonCrypto — Especificação técnica inicial v1.0

> A especificação atual de sintaxe, tipos, EBNF, imports, overflow, bytes, erros e ownership está em [MOONLANGUAGE.md](MOONLANGUAGE.md).

> **Sintaxe atual:** chamadas usam colchetes: `print[texto]`, `somar[1, 2]` e `crypto.sha256[texto]`. Parênteses para chamadas são sintaxe desatualizada e geram erro.

**Status:** proposta para revisão. A execução nativa, ownership verificado pelo compilador, suporte a Git e prioridades de portabilidade e interoperabilidade com C++ estão definidos como direção. O tratamento de erros continua em aberto.

> **Estado do repositório:** protótipo nativo em C++20. `mooncyt {name}.mncyt` resolve o arquivo de entrada e `mooncyt {name}.mncyt` interpreta o subconjunto validado da linguagem `.mncyt`; o comando `mooncyt arquivo.mncyt` é a interface direta recomendada.

### VS Code

O suporte inicial ao editor fica em [`editors/vscode`](editors/vscode/README.md). A extensão empacotada `editors/vscode/mooncrypto-language-1.5.4.vsix` pode ser instalada com `code --install-extension .\\editors\\vscode\\mooncrypto-language-1.5.4.vsix --force`. Ela adiciona realce de sintaxe, comentários, pares de colchetes e snippets para arquivos `.mncyt`. É suporte de edição; não oferece ainda diagnósticos do compilador.

### Compilar o protótipo

Com um compilador C++20 disponível:

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -Isrc src/main.cpp src/project.cpp src/lexer.cpp src/ast.cpp -o moon.exe
./mooncyt examples/hello
./mooncyt examples/hello
```

No Windows. O primeiro comando retorna `main.mncyt`; o segundo exibe `Hello, MoonCrypto!`.

### Comando global `mooncyt`

Use o nome real do arquivo, sem escrever os símbolos `<` e `>`:

```powershell
mooncyt caminho\para\programa.mncyt
```

O binário está em `C:\Program Files\MoonCrypto\mooncyt.exe`, e essa pasta foi adicionada ao PATH do usuário. Abra um novo terminal para o comando `mooncyt` ser reconhecido. Nesta fase, o comando executa o subconjunto interpretado da linguagem; a geração de executáveis nativos será adicionada posteriormente.

O executável configura a saída do console do Windows como UTF-8, permitindo imprimir caracteres como `é` sejam imprimidos corretamente.

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
.\moon.exe run examples\if_else  **(Erro Descoberto!!)**
```

O resultado deve ser `if funcionou`. A implementação ainda é experimental e aceita apenas declarações `let` com valores inteiros e comparações simples.

### Variáveis e tipos no protótipo

O interpretador aceita variáveis locais com `let`, reatribuições e comparações. Os tipos suportados incluem:
- Inteiros (`Int32`, `Int64`, etc.)
- Textos (`String32`, `String64`, `String`)
- Caracteres (`Char` com aspas simples, ex: `'A'`)

```cpp
fn main[] -> Int32 {
    let titulo: String32 = "MoonCrypto";
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
.\moon.exe run examples\while  **(Erro Descoberto!!)**
```

### Módulos e Funções com `#external`

O interpretador permite criar funções reutilizáveis (`fn nome[params] -> Retorno { ... }`) e dividir o código em múltiplos arquivos `.mncyt` usando a diretiva `#external`:

```cpp
// matematica.mncyt
fn somar[a: Int32, b: Int32] -> Int32 {
    return a + b;
}

fn saudar[nome: String32] -> Int32 {
    print["Olá, " + nome + "!"];
    return 0;
}

// main.mncyt
#external "matematica"

fn main[] -> Int32 {
    saudar["MoonCrypto"];
    let s: Int32 = somar[15, 25];
    print["Soma: " + s];
    return 0;
}
```

Teste o exemplo de módulos com:

```powershell
mooncyt examples\modules\main.mncyt **(Esse funciona)**
```


### Módulo JSON nativo

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


### Criptografia com `use crypto`

MoonCrypto terá um módulo criptográfico oficial. A sintaxe proposta é:

```mncyt
use crypto

fn main[] -> Int32 {
    let texto: String32 = "MoonCrypto";
    let h = crypto.sha256[texto];
    print["SHA-256: " + h.hex[]];
    return 0;
}
```

Para cifragem, a API deve usar criptografia autenticada:

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

`crypto.sha256` já está disponível no interpretador Windows e usa BCrypt; em Linux/macOS, o CMake usa OpenSSL EVP quando disponível. `crypto.key_from` e `crypto.aes_encrypt`/`crypto.aes_decrypt` continuam planejadas e deverão usar derivação de chave adequada e um modo AEAD como AES-GCM. Não será implementado um algoritmo criptográfico próprio.

Bytes aleatórios seguros podem ser gerados com `crypto.random_bytes[n]`; o tamanho máximo de uma chamada é 1 MiB.

Autenticação de mensagens usa `crypto.hmac_sha256[dados, chave]`, retornando um digest `Hash256`.

O interpretador também possui um modelo inicial de erro explícito com `Result`:

```mncyt
let resposta: Result = ok[42];
if (resposta.is_ok[]) {
    print[resposta.unwrap[]];
}
```

Comportamento implementado: prioriza `main.mncyt` na raiz; caso não exista, procura arquivos `.mncyt` com `init` no nome na raiz e em `src/`. A busca não é recursiva. Nenhum candidato gera erro, e múltiplos candidatos são listados como ambiguidade.

## 1. Objetivos

MoonCrypto será uma linguagem compilada para código nativo, com sintaxe familiar a quem conhece C++, mas com regras próprias para melhorar segurança e legibilidade.

Prioridades iniciais:

1. Portabilidade entre sistemas e arquiteturas populares.
2. Interoperabilidade com C e C++.
3. Segurança de memória com ownership verificado pelo compilador.
4. APIs de criptografia seguras por padrão.
5. Builds reproduzíveis por meio de versões fixadas de dependências.

## 2. Execução e compilação

O compilador, provisoriamente chamado `moon`, traduz arquivos `.mncyt` para código nativo. A proposta é usar LLVM como representação intermediária e deixar a vinculação final a cargo da toolchain da plataforma.

```text
.mncyt → análise e verificação → LLVM IR → código objeto → linker → executável
```

O artefato será um executável nativo do sistema de destino. O compilador deve permitir builds para x64 e ARM64, inicialmente nos sistemas operacionais escolhidos para a primeira versão.

Exemplo de comandos ```planejados```:

```text
moon build
moon run
moon test
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
    print["MoonCrypto"];
    return 0;
}
```

A memória seguirá ownership verificado pelo compilador. Cada valor tem um proprietário; empréstimos permitem acesso temporário sem transferir a posse. Referências mutáveis terão regras mais restritas. Operações que não possam ser verificadas pelo compilador, como certos usos de ponteiros, ficarão em blocos `unsafe` explícitos.

```cpp
fn checksum(data: &Bytes) -> Digest {
    return crypto.hash.sha256(data);
}

fn main[] -> Int32 {
    let data = Bytes.from("MoonCrypto");
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

MoonCrypto terá módulos próprios e suporte a dependências obtidas por Git. O lockfile deverá registrar revisões exatas para que outra máquina possa reconstruir as mesmas dependências.

```toml
[dependencies]
crypto-utils = {
    git = "https://example.com/crypto-utils",
    rev = "a1b2c3d"
}
```

Para C, o compilador poderá oferecer declarações de funções com ABI C. Para C++, a estratégia inicial será adaptadores escritos em C que exponham interfaces estáveis. A vinculação C++ direta poderá ser avaliada depois, considerando compilador, ABI, sistema operacional e arquitetura.

Isso aproveita bibliotecas populares de C e C++ sem exigir que MoonCrypto reproduza detalhes específicos de ABI C++.

**Vantagens:** acesso amplo a bibliotecas existentes e menor acoplamento à ABI de um compilador C++ específico.

**Limitações:** adaptadores podem exigir código adicional; bibliotecas nativas podem precisar de builds e configurações diferentes em cada plataforma.

**Decisões pendentes:** plataformas suportadas na primeira versão; formato do manifesto; comportamento de scripts de build de dependências; políticas de segurança e confiança para dependências Git.

## 6. Criptografia

A linguagem terá uma biblioteca criptográfica oficial. A orientação é reutilizar implementações consolidadas e oferecer APIs de alto nível como caminho padrão. Primitivas de baixo nível ficarão separadas e poderão gerar avisos do linter.

Exemplo ilustrativo:

```cpp
use crypto.aead;
use crypto.random;

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

- `moon`: compilador e interface de linha de comando.
- `moon pkg`: obtenção e fixação de dependências.
- `moon fmt`: formatador.
- `moon lint`: avisos sobre código inseguro, conversões e APIs criptográficas de baixo nível.
- `moon doc`: geração de documentação.
- `moon test`: execução de testes.

Essas ferramentas são propostas; seus comandos e formatos ainda não foram fechados.

## 8. Projeto e ponto de entrada

Estrutura sugerida:

```text
wallet/
├── moon.toml
├── src/
│   ├── main.mncyt
│   ├── init_crypto.mncyt
│   └── wallet.mncyt
├── tests/
└── moon.lock
```

Regra inicial de localização do ponto de entrada:

1. Procurar `main.mncyt` na raiz do projeto.
2. Se não existir, procurar arquivos `.mncyt` contendo `init` no nome, apenas na raiz e em `src/`.
3. Se houver exatamente um candidato, usá-lo.
4. Se houver vários, reportar ambiguidade e listar os caminhos.
5. Se não houver candidato, informar que falta um ponto de entrada.

A busca não será recursiva nesta versão. A sensibilidade a maiúsculas e minúsculas ainda precisa ser definida. Um campo `entry` no manifesto poderá permitir seleção explícita.

## 9. Decisões abertas para a próxima revisão

1. Quais sistemas operacionais devem fazer parte da primeira versão para x64 e ARM64?
2. A busca por `init` deve incluir nomes como `initialize.mncyt` e `InitCrypto.mncyt`?
3. Qual regra de maiúsculas e minúsculas deve ser usada nos nomes de arquivo?
4. Para erros, você prefere investigar primeiro `Result`, exceções ou um modelo combinado?
5. Quer definir agora um manifesto mínimo, incluindo formato e campo opcional `entry`?
6. Há uma biblioteca criptográfica consolidada que você prefira avaliar para a implementação oficial?
