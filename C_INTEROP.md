# Interoperabilidade C da MonC

## Estado atual

A MonC já consegue declarar protótipos de funções C dentro de `#external` e chamá-los pelo runtime. Nesta etapa, as bibliotecas reconhecidas são:

- `math.h`: biblioteca matemática nativa (`libm` em sistemas POSIX ou runtime matemático do Windows).
- `stdlib.h`: biblioteca C padrão do sistema.

As chamadas C usam parênteses, como na linguagem C. As funções nativas da MonC continuam usando colchetes, por exemplo `print[resultado]`.

## Bibliotecas e funções disponíveis

### `math.h`

O runtime aceita funções com retorno `float` ou `double`, de zero a dois parâmetros do mesmo tipo declarado no retorno. As funções já usadas nos exemplos e testes são:

| Função | Protótipo C | Uso |
|---|---|---|
| `sqrt` | `double sqrt(double value)` | Raiz quadrada |
| `pow` | `double pow(double base, double exponent)` | Potência |
| `sin` | `double sin(double value)` | Seno |

A regra de assinatura também permite outros símbolos da biblioteca matemática que respeitem o mesmo formato e estejam disponíveis na biblioteca nativa do sistema. Isso não significa que todo o conteúdo de `math.h` esteja implementado: macros, tipos especiais, parâmetros mistos e outras assinaturas ainda não são aceitos.

### `stdlib.h`

| Função | Protótipo C | Estado |
|---|---|---|
| `abs` | `int abs(int value)` | Implementada |
| `atoi` | `int atoi(const char* text)` | Implementada |
| `rand` | `int rand(void)` | Implementada |
| `srand` | `void srand(unsigned int seed)` | Implementada |
| `malloc` | `void* malloc(size_t size)` | Implementada |
| `free` | `void free(void* ptr)` | Implementada |
| `NULL` | constante/valor de ponteiro nulo da MonC | Implementado |

`atof`, `atol` e `atoll` ainda não estão implementadas nesta versão documentada.

### Exemplo completo

```monc
#external "math.h" {
    playCodeC {
        double sqrt(double value);
        double pow(double base, double exponent);
        double sin(double value);
    }
}

#external "stdlib.h" {
    playCodeC {
        int abs(int value);
        int atoi(const char* text);
        int rand(void);
        void srand(unsigned int seed);
        void* malloc(size_t size);
        void free(void* ptr);
    }
}

fn main[] -> Int32 {
    let raiz: Float64 = sqrt(25.0);
    let potencia: Float64 = pow(2.0, 5.0);
    let seno: Float64 = sin(0.0);
    let absoluto: Int32 = abs(-17);
    let numero: Int32 = atoi("123");

    srand(123);
    let aleatorio: Int32 = rand();

    let memoria = malloc(64);
    free(memoria);
    free(NULL);

    print["sqrt=" + raiz];
    print["pow=" + potencia];
    print["sin=" + seno];
    print["abs=" + absoluto];
    print["atoi=" + numero];
    print["rand=" + aleatorio];
    print["malloc/free/NULL funcionando"];

    return 0;
}
```

## Ponteiros e memória

A primeira integração de ponteiros suporta o retorno de `void* malloc(size_t)`, a passagem de `void*` para `free` e o valor `NULL`. O runtime representa esses valores como ponteiros.

Limites importantes:

- A MonC ainda não oferece um sistema geral de ponteiros C.
- Não há suporte geral para desreferenciar ponteiros, fazer aritmética de ponteiros ou converter livremente entre tipos de ponteiro.
- `free(NULL)` é aceito e segue a semântica definida pela biblioteca C.
- Depois de `free(ptr)`, o programa não deve reutilizar o ponteiro. A linguagem ainda não verifica automaticamente use-after-free ou double-free.
- `malloc` recebe um inteiro não negativo compatível com `size_t`; falha de alocação é reportada pelo runtime.

## Como funciona

O parser registra os protótipos declarados em `playCodeC`. Em tempo de execução, a MonC procura os símbolos na biblioteca nativa do sistema:

- Windows: carrega o runtime C do Windows (`ucrtbase.dll`, com fallback para `msvcrt.dll`) e resolve os símbolos.
- Sistemas POSIX suportados pelo runtime: usa `dlopen` e `dlsym` para localizar `libm` ou `libc`.

A disponibilidade de cada símbolo depende da biblioteca e da ABI fornecidas pelo sistema operacional. A implementação não incorpora uma cópia própria dessas bibliotecas.

## Limitações atuais

Ainda não há suporte geral a:

- `atof`, `atol`, `atoll`;
- bibliotecas arbitrárias indicadas pelo usuário;
- macros, `typedef`, structs C e unions;
- funções variádicas, callbacks e ponteiros de função;
- parâmetros C mistos ou assinaturas não listadas nesta documentação;
- conversões gerais entre strings, buffers e ponteiros C;
- desreferência e aritmética geral de ponteiros.

A interoperabilidade está em evolução. Consulte esta tabela e os testes do repositório antes de considerar uma assinatura C como suportada.
