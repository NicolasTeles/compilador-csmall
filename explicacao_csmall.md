# Compilador Csmall: explicação detalhada do código

Este documento explica, parte por parte, como funciona o arquivo `csmall.c`. A ideia é que você consiga entender a lógica e também explicar o trabalho para o professor.

---

## 1. Visão geral

O compilador lê um arquivo `.csmall` e **verifica** se o programa está correto. Ele **não executa** o programa: o `print(x);` é checado (sintaxe, variável declarada), mas nada é impresso na tela.

O processo passa por quatro etapas, que no código trabalham juntas:

```
 arquivo .csmall
        │
        ▼
 ┌───────────────┐   tokens   ┌───────────────┐
 │  LEXER        │ ─────────► │  PARSER       │
 │ (proximoToken)│            │ (descida      │
 └───────────────┘            │  recursiva)   │
                              └──────┬────────┘
                                     │ consulta / insere
                                     ▼
                              ┌───────────────┐
                              │ TABELA DE     │
                              │ SÍMBOLOS      │
                              │ (escopos)     │
                              └───────────────┘
                                     ▲
                                     │ regras de tipo e declaração
                              ANÁLISE SEMÂNTICA
                              (feita dentro das funções do parser)
```

Um ponto importante: **a análise semântica não é um passo separado**. Ela acontece *durante* o parsing. Quando o parser reconhece, por exemplo, uma atribuição, ele já consulta a tabela de símbolos e confere os tipos naquele momento. Isso se chama **tradução dirigida pela sintaxe**.

O parser **puxa** tokens do lexer sob demanda: sempre que precisa do próximo token, chama `proximoToken()`. O lexer não lê o arquivo inteiro antes.

---

## 2. Organização do arquivo

O arquivo está dividido nesta ordem (as seções são marcadas com comentários `/* ---- ... ---- */`):

| Seção | O que contém |
|---|---|
| Tokens | `enum TokenType`, `struct Token`, `nomeToken`, `imprimirToken` |
| Leitura do código-fonte | `CodigoFonte`, `lerCaractere`, `devolverCaractere`, `olharCaractere` |
| Mensagens de erro | `erroLexico`, `erroSintatico`, `erroSintaticoMsg`, `erroSemantico` |
| Lexer | `proximoToken` e funções auxiliares |
| Tabela de símbolos | `EntradaSimbolo`, `TabelaSimbolos`, criar/destruir escopo, inserir/buscar |
| Parser + semântica | `Parser`, `casar`, uma função por regra da gramática, `analisarPrograma` |
| Programa principal | `main`, tratamento das opções `-l` e `-t` |

> Em relação ao código base do professor, a tabela de símbolos foi movida para **antes** do parser, porque a struct `Parser` precisa guardar um ponteiro para o escopo atual.

---

## 3. Tokens

Um **token** é a menor unidade com significado do programa. O enum `TokenType` lista todos os tipos possíveis: palavras reservadas (`TOK_INT`, `TOK_IF`...), identificadores, constantes, operadores, delimitadores, `TOK_EOF` (fim da entrada) e `TOK_INVALID` (erro léxico).

```c
typedef struct {
    TokenType tipo;          // a categoria (ex.: TOK_PLUS)
    char lexema[TAM_LEXEMA]; // o texto original (ex.: "+")
    int linha;               // linha onde o token começa
} Token;
```

Exemplo: no trecho `x = 10;` na linha 3, o lexer produz:

| tipo | lexema | linha |
|---|---|---|
| `TOK_IDENTIFIER` | `x` | 3 |
| `TOK_ASSIGN` | `=` | 3 |
| `TOK_INT_CONST` | `10` | 3 |
| `TOK_SEMICOLON` | `;` | 3 |

---

## 4. Leitura do código-fonte

A struct `CodigoFonte` guarda o `FILE *` e o contador `linha`.

- **`lerCaractere`**: lê um caractere com `fgetc`. Se for `'\n'`, incrementa `linha`. Retorna `int` (e não `char`) porque `EOF` é um valor especial que não cabe em `char`.
- **`devolverCaractere`**: devolve o caractere ao arquivo com `ungetc`. Se era `'\n'`, decrementa `linha`, para o contador continuar correto.
- **`olharCaractere`**: espiar o próximo caractere sem consumi-lo (*lookahead*). Lê e devolve em seguida.

Por que isso é necessário? Para saber que um identificador terminou, o lexer precisa ler o caractere **seguinte**. Esse caractere já pertence ao próximo token e, por isso, tem que ser devolvido.

---

## 5. O Lexer (`proximoToken`)

