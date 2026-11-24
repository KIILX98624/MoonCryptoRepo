# Guia para iniciantes da MonC

> Este guia ensina a escrever programas MonC passo a passo. Ele descreve o protótipo interpretado disponível nesta branch, não todos os recursos planejados para a linguagem final.

[Voltar à documentação](README.md) · [Guia técnico avançado](GUIA_AVANCADO.md) · [Especificação da linguagem](MOONLANGUAGE.md)

## 1. O que é a MonC?

MonC é uma linguagem experimental cuja sintaxe usa funções tipadas, variáveis, módulos e chamadas com colchetes. O interpretador/runtime atual foi desenvolvido em C++20.

**Importante:** a MonC ainda não gera executáveis nativos a partir dos arquivos `.monc`. O programa é interpretado pelo runtime. Algumas funcionalidades descritas nos documentos de planejamento ainda não existem.

## 2. Seu primeiro programa

Crie um arquivo chamado `main.monc`:

```monc
fn main[] -> Int32 {
    print["Olá, MonC!"];
    return 0;
}
```

Entendendo cada parte:

- `fn` inicia a declaração de uma função.
- `main` é o nome da função principal, usada como ponto de entrada.
- `[]` indica a lista de parâmetros. Ela está vazia porque `main` não recebe argumentos.
- `-> Int32` declara o tipo de retorno da função.
- `{` e `}` delimitam o corpo da função.
- `print[...]` exibe um valor no terminal.
- `return 0;` devolve o código de saída do programa.

Execute o arquivo com:

```powershell
monc run caminho\para\main.monc
```

As chamadas de funções MonC usam colchetes. Não confunda com as chamadas de funções C, que usam parênteses dentro de `playCodeC`.

## 3. Variáveis e tipos

Uma variável é um espaço nomeado para guardar um valor. Use `let`:

```monc
let idade: Int32 = 15;
let nome: String32 = "Gustavo";
let ativo: Bool = true;
```

A estrutura geral é:

```monc
let nome_da_variavel: Tipo = valor;
```

A anotação de tipo pode ser omitida quando o interpretador consegue inferir o tipo:

```monc
let pontos = 100;
let mensagem = "Tudo certo";
```

Use `const` para declarar uma constante tipada:

```monc
const LIMITE: Int32 = 100;
```

Uma variável declarada com `let` pode receber outro valor compatível:

```monc
let pontos: Int32 = 10;
pontos = 25;
```

O tipo de uma variável tipada deve continuar compatível. Por exemplo, não atribua uma string a uma variável `Int32`.

## 4. Tipos disponíveis

Os nomes abaixo são os nomes usados nas anotações de tipo da MonC. Alguns grupos têm recursos em diferentes níveis de implementação.

### 4.1 Inteiros

Inteiros representam números sem parte decimal.

| Tipo | Uso geral |
|---|---|
| `Int8`, `Int16`, `Int32`, `Int64` | Inteiros com sinal e largura indicada pelo nome |
| `UInt8`, `UInt32`, `UInt64` | Inteiros sem sinal |
| `Byte` | Valor inteiro de byte |

Exemplo:

```monc
let quantidade: Int32 = 42;
let codigo: UInt8 = 255;
print[quantidade];
print[codigo];
```

Os limites numéricos são verificados em declarações tipadas. Consulte a especificação para detalhes sobre a largura de cada tipo.

### 4.2 Números de ponto flutuante

Use tipos de ponto flutuante para valores com casas decimais ou notação científica.

| Tipo | Precisão armazenada no runtime |
|---|---|
| `Float32` | binary32 |
| `Float64` | binary64 |
| `Double32` | binary32 |
| `Double64` | binary64 |

Exemplo:

```monc
let temperatura: Float32 = 23.5;
let distancia: Float64 = 1250.75;
let pi: Double64 = 3.1415926535;

print[temperatura];
print[distancia];
print[pi];
```

Também é possível usar notação científica:

```monc
let velocidade: Double64 = 3.0e8;
```

**Atenção:** `Float32` e `Double32` são armazenados com precisão binary32. `Float64` e `Double64` usam binary64. Os tipos `Float` e `Double` sem sufixo não são os nomes principais documentados para declarações nesta versão.

### 4.3 Strings

