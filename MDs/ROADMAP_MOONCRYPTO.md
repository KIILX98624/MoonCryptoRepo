# Roteiro de evolução da MonC

Este documento reúne as decisões de design, recursos de criptografia, fundamentos da linguagem e correções de documentação identificados na revisão. Ele serve como checklist para planejar a evolução da MonC. Os itens descrevem lacunas e decisões a tomar; não significam que tudo já esteja implementado.

## 1. Decisões de design que ficam caras depois

### 1.1 Chamadas de função e indexação

Atualmente funções definidas pelo usuário são chamadas com colchetes, por exemplo `somar[10, 20]`. Quando arrays forem adicionados, a indexação normalmente também usará colchetes, como `lista[0]`, criando ambiguidade.

Definir antes de introduzir arrays:

- se chamadas de função continuarão usando colchetes ou passarão a usar parênteses;
- qual sintaxe será usada para indexar arrays;
- como o parser distingue uma chamada de uma indexação, caso ambas usem colchetes.

### 1.2 Sintaxe uniforme de chamadas

A linguagem atualmente mistura chamadas: funções próprias usam colchetes (`somar[1, 2]`), enquanto `print(...)`, `crypto.sha256(...)` e métodos como `h.hex()` usam parênteses.

Decidir se isso é uma regra intencional — por exemplo, uma distinção entre funções do usuário, builtins e métodos — ou se todas as chamadas devem adotar a mesma sintaxe. Documentar a regra escolhida.

### 1.3 Formas de importar módulos

Há duas formas distintas de importar:

- `use crypto`, com acesso qualificado, como `crypto.sha256(...)`;
- `#external "matematica"`, que torna funções acessíveis sem prefixo, como `somar[...]`.

Imports sem prefixo podem causar colisões quando módulos diferentes exportam o mesmo nome. Definir regras de namespace, resolução de nomes, aliases e tratamento de colisões.

### 1.4 Erros de operações criptográficas

`aes_decrypt` pode falhar, por exemplo, se a tag de autenticação não for válida. Um exemplo que retorna o texto diretamente não mostra como o chamador detecta essa falha.

Definir um modelo de erros antes de estabilizar a API criptográfica. Opções incluem um tipo `Result`, exceções ou outro mecanismo explícito. A falha de autenticação nunca deve ser confundida com texto vazio ou dados válidos.

### 1.5 Overflow de inteiros

Especificar o comportamento quando uma operação excede a faixa do tipo inteiro: erro, saturação ou wraparound (volta ao início da faixa). A criptografia frequentemente depende de aritmética modular/wraparound, então essa semântica precisa ser explícita, previsível e consistente entre plataformas e modos de compilação.

## 2. Recursos necessários para uma linguagem de criptografia

### 2.1 Bytes, arrays e codificação

A criptografia trabalha com bytes, mas o modelo descrito atualmente se concentra em strings. Definir e implementar tipos apropriados, como:

- `Byte` ou `UInt8`;
- inteiros sem sinal de largura fixa, como `UInt32` e `UInt64`;
- `Bytes` e arrays, incluindo criação, tamanho, indexação e passagem a funções.

Esclarecer o significado de `String32` e `String64`: 32/64 bytes, caracteres, capacidade ou outra medida. Especificar a codificação do texto quando convertido em bytes. Para interoperabilidade criptográfica, definir UTF-8 explicitamente antes de aplicar hash, cifragem ou assinatura.

### 2.2 Operadores e booleanos

Além de `+` e comparações, definir os operadores necessários para programação geral e criptográfica:

- aritméticos: `-`, `*`, `/`, `%`;
- lógicos: `&&`, `||`, `!`;
- bitwise: AND, OR, XOR, NOT, shifts e rotações;
- tipo booleano explícito `Bool`, incluindo valores, conversões e regras de avaliação.

A precedência e associatividade de todos os operadores devem ser documentadas.

### 2.3 Primitivas criptográficas

Além de hash e AEAD, avaliar e especificar uma API para:

- assinaturas digitais e pares de chaves;
- HMAC;
- derivação de chaves (KDF), incluindo derivação de senha;
- geração criptograficamente segura de bytes aleatórios (`random_bytes`);
- comparação em tempo constante para segredos e tags;
- conversão de/para hexadecimal (`from_hex`, `hex`) e Base64.

A API deve deixar claros os tipos de entrada e saída, erros possíveis, requisitos de nonce/IV, parâmetros e limites de cada primitiva.

### 2.4 Derivação de chave e nonces

Uma operação como `key_from("senha", 32)` não é segura se apenas transforma a senha diretamente em 32 bytes. Derivar uma chave de senha requer salt e parâmetros de custo, com uma KDF adequada, por exemplo Argon2id, scrypt ou PBKDF2 (conforme a política e disponibilidade da implementação).

A API deve gerar nonces de forma segura e impedir reutilização quando isso for requisito do algoritmo. Para AES-GCM, nonce não deve ser escolhido de forma descuidada pelo usuário. Definir como a API gera, guarda e transporta o nonce junto ao texto cifrado, e como lida com unicidade e persistência.