Cada chamada devolve **um** token. Ela segue estes passos:

### 5.1 Ignorar espaços e comentários

```c
for (;;) {
    c = lerCaractere(codigo);
    if (c é espaço/tab/\n/\r...) continue;
    if (c == '/' && olharCaractere(codigo) == '/') {
        // comentário: consome até o fim da linha
        while (c != '\n' && c != EOF) c = lerCaractere(codigo);
        continue;
    }
    break;
}
```

Repare que o `/` sozinho é o operador de divisão (`TOK_DIV`); só vira comentário se o próximo caractere também for `/`. Por isso o uso de `olharCaractere`.

Depois desse laço, `c` é o **primeiro caractere do token** e `codigo->linha` já é a linha correta do token, que é guardada em `linha`.

### 5.2 Fim da entrada

Se `c == EOF`, retorna `TOK_EOF`.

### 5.3 Identificadores e palavras reservadas

Regra: `[A-Za-z_][A-Za-z0-9_]*`.

```c
if (isalpha(c) || c == '_') {
    while (isalnum(c) || c == '_') { guarda c; c = lerCaractere(); }
    devolverCaractere(codigo, c);   // o caractere que não pertence ao ID
    return criarToken(classificarIdentificador(buf), buf, linha);
}
```

Seguindo a especificação, a sequência é **primeiro** reconhecida como identificador e **depois** comparada com as palavras reservadas em `classificarIdentificador` (`int`, `float`, `if`, `else`, `while`, `print`). Se não for nenhuma, é `TOK_IDENTIFIER`.

### 5.4 Constantes numéricas

- `INT_CONST`: `[0-9]+`
- `FLOAT_CONST`: `[0-9]+\.[0-9]+`

O lexer lê os dígitos. Se o próximo caractere é `.`, ele espia o seguinte:

- se for dígito, continua lendo e retorna `TOK_FLOAT_CONST`;
- se não for, é um número mal formado (`3.` ou `3.x`) e retorna `TOK_INVALID`.
- se não houver ponto, devolve o caractere lido e retorna `TOK_INT_CONST`.

Como o ponto sozinho não pertence a nenhum token válido, não é preciso "desfazer" a leitura do `.`. Isso é bom, porque `ungetc` só garante devolver **um** caractere.

### 5.5 Operadores e delimitadores (correspondência máxima)

Um `switch` trata os símbolos. Os de um caractere (`+ - * / ( ) { } ;`) são retornados direto. Para `=`, `<`, `>` e `!`, o lexer espia o próximo caractere:

```c
case '<':
    if (olharCaractere(codigo) == '=') {
        lerCaractere(codigo);                  // consome o '='
        return criarToken(TOK_LE, "<=", linha);
    }
    return criarToken(TOK_LT, "<", linha);
```

Isso é a **correspondência máxima**: `<=` vira **um** token `LE`, e não `LT` seguido de `ASSIGN`. O `!` sozinho (sem `=`) é inválido, pois a linguagem não tem operador lógico.

### 5.6 Caractere inválido

Qualquer outro caractere (`@`, `#`, `$`...) cai no `default` e gera `TOK_INVALID`.

---

## 6. Tabela de símbolos

Guarda as variáveis declaradas. Cada símbolo é uma `EntradaSimbolo`:

```c
typedef struct EntradaSimbolo {
    char lexema[TAM_LEXEMA];   // nome da variável
    TipoDado tipo;             // TIPO_INT, TIPO_FLOAT ou TIPO_ERRO
    int linha;                 // linha da declaração
    int escopo;                // nível do escopo (0 = global)
    struct EntradaSimbolo *prox;
} EntradaSimbolo;
```

Cada escopo é uma `TabelaSimbolos`, que tem a lista de entradas e um ponteiro **`anterior`** para o escopo externo. Os escopos formam uma **pilha encadeada**:

```
escopo atual (nível 2) ──anterior──► escopo (nível 1) ──anterior──► global (nível 0) ──► NULL
```

### Funções

| Função | O que faz |
|---|---|
| `criarEscopo(anterior)` | Aloca um novo escopo. O nível é `anterior->nivel + 1` (ou 0 se for o global). |
| `destruirEscopo(tabela)` | Libera a memória do escopo e devolve o escopo externo. |
| `inserirSimbolo(...)` | Insere no escopo informado. Retorna **0** se o nome já existe **nesse mesmo escopo** (redeclaração). |
| `buscarSimboloLocal(...)` | Procura só no escopo informado. |
| `buscarSimbolo(...)` | Procura no escopo atual e, se não achar, **sobe** pelos escopos externos. |
| `imprimirEscopo(...)` | Mostra o conteúdo de um escopo (usado com a opção `-t`). |