Strings guardam texto UTF-8.

```monc
let nome: String32 = "MonC";
let descricao: String64 = "Uma linguagem em desenvolvimento";
print[nome];
print[descricao];
```

Você pode concatenar textos com `+`:

```monc
let saudacao: String32 = "Olá, " + nome + "!";
print[saudacao];
```

O operador `+` também permite concatenar texto com valores como inteiros e caracteres, convertendo esses valores para texto nesse contexto.

### 4.4 Caracteres

`Char` representa um caractere:

```monc
let inicial: Char = 'M';
print[inicial];
```

Strings usam aspas duplas; caracteres usam aspas simples.

### 4.5 Booleanos

`Bool` representa verdadeiro ou falso:

```monc
let tem_acesso: Bool = true;
let esta_pronto: Bool = false;
print[tem_acesso];
```

Condições aceitam expressões booleanas e comparações. Não dependa de conversão implícita de inteiros para booleanos.

```monc
if (tem_acesso == true) {
    print["Acesso permitido"];
} else {
    print["Acesso negado"];
}
```

### 4.6 Outros tipos

- `Bytes`: sequência de bytes, útil para dados binários.
- `ByteArray`: sequência mutável de bytes.
- `Array<T>`: array com elementos de um tipo indicado.
- `JsonValue`: valor JSON.
- `Result`: resultado que pode representar sucesso ou erro.
- Ponteiros: suporte inicial e restrito à interoperabilidade com C.

Consulte [MOONLANGUAGE.md](MOONLANGUAGE.md) para o estado exato de cada recurso.

## 5. Como criar funções

Uma função é um bloco de código com nome. Ela pode receber parâmetros e devolver um resultado.

Formato geral:

```monc
fn nome_da_funcao[parametro: Tipo] -> TipoDeRetorno {
    // instruções
    return valor;
}
```

Uma função pode receber nenhum, um ou vários parâmetros.

### 5.1 Função que recebe e retorna Int32

```monc
fn somar[a: Int32, b: Int32] -> Int32 {
    return a + b;
}

fn main[] -> Int32 {
    let resultado: Int32 = somar[10, 20];
    print[resultado];
    return 0;
}
```

Aqui, `a` e `b` são parâmetros. A chamada `somar[10, 20]` fornece os argumentos. O `return` devolve o resultado da soma.

### 5.2 Função que recebe e retorna Float32

```monc
fn dobrar[valor: Float32] -> Float32 {
    return valor * 2.0;
}

fn main[] -> Int32 {
    let resultado: Float32 = dobrar[2.5];
    print[resultado];
    return 0;
}
```

### 5.3 Função que recebe e retorna Float64 ou Double64

```monc
fn calcular_media[a: Double64, b: Double64] -> Double64 {
    return (a + b) / 2.0;
}

fn main[] -> Int32 {
    let media: Double64 = calcular_media[8.5, 9.5];
    print[media];
    return 0;
}
```

### 5.4 Função que recebe e retorna String

```monc
fn criar_mensagem[nome: String32] -> String32 {
    return "Olá, " + nome + "!";
}

fn main[] -> Int32 {
    let mensagem: String32 = criar_mensagem["MonC"];
    print[mensagem];
    return 0;
}
```

### 5.5 Função que recebe e retorna Bool

```monc
fn esta_aprovado[nota: Int32] -> Bool {
    return nota >= 60;
}

fn main[] -> Int32 {
    let aprovado: Bool = esta_aprovado[75];
    print[aprovado];
    return 0;
}
```

### 5.6 Função que recebe e retorna Char

```monc
fn primeira_letra[letra: Char] -> Char {
    return letra;
}

fn main[] -> Int32 {
    let inicial: Char = primeira_letra['M'];
    print[inicial];
    return 0;
}
```

### 5.7 Função com parâmetros de tipos diferentes

Uma mesma função pode receber tipos diferentes:

```monc
fn descrever[nome: String32, idade: Int32, ativo: Bool] -> String32 {
    return nome + " | idade: " + idade + " | ativo: " + ativo;
}

fn main[] -> Int32 {
    let texto: String32 = descrever["Ana", 20, true];
    print[texto];
    return 0;
}
```

### 5.8 Função sem retorno explícito