### 2.5 Tipos de segredo e memória

Criar tipos opacos para segredos, de modo que não possam ser impressos acidentalmente nem comparados com `==` comum. Considerar limpeza da memória quando deixam de ser usados e regras de ownership que controlem cópias e tempo de vida.

Especificar as limitações reais: runtimes, otimizações e cópias internas podem dificultar garantir apagamento completo. A API deve reduzir exposição de segredo e tornar as operações permitidas explícitas.

### 2.6 Backend criptográfico portátil

O SHA-256 existente usa BCrypt e, portanto, funciona no Windows. Definir um backend portátil para Windows, Linux e macOS. Uma biblioteca consolidada, como libsodium ou OpenSSL, pode fornecer operações comuns (por exemplo SHA-2, AES-GCM, ChaCha20-Poly1305 e Ed25519, conforme o backend escolhido) sem implementar algoritmos criptográficos próprios.

Escolher e documentar dependências, versões, licenças, disponibilidade por plataforma e estratégia de fallback. Manter vetores de teste conhecidos para validar resultados entre plataformas.

## 3. Fundamentos da linguagem que faltam ou precisam de regra

Implementar ou especificar:

- `else if`;
- funções sem valor de retorno;
- entrada de dados por stdin, arquivos e argumentos de linha de comando;
- conversões explícitas de tipos e regras de concatenação. Por exemplo, definir se `"Idade: " + idade` é permitido e como um número vira texto, evitando conversões implícitas surpreendentes;
- regras de `switch`: fallthrough, tipos aceitos (incluindo strings), valores duplicados e necessidade de cobertura/default;
- semântica de `const`: se é verificado em compilação ou execução e quais expressões são válidas;
- o que `main` pode retornar e como o valor vira código de saída do processo.

## 4. Gramática formal

Escrever uma gramática formal, por exemplo em EBNF, para registrar a sintaxe da linguagem. Incluir declarações, funções, parâmetros, tipos, expressões, precedência de operadores, chamadas, indexação, imports, controle de fluxo, comentários e literais.

A gramática deve corresponder ao parser real e servir como referência para o interpretador, futuro compilador, editor e documentação.

## 5. Correções e esclarecimentos na documentação

Revisar o documento principal e exemplos para corrigir estes pontos:

- mover o parágrafo “Tipos atualmente usados pelo interpretador” para a seção de variáveis e tipos, em vez de deixá-lo em **Ownership**;
- reconciliar a afirmação de que criptografia ainda não está ligada ao interpretador com o fato de que SHA-256 já está integrado;
- esclarecer que `.hex()` aparece no exemplo SHA-256 apesar de também ser listado como planejado, e indicar com precisão o que está implementado;
- remover ou atualizar `const` da lista de próximos recursos, pois já está implementado;
- explicar a diferença entre `monc run arquivo.monc` e `monc run pasta`, incluindo quando cada comando deve ser usado;
- documentar como compilar e executar em Linux e macOS, além dos exemplos existentes em PowerShell;
- esclarecer o que significa “Editor / interpretador” no estado do projeto: suporte do editor, parser, interpretador, compilador ou combinação deles.

Manter uma tabela de estado (planejado, parcial, implementado, por plataforma) para que exemplos e promessas não fiquem desatualizados.

## 6. Ordem sugerida de trabalho

1. Registrar as decisões semânticas centrais: chamadas/indexação, imports, erros, overflow, strings/bytes e conversões.
2. Escrever e manter a gramática formal alinhada com o parser.
3. Definir os tipos e operações fundamentais (`Bool`, bytes, arrays e operadores).
4. Definir o modelo de erros e os tipos opacos/ownership para segredos.
5. Escolher e integrar um backend criptográfico portátil e especificar a API segura de alto nível.
6. Completar recursos básicos da linguagem e a interface de entrada/saída.
7. Atualizar documentação, exemplos e instruções de plataforma conforme cada recurso muda de estado.

Esta ordem é uma sugestão de dependências: as decisões de sintaxe e semântica devem preceder recursos que dependam delas, como arrays e APIs criptográficas estáveis.


## Parte 1: built-ins e ByteArray

**Em implementação na branch `feature/part-1-python-builtins-bytearray`.**

Já adicionados ao runtime nesta branch:
- Tipo mutável `ByteArray`, construtores por tamanho e por `Array`.
- Indexação, atribuição de um ou vários índices em ordem crescente e propriedade `length`.
- Métodos `append`, `pop`, `vpop`, `insert`, `clear`, `copy`, `reverse`, `contains`, `index`, `count`, `extend`, `slice` e `fill`.
- Built-ins `all`, `any`, `ascii`, `bin`, `bool` e o placeholder `breakpoint`.
- Fixture de integração e teste CTest para esse subconjunto.

**Ainda pendente nesta Parte 1:** as demais built-ins da lista de comparação, como conversores genéricos, funções de iteração, introspecção e utilitários. Elas não devem ser consideradas implementadas só porque a primeira leva foi adicionada. A suíte CTest passou no GitHub Actions em Windows e Linux no commit `b2d4c275`. As demais built-ins da lista comparativa continuam pendentes.