### Por que são duas buscas?

- **Redeclaração** (regra 2) só olha o escopo atual → `buscarSimboloLocal`. Por isso `int x;` no global e `float x;` dentro de um bloco é permitido.
- **Uso de variável** (regras 1 e 4) olha do escopo atual para fora → `buscarSimbolo`. É isso que faz a variável interna **ocultar** a externa: a busca acha primeiro a mais interna.

---

## 7. O Parser (descida recursiva)

### 7.1 A ideia

Para cada regra (não terminal) da gramática existe **uma função** com o mesmo nome. O parser olha o token atual (`parser->tokenAtual`) e decide qual regra seguir. Como a gramática é LL(1), basta **um token de lookahead**.

### 7.2 Estrutura `Parser`

```c
typedef struct {
    CodigoFonte *codigo;
    Token tokenAtual;           // token que está sendo analisado
    TabelaSimbolos *escopoAtual;// topo da pilha de escopos
    int errosSemanticos;        // contador
    int mostrarTabela;          // opção -t
} Parser;
```

### 7.3 Funções básicas

- **`avancarEntrada`**: pede o próximo token ao lexer e guarda em `tokenAtual`. Se vier `TOK_INVALID`, reporta o erro léxico e interrompe a análise.
- **`casar(parser, esperado)`**: se o token atual for o esperado, consome (avança); senão, reporta erro sintático. É o equivalente a "o próximo símbolo **tem** que ser este".

### 7.4 Gramática → funções

| Regra da gramática | Função em C |
|---|---|
| `Programa → ListaDecl ListaCmd` | `analisarPrograma` |
| `ListaDecl → Declaracao ListaDecl \| ε` | `listaDecl` (laço `while`) |
| `Declaracao → Tipo ID ;` | `declaracao` |
| `ListaCmd → Comando ListaCmd \| ε` | `listaCmd` (laço `while`) |
| `Comando → ...` | `comando` (`switch` no token atual) |
| `Atribuicao → ID = Expressao ;` | `atribuicao` |
| `Print → print ( Expressao ) ;` | `comandoPrint` |
| `If → if ( Condicao ) Bloco OpcionalElse` | `comandoIf` |
| `While → while ( Condicao ) Bloco` | `comandoWhile` |
| `Bloco → { ListaDecl ListaCmd }` | `bloco` |
| `Condicao → Expressao RelOp Expressao` | `condicao` |
| `Expressao → Termo ExprLinha` | `expressao` |
| `ExprLinha → + Termo ExprLinha \| - ... \| ε` | `exprLinha` |
| `Termo → Fator TermoLinha` | `termo` |
| `TermoLinha → * Fator ... \| / Fator ... \| ε` | `termoLinha` |
| `Fator → ID \| INT_CONST \| FLOAT_CONST \| ( Expressao )` | `fator` |

### 7.5 Como a decisão é tomada (conjuntos "primeiro")

- **`listaDecl`** continua enquanto o token for `int` ou `float`.
- **`listaCmd`** continua enquanto o token for `ID`, `print`, `if`, `while` ou `{` (função `ehInicioDeComando`).
- **`comandoIf`** depois do bloco olha se o token é `else`; se for, consome e lê outro bloco. Se não, é o ε (vazio).
- **`exprLinha`** / **`termoLinha`**: se o token for `+`/`-` (ou `*`/`/`), continuam; senão, terminam (ε).

### 7.6 Por que declarações só no início?

Isso sai da própria gramática: `Programa → ListaDecl ListaCmd` e `Bloco → { ListaDecl ListaCmd }`. Primeiro vêm **todas** as declarações, depois os comandos. Se você escrever `x = 1; int y;`, `listaCmd` termina ao ver `int`, e o parser esperava o fim do arquivo (ou `}`), então dá erro sintático (`esperado EOF, encontrado INT`).

### 7.7 Expressões e a precedência

A gramática separa `Expressao` (`+ -`) de `Termo` (`* /`). Como `Termo` fica "dentro" de `Expressao`, a multiplicação e a divisão são resolvidas antes: `2 + 3 * 4` é `2 + (3*4)`. Os parênteses voltam a `Expressao` por meio de `Fator`.

As funções `exprLinha` e `termoLinha` recebem um parâmetro `esq`: o **tipo acumulado até agora** à esquerda. É assim que o tipo de `a + b + c` vai sendo calculado, da esquerda para a direita, sem precisar de recursão à esquerda na gramática.

---