Quando não há `-> Tipo`, o parser registra o retorno como `Void`. Esse formato é útil para uma função que apenas executa uma ação:

```monc
fn mostrar_aviso[] {
    print["Atenção!"];
}

fn main[] -> Int32 {
    mostrar_aviso[];
    return 0;
}
```

No runtime atual, uma função que chega ao fim sem executar `return` devolve um valor inteiro zero internamente. O comportamento de retorno `Void` ainda está em evolução; prefira não depender desse detalhe para lógica importante.

### 5.9 Observação importante sobre a verificação de tipos em funções

As anotações de parâmetros e de retorno fazem parte da declaração da função. Porém, o interpretador atual ainda não realiza uma verificação estática completa das assinaturas de funções: ele associa os argumentos aos parâmetros e registra os tipos declarados, mas não valida rigorosamente todos os argumentos e todos os valores de `return` contra a assinatura.

Já as declarações e reatribuições de variáveis tipadas, como `let valor: Int32 = ...`, passam por verificação de compatibilidade no runtime.

Por isso, escreva argumentos e retornos com os tipos indicados e teste os exemplos no interpretador. A verificação completa de assinaturas é uma melhoria futura, não uma garantia desta versão.

## 6. Condições

Use `if` e `else` para escolher qual bloco executar:

```monc
let temperatura: Int32 = 30;

if (temperatura > 25) {
    print["Está quente"];
} else {
    print["Não está tão quente"];
}
```

Operadores de comparação:

| Operador | Significado |
|---|---|
| `==` | Igual |
| `!=` | Diferente |
| `>` | Maior |
| `<` | Menor |
| `>=` | Maior ou igual |
| `<=` | Menor ou igual |

Operadores lógicos documentados: `&&` (E), `||` (OU) e `!` (negação).

## 7. Repetições

O protótipo inclui estruturas de repetição como `while` e `for`. Consulte os exemplos em `Code MonC/Examples/others` e a especificação para a sintaxe coberta pela versão atual.

Exemplo de `while`:

```monc
fn main[] -> Int32 {
    let contador: Int32 = 0;

    while [contador < 3] {
        print[contador];
        contador = contador + 1;
    }

    return 0;
}
```

## 8. Arrays

Arrays guardam vários valores em sequência. Índices começam em zero:

```monc
fn main[] -> Int32 {
    let numeros: Array<Int32> = Array[10, 20, 30];
    print[numeros[0]];
    print[array.length[numeros]];
    array.push[numeros, 40];
    return 0;
}
```

A API inicial inclui `array.length`, `array.push`, `array.pop`, `array.get`, `array.set`, `array.remove` e `array.clear`.

## 9. Módulos e código em outros arquivos

Um arquivo pode declarar funções que serão usadas por outro arquivo. A diretiva `#external` carrega o módulo:

```monc
#external "math"

fn main[] -> Int32 {
    let resultado: Int32 = somar[4, 5];
    print[resultado];
    return 0;
}
```

Consulte os exemplos em `Code MonC/Examples/functions` e `Code MonC/Examples/modules` para ver a organização completa dos arquivos.

## 10. Chamadas MonC e chamadas C

Funções MonC são chamadas com colchetes:

```monc
somar[2, 3];
print["Olá"];
```

As funções C declaradas dentro de `playCodeC` usam parênteses, pois seguem a sintaxe C:

```monc
#external "math.h" {
    playCodeC {
        double sqrt(double value);
    }
}

fn main[] -> Int32 {
    let raiz: Float64 = sqrt(25.0);
    print[raiz];
    return 0;
}
```

A integração C é limitada às bibliotecas e assinaturas documentadas em [C_INTEROP.md](C_INTEROP.md).

## 11. Próximos passos

- [Especificação da linguagem](MOONLANGUAGE.md): regras e tipos em mais detalhes.
- [Guia técnico avançado](GUIA_AVANCADO.md): arquitetura e detalhes do runtime.
- [Interoperabilidade com C](C_INTEROP.md): assinaturas e limitações.
- [Exemplos](../Code%20MonC/Examples): programas práticos da MonC.

Se você está começando, experimente alterar os exemplos, mudar os argumentos das funções e criar novas funções com os tipos mostrados neste guia.