## 8. Análise semântica (onde cada regra está no código)

A cada expressão, as funções retornam um `TipoDado` (`TIPO_INT`, `TIPO_FLOAT` ou `TIPO_ERRO`). O tipo sobe das folhas (`fator`) até o comando que usa a expressão.

| Regra do PDF | Onde é aplicada |
|---|---|
| **1.** Declarar antes de usar | `fator` (uso em expressão) e `atribuicao` (lado esquerdo): `buscarSimbolo` retornou `NULL` → erro "não declarada". |
| **2.** Sem redeclarar no mesmo escopo | `declaracao`: `inserirSimbolo` retornou 0 → erro, mostrando a linha da 1ª declaração. |
| **3.** Global + um escopo por bloco | `analisarPrograma` abre o global; `bloco` chama `entrarEscopo` ao ler `{` e `sairEscopo` ao ler `}`. |
| **4.** Ocultação de variável | `buscarSimbolo` começa no escopo atual e sobe. |
| **5.** Tipo das constantes e identificadores | `fator`: `INT_CONST` → int, `FLOAT_CONST` → float, `ID` → tipo da tabela. |
| **6.** Tipo do resultado dos operadores | `combinarTipos`: int com int → int; qualquer outro caso → float. |
| **7.** Atribuição sem conversão implícita | `atribuicao`: se o tipo da variável é diferente do tipo da expressão → erro. |
| **8.** Condição compara duas expressões | `condicao`: exige um operador relacional entre duas expressões. Sem ele (`if (y)`), é erro sintático. |
| **9.** `print` recebe expressão numérica | `comandoPrint`: como só existem `int` e `float`, toda expressão válida já é numérica. |

### O tipo `TIPO_ERRO` (evita erros em cascata)

Se uma variável não foi declarada, `fator` devolve `TIPO_ERRO`. Em `combinarTipos`, qualquer operando com `TIPO_ERRO` resulta em `TIPO_ERRO`, e `atribuicao` **não** compara tipos quando algum lado é erro. Assim, um único erro (variável não declarada) não gera uma sequência de mensagens extras.

### Por que `fator` guarda uma cópia do token?

```c
Token t = p->tokenAtual;
avancarEntrada(p);   // sobrescreve p->tokenAtual
... usa t.lexema e t.linha ...
```

`avancarEntrada` troca o `tokenAtual`. Para não perder o nome e a linha do identificador (necessários para a busca e para a mensagem de erro), o código guarda uma cópia antes de avançar.

---

## 9. Tratamento de erros

O compilador tem três categorias de erro, com tratamentos diferentes:

| Tipo | Quando ocorre | O que acontece |
|---|---|---|
| **Léxico** | Caractere inválido (`@`, `!` sozinho, `3.`) | Mensagem e **interrompe** a análise. |
| **Sintático** | Token inesperado (falta `;`, `if (y)`...) | Mensagem e **interrompe** a análise. |
| **Semântico** | Violação das regras da seção 5 do PDF | Mensagem, **conta** o erro e **continua**. |

### Como interrompe: `setjmp` / `longjmp`

Em `analisarPrograma`, a chamada `setjmp(saidaErro)` marca um ponto de retorno. Quando ocorre um erro léxico ou sintático em qualquer ponto de uma função aninhada, o código chama `longjmp(saidaErro, 1)` e a execução **volta** para o `setjmp`, que então:

1. libera os escopos que ficaram abertos (evita vazamento de memória);
2. retorna 0 (falha).

É uma forma simples de "abortar" no meio de várias chamadas recursivas, sem precisar checar um valor de retorno em cada função. Isso é feito para os erros léxicos e sintáticos porque, depois deles, não há como continuar de forma confiável.

Já os erros semânticos **não** interrompem: o programa continua sendo analisado e você vê todos os erros semânticos de uma vez.

### Funções de erro

- `erroLexico(linha, lexema)` → `Erro lexico na linha N: sequencia invalida '@'.`
- `erroSintatico(linha, esperado, encontrado)` → `Erro sintatico na linha N: esperado X, encontrado Y.`
- `erroSintaticoMsg(...)` → igual, mas o "esperado" é um texto (ex.: "operador relacional").
- `erroSemantico(linha, msg)` → `Erro semantico na linha N: ...`

Os erros vão para `stderr`.

---

## 10. A função `main`

1. Lê os argumentos: `-l`, `-t` e o nome do arquivo.
2. Abre o arquivo com `abrirCodigoFonte`.
3. **Modo `-l`**: só repete `proximoToken` e imprime cada token até `EOF` ou `INVALID`. Serve para testar o lexer isoladamente.
4. **Modo normal**: cria o `Parser`, chama `analisarPrograma` e, conforme o resultado, imprime:
   - `Compilacao concluida: nenhum erro encontrado.` (retorna sucesso), ou
   - `Compilacao falhou.` (retorna `EXIT_FAILURE`).
5. Com `-t`, cada escopo é impresso ao ser fechado (os internos primeiro, o global por último).

---

## 11. Exemplo passo a passo

Programa:

```c
int x;
x = 2 + 3;
{
    float x;
    x = 1.5;
}
```

**Lexer** (resumo dos tokens): `INT`, `ID(x)`, `;`, `ID(x)`, `=`, `INT_CONST(2)`, `+`, `INT_CONST(3)`, `;`, `{`, `FLOAT`, `ID(x)`, `;`, `ID(x)`, `=`, `FLOAT_CONST(1.5)`, `;`, `}`, `EOF`.

**Parser + semântica:**

1. `analisarPrograma` cria o escopo global (nível 0) e lê o primeiro token (`int`).
2. `listaDecl` vê `int` → `declaracao`: consome `int`, guarda `x`, casa `;`. Insere `x : int` no global.
3. O próximo token é `x` (ID), então `listaDecl` termina e `listaCmd` começa.
4. `comando` → `atribuicao`:
   - consome `x`, busca na tabela → achou, tipo `int`;
   - casa `=`;
   - `expressao` → `termo` → `fator`: `2` (int); `exprLinha` vê `+`, `termo` → `fator`: `3` (int); `combinarTipos(int, int)` = `int`;
   - casa `;`;
   - compara: variável `int` = expressão `int` ✔.
5. Próximo token `{` → `comando` → `bloco`: casa `{`, **abre escopo nível 1**.
6. Dentro: `listaDecl` → `float x;` insere `x : float` no nível 1. **Não é redeclaração**, pois é outro escopo.
7. `atribuicao`: `buscarSimbolo` acha primeiro o `x` do nível 1 (`float`); a expressão `1.5` é `float` ✔.
8. `casar(})` e **fecha o escopo nível 1** (o `x` float deixa de existir).
9. Fim: `casar(EOF)`, fecha o global. Resultado: nenhum erro.

---

## 12. Exemplos de erro e o que o código faz

| Programa | Mensagem |
|---|---|
| `x = 10;` | `Erro semantico na linha 1: variavel 'x' nao declarada.` |
| `int x; int x;` | `Erro semantico na linha 1: variavel 'x' ja foi declarada neste escopo (linha 1).` |
| `float y; y = 3;` | `Erro semantico ...: tipo incompativel na atribuicao: 'y' e float, mas a expressao e int.` |
| `int y; if (y) {...}` | `Erro sintatico ...: esperado operador relacional (...), encontrado RPAREN.` |
| `x = 3 @ 2;` | `Erro lexico na linha N: sequencia invalida '@'.` |
| `int x; x = 1; int y;` | `Erro sintatico ...: esperado EOF, encontrado INT.` |
| `while (x < 3) { x = x + 1;` (falta `}`) | `Erro sintatico ...: esperado RBRACE, encontrado EOF.` |

---

## 13. Limitações (para você saber responder)

- O compilador **só verifica**, não executa nem gera código (o `print` não imprime nada).
- Um erro léxico ou sintático **para** a análise; erros semânticos seguintes ao primeiro léxico/sintático não aparecem.
- Identificadores com mais de 127 caracteres são truncados silenciosamente (limite `TAM_LEXEMA`).
- A divisão por zero (`x / 0`) não é verificada, pois a especificação não pede.

---

## 14. Perguntas que o professor pode fazer

- **Por que `lerCaractere` retorna `int`?** Porque `EOF` é um valor negativo que não pode ser representado em um `char` sem ambiguidade.
- **O que é correspondência máxima?** Reconhecer o maior token possível: `<=` em vez de `<` e `=`.
- **Por que a gramática não tem recursão à esquerda?** Porque a descida recursiva entraria em recursão infinita. Por isso existem `ExprLinha` e `TermoLinha`.
- **Como funciona a ocultação de variáveis?** Pela busca da tabela: começa no escopo mais interno e sobe, e a primeira ocorrência encontrada vence.
- **Por que a ambiguidade do "else pendente" não ocorre?** Porque `if` e `while` exigem bloco `{ }`; cada `else` pertence sem dúvida ao `if` do bloco imediatamente anterior.
- **Por que dois tipos de busca na tabela?** Redeclaração olha só o escopo atual; uso olha todos os escopos.
