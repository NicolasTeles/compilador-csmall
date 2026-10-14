#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <setjmp.h>

#define TAM_LEXEMA 128

/* -------------------- Tokens da Csmall -------------------- */

typedef enum {
    TOK_INT, TOK_FLOAT, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_PRINT,
    TOK_IDENTIFIER, TOK_INT_CONST, TOK_FLOAT_CONST,
    TOK_PLUS, TOK_MINUS, TOK_MULT, TOK_DIV,
    TOK_ASSIGN, TOK_LT, TOK_LE, TOK_GT, TOK_GE, TOK_EQ, TOK_NE,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_SEMICOLON,
    TOK_EOF, TOK_INVALID
} TokenType;

typedef struct {
    TokenType tipo;
    char lexema[TAM_LEXEMA];
    int linha;
} Token;

const char *nomeToken(TokenType tipo)
{
    switch (tipo) {
        case TOK_INT:         return "INT";
        case TOK_FLOAT:       return "FLOAT";
        case TOK_IF:          return "IF";
        case TOK_ELSE:        return "ELSE";
        case TOK_WHILE:       return "WHILE";
        case TOK_PRINT:       return "PRINT";
        case TOK_IDENTIFIER:  return "IDENTIFIER";
        case TOK_INT_CONST:   return "INT_CONST";
        case TOK_FLOAT_CONST: return "FLOAT_CONST";
        case TOK_PLUS:        return "PLUS";
        case TOK_MINUS:       return "MINUS";
        case TOK_MULT:        return "MULT";
        case TOK_DIV:         return "DIV";
        case TOK_ASSIGN:      return "ASSIGN";
        case TOK_LT:          return "LT";
        case TOK_LE:          return "LE";
        case TOK_GT:          return "GT";
        case TOK_GE:          return "GE";
        case TOK_EQ:          return "EQ";
        case TOK_NE:          return "NE";
        case TOK_LPAREN:      return "LPAREN";
        case TOK_RPAREN:      return "RPAREN";
        case TOK_LBRACE:      return "LBRACE";
        case TOK_RBRACE:      return "RBRACE";
        case TOK_SEMICOLON:   return "SEMICOLON";
        case TOK_EOF:         return "EOF";
        case TOK_INVALID:     return "INVALID";
        default:              return "UNKNOWN";
    }
}

void imprimirToken(Token token)
{
    printf("%-14s %-20s linha %d\n",
           nomeToken(token.tipo), token.lexema, token.linha);
}

/* -------------------- Leitura do codigo-fonte -------------------- */

typedef struct {
    FILE *arquivo;
    int linha;
} CodigoFonte;

int abrirCodigoFonte(CodigoFonte *codigo, const char *nomeArquivo)
{
    codigo->arquivo = fopen(nomeArquivo, "r");
    codigo->linha = 1;

    if (codigo->arquivo == NULL) {
        fprintf(stderr, "Nao foi possivel abrir o arquivo: %s\n", nomeArquivo);
        return 0;
    }

    return 1;
}

void fecharCodigoFonte(CodigoFonte *codigo)
{
    if (codigo->arquivo != NULL) {
        fclose(codigo->arquivo);
        codigo->arquivo = NULL;
    }
}

/* Retornamos int porque fgetc() precisa poder retornar EOF. */
int lerCaractere(CodigoFonte *codigo)
{
    int caractere = fgetc(codigo->arquivo);

    if (caractere == '\n') {
        codigo->linha++;
    }

    return caractere;
}

void devolverCaractere(CodigoFonte *codigo, int caractere)
{
    if (caractere == EOF) {
        return;
    }

    ungetc(caractere, codigo->arquivo);

    if (caractere == '\n') {
        codigo->linha--;
    }
}

/* Le um caractere para consulta e depois o devolve ao arquivo (lookahead). */
int olharCaractere(CodigoFonte *codigo)
{
    int caractere = lerCaractere(codigo);
    devolverCaractere(codigo, caractere);
    return caractere;
}

/* -------------------- Mensagens de erro -------------------- */

void erroLexico(int linha, const char *lexema)
{
    fprintf(stderr, "Erro lexico na linha %d: sequencia invalida '%s'.\n",
            linha, lexema);
}

void erroSintatico(int linha, TokenType esperado, TokenType encontrado)
{
    fprintf(stderr, "Erro sintatico na linha %d: esperado %s, encontrado %s.\n",
            linha, nomeToken(esperado), nomeToken(encontrado));
}

/* Variante para quando o "esperado" nao e um unico token (ex.: operador relacional). */
void erroSintaticoMsg(int linha, const char *esperado, TokenType encontrado)
{
    fprintf(stderr, "Erro sintatico na linha %d: esperado %s, encontrado %s.\n",
            linha, esperado, nomeToken(encontrado));
}

void erroSemantico(int linha, const char *mensagem)
{
    fprintf(stderr, "Erro semantico na linha %d: %s\n", linha, mensagem);
}

/* -------------------- Lexer -------------------- */

static Token criarToken(TokenType tipo, const char *lexema, int linha)
{
    Token token;
    token.tipo = tipo;
    strncpy(token.lexema, lexema, TAM_LEXEMA - 1);
    token.lexema[TAM_LEXEMA - 1] = '\0';
    token.linha = linha;
    return token;
}

/* Compara o lexema de um identificador com as palavras reservadas. */
static TokenType classificarIdentificador(const char *lexema)
{
    if (strcmp(lexema, "int") == 0)   return TOK_INT;
    if (strcmp(lexema, "float") == 0) return TOK_FLOAT;
    if (strcmp(lexema, "if") == 0)    return TOK_IF;
    if (strcmp(lexema, "else") == 0)  return TOK_ELSE;
    if (strcmp(lexema, "while") == 0) return TOK_WHILE;
    if (strcmp(lexema, "print") == 0) return TOK_PRINT;
    return TOK_IDENTIFIER;
}

static void anexarChar(char *buffer, int *n, int c)
{
    if (*n < TAM_LEXEMA - 1) {
        buffer[(*n)++] = (char)c;
    }
}

Token proximoToken(CodigoFonte *codigo)
{
    char buf[TAM_LEXEMA];
    int n = 0;
    int c;

    /* 1) Ignora espacos, tabulacoes, quebras de linha e comentarios (//...). */
    for (;;) {
        c = lerCaractere(codigo);

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '\f' || c == '\v') {
            continue;
        }

        if (c == '/' && olharCaractere(codigo) == '/') {
            while (c != '\n' && c != EOF) {
                c = lerCaractere(codigo);
            }
            continue;
        }

        break;
    }

    /* Linha onde o token comeca (c nao e '\n', entao o contador esta correto). */
    int linha = codigo->linha;

    /* 2) Fim da entrada. */
    if (c == EOF) {
        return criarToken(TOK_EOF, "", linha);
    }

    /* 3) Identificadores e palavras reservadas: [A-Za-z_][A-Za-z0-9_]* */
    if (isalpha(c) || c == '_') {
        while (isalnum(c) || c == '_') {
            anexarChar(buf, &n, c);
            c = lerCaractere(codigo);
        }
        devolverCaractere(codigo, c);
        buf[n] = '\0';
        return criarToken(classificarIdentificador(buf), buf, linha);
    }

    /* 4) Constantes numericas: INT_CONST [0-9]+ ou FLOAT_CONST [0-9]+\.[0-9]+ */
    if (isdigit(c)) {
        while (isdigit(c)) {
            anexarChar(buf, &n, c);
            c = lerCaractere(codigo);
        }

        if (c == '.') {
            anexarChar(buf, &n, c);
            int prox = olharCaractere(codigo);

            if (!isdigit(prox)) {
                /* "3." ou "3.x": float mal formado */
                buf[n] = '\0';
                return criarToken(TOK_INVALID, buf, linha);
            }

            c = lerCaractere(codigo);
            while (isdigit(c)) {
                anexarChar(buf, &n, c);
                c = lerCaractere(codigo);
            }
            devolverCaractere(codigo, c);
            buf[n] = '\0';
            return criarToken(TOK_FLOAT_CONST, buf, linha);
        }

        devolverCaractere(codigo, c);
        buf[n] = '\0';
        return criarToken(TOK_INT_CONST, buf, linha);
    }

    /* 5) Operadores e delimitadores (com correspondencia maxima). */
    switch (c) {
        case '+': return criarToken(TOK_PLUS, "+", linha);
        case '-': return criarToken(TOK_MINUS, "-", linha);
        case '*': return criarToken(TOK_MULT, "*", linha);
        case '/': return criarToken(TOK_DIV, "/", linha);
        case '(': return criarToken(TOK_LPAREN, "(", linha);
        case ')': return criarToken(TOK_RPAREN, ")", linha);
        case '{': return criarToken(TOK_LBRACE, "{", linha);
        case '}': return criarToken(TOK_RBRACE, "}", linha);
        case ';': return criarToken(TOK_SEMICOLON, ";", linha);

        case '=':
            if (olharCaractere(codigo) == '=') {
                lerCaractere(codigo);
                return criarToken(TOK_EQ, "==", linha);
            }
            return criarToken(TOK_ASSIGN, "=", linha);

        case '<':
            if (olharCaractere(codigo) == '=') {
                lerCaractere(codigo);
                return criarToken(TOK_LE, "<=", linha);
            }
            return criarToken(TOK_LT, "<", linha);

        case '>':
            if (olharCaractere(codigo) == '=') {
                lerCaractere(codigo);
                return criarToken(TOK_GE, ">=", linha);
            }
            return criarToken(TOK_GT, ">", linha);

        case '!':
            if (olharCaractere(codigo) == '=') {
                lerCaractere(codigo);
                return criarToken(TOK_NE, "!=", linha);
            }
            return criarToken(TOK_INVALID, "!", linha);

        default: {
            /* Qualquer outro caractere nao pertence a nenhum token valido. */
            char s[2] = { (char)c, '\0' };
            return criarToken(TOK_INVALID, s, linha);
        }
    }
}

/* -------------------- Tabela de simbolos -------------------- */

typedef enum {
    TIPO_INT,
    TIPO_FLOAT,
    TIPO_ERRO
} TipoDado;

typedef struct EntradaSimbolo {
    char lexema[TAM_LEXEMA];
    TipoDado tipo;
    int linha;
    int escopo;                   /* nivel do escopo: 0 = global */
    struct EntradaSimbolo *prox;
} EntradaSimbolo;

typedef struct TabelaSimbolos {
    EntradaSimbolo *entradas;
    int nivel;
    struct TabelaSimbolos *anterior;   /* escopo externo */
} TabelaSimbolos;

static const char *nomeTipo(TipoDado tipo)
{
    switch (tipo) {
        case TIPO_INT:   return "int";
        case TIPO_FLOAT: return "float";
        default:         return "erro";
    }
}

/* Cria um novo escopo, encadeado ao escopo externo "anterior" (pode ser NULL). */
TabelaSimbolos *criarEscopo(TabelaSimbolos *anterior)
{
    TabelaSimbolos *tabela = malloc(sizeof(TabelaSimbolos));
    if (tabela == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }
    tabela->entradas = NULL;
    tabela->anterior = anterior;
    tabela->nivel = (anterior == NULL) ? 0 : anterior->nivel + 1;
    return tabela;
}

/* Libera o escopo e devolve o escopo externo. */
TabelaSimbolos *destruirEscopo(TabelaSimbolos *tabela)
{
    TabelaSimbolos *anterior = tabela->anterior;
    EntradaSimbolo *e = tabela->entradas;

    while (e != NULL) {
        EntradaSimbolo *prox = e->prox;
        free(e);
        e = prox;
    }
    free(tabela);
    return anterior;
}

/* Busca apenas no escopo informado (usada para detectar redeclaracao). */
EntradaSimbolo *buscarSimboloLocal(TabelaSimbolos *tabela, const char *lexema)
{
    for (EntradaSimbolo *e = tabela->entradas; e != NULL; e = e->prox) {
        if (strcmp(e->lexema, lexema) == 0) {
            return e;
        }
    }
    return NULL;
}

/* Insere no escopo atual. Retorna 0 se ja existe no MESMO escopo, 1 se inseriu. */
int inserirSimbolo(TabelaSimbolos *tabela, const char *lexema,
                   TipoDado tipo, int linha)
{
    if (buscarSimboloLocal(tabela, lexema) != NULL) {
        return 0;
    }

    EntradaSimbolo *nova = malloc(sizeof(EntradaSimbolo));
    if (nova == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }
    strncpy(nova->lexema, lexema, TAM_LEXEMA - 1);
    nova->lexema[TAM_LEXEMA - 1] = '\0';
    nova->tipo = tipo;
    nova->linha = linha;
    nova->escopo = tabela->nivel;
    nova->prox = NULL;

    /* Insere no fim para manter a ordem de declaracao ao imprimir. */
    if (tabela->entradas == NULL) {
        tabela->entradas = nova;
    } else {
        EntradaSimbolo *ult = tabela->entradas;
        while (ult->prox != NULL) {
            ult = ult->prox;
        }
        ult->prox = nova;
    }
    return 1;
}

/* Busca no escopo atual e, se nao achar, nos escopos externos. */
EntradaSimbolo *buscarSimbolo(TabelaSimbolos *tabela, const char *lexema)
{
    for (TabelaSimbolos *t = tabela; t != NULL; t = t->anterior) {
        EntradaSimbolo *e = buscarSimboloLocal(t, lexema);
        if (e != NULL) {
            return e;
        }
    }
    return NULL;
}

void imprimirEscopo(TabelaSimbolos *tabela)
{
    printf("\n[Tabela de simbolos - escopo nivel %d%s]\n",
           tabela->nivel, tabela->nivel == 0 ? " (global)" : "");
    printf("  %-20s %-8s %-8s %s\n", "NOME", "TIPO", "ESCOPO", "DECLARADA NA LINHA");

    if (tabela->entradas == NULL) {
        printf("  (vazio)\n");
        return;
    }
    for (EntradaSimbolo *e = tabela->entradas; e != NULL; e = e->prox) {
        printf("  %-20s %-8s %-8d %d\n",
               e->lexema, nomeTipo(e->tipo), e->escopo, e->linha);
    }
}

/* -------------------- Parser + analise semantica -------------------- */

typedef struct {
    CodigoFonte *codigo;
    Token tokenAtual;
    TabelaSimbolos *escopoAtual;
    int errosSemanticos;
    int mostrarTabela;      /* imprime cada escopo ao fecha-lo */
} Parser;

/* Erros lexicos e sintaticos interrompem a analise (longjmp);
   erros semanticos sao contados e a analise continua. */
static jmp_buf saidaErro;

void avancarEntrada(Parser *parser)
{
    parser->tokenAtual = proximoToken(parser->codigo);

    if (parser->tokenAtual.tipo == TOK_INVALID) {
        erroLexico(parser->tokenAtual.linha, parser->tokenAtual.lexema);
        longjmp(saidaErro, 1);
    }
}

void casar(Parser *parser, TokenType esperado)
{
    if (parser->tokenAtual.tipo == esperado) {
        avancarEntrada(parser);
    } else {
        erroSintatico(parser->tokenAtual.linha, esperado,
                      parser->tokenAtual.tipo);
        longjmp(saidaErro, 1);
    }
}

static void erroSintaticoAbortar(Parser *parser, const char *esperado)
{
    erroSintaticoMsg(parser->tokenAtual.linha, esperado, parser->tokenAtual.tipo);
    longjmp(saidaErro, 1);
}

/* Registra um erro semantico (formatado) e continua a analise. */
static void erroSem(Parser *parser, int linha, const char *formato, ...)
{
    char mensagem[300];
    va_list args;

    va_start(args, formato);
    vsnprintf(mensagem, sizeof(mensagem), formato, args);
    va_end(args);

    erroSemantico(linha, mensagem);
    parser->errosSemanticos++;
}

static void entrarEscopo(Parser *parser)
{
    parser->escopoAtual = criarEscopo(parser->escopoAtual);
}

static void sairEscopo(Parser *parser)
{
    if (parser->mostrarTabela) {
        imprimirEscopo(parser->escopoAtual);
    }
    parser->escopoAtual = destruirEscopo(parser->escopoAtual);
}

/* Regra 6: int op int -> int; qualquer outra combinacao -> float. */
static TipoDado combinarTipos(TipoDado a, TipoDado b)
{
    if (a == TIPO_ERRO || b == TIPO_ERRO) return TIPO_ERRO;
    if (a == TIPO_INT && b == TIPO_INT)   return TIPO_INT;
    return TIPO_FLOAT;
}

static int ehInicioDeComando(TokenType t)
{
    return t == TOK_IDENTIFIER || t == TOK_PRINT || t == TOK_IF ||
           t == TOK_WHILE || t == TOK_LBRACE;
}

static int ehOperadorRelacional(TokenType t)
{
    return t == TOK_LT || t == TOK_LE || t == TOK_GT ||
           t == TOK_GE || t == TOK_EQ || t == TOK_NE;
}

/* Prototipos (a gramatica e mutuamente recursiva). */
static void listaDecl(Parser *p);
static void listaCmd(Parser *p);
static void comando(Parser *p);
static void bloco(Parser *p);
static TipoDado expressao(Parser *p);

/* Declaracao -> Tipo ID ; */
static void declaracao(Parser *p)
{
    TipoDado tipo = (p->tokenAtual.tipo == TOK_INT) ? TIPO_INT : TIPO_FLOAT;
    avancarEntrada(p);                       /* consome int/float */

    Token id = p->tokenAtual;
    casar(p, TOK_IDENTIFIER);
    casar(p, TOK_SEMICOLON);

    /* Regra 2: nao pode redeclarar no mesmo escopo. */
    if (!inserirSimbolo(p->escopoAtual, id.lexema, tipo, id.linha)) {
        EntradaSimbolo *anterior = buscarSimboloLocal(p->escopoAtual, id.lexema);
        erroSem(p, id.linha,
                "variavel '%s' ja foi declarada neste escopo (linha %d).",
                id.lexema, anterior->linha);
    }
}

/* ListaDecl -> Declaracao ListaDecl | epsilon */
static void listaDecl(Parser *p)
{
    while (p->tokenAtual.tipo == TOK_INT || p->tokenAtual.tipo == TOK_FLOAT) {
        declaracao(p);
    }
}

/* Fator -> ID | INT_CONST | FLOAT_CONST | ( Expressao ) */
static TipoDado fator(Parser *p)
{
    Token t = p->tokenAtual;

    switch (t.tipo) {
        case TOK_IDENTIFIER: {
            avancarEntrada(p);
            /* Regras 1, 4 e 5: busca do escopo atual para os externos. */
            EntradaSimbolo *s = buscarSimbolo(p->escopoAtual, t.lexema);
            if (s == NULL) {
                erroSem(p, t.linha, "variavel '%s' nao declarada.", t.lexema);
                return TIPO_ERRO;
            }
            return s->tipo;
        }
        case TOK_INT_CONST:
            avancarEntrada(p);
            return TIPO_INT;

        case TOK_FLOAT_CONST:
            avancarEntrada(p);
            return TIPO_FLOAT;

        case TOK_LPAREN: {
            avancarEntrada(p);
            TipoDado tipo = expressao(p);
            casar(p, TOK_RPAREN);
            return tipo;
        }
        default:
            erroSintaticoAbortar(p, "IDENTIFIER, INT_CONST, FLOAT_CONST ou LPAREN");
            return TIPO_ERRO;
    }
}

/* TermoLinha -> * Fator TermoLinha | / Fator TermoLinha | epsilon */
static TipoDado termoLinha(Parser *p, TipoDado esq)
{
    if (p->tokenAtual.tipo == TOK_MULT || p->tokenAtual.tipo == TOK_DIV) {
        avancarEntrada(p);
        TipoDado dir = fator(p);
        return termoLinha(p, combinarTipos(esq, dir));
    }
    return esq;
}

/* Termo -> Fator TermoLinha */
static TipoDado termo(Parser *p)
{
    TipoDado esq = fator(p);
    return termoLinha(p, esq);
}

/* ExprLinha -> + Termo ExprLinha | - Termo ExprLinha | epsilon */
static TipoDado exprLinha(Parser *p, TipoDado esq)
{
    if (p->tokenAtual.tipo == TOK_PLUS || p->tokenAtual.tipo == TOK_MINUS) {
        avancarEntrada(p);
        TipoDado dir = termo(p);
        return exprLinha(p, combinarTipos(esq, dir));
    }
    return esq;
}

/* Expressao -> Termo ExprLinha */
static TipoDado expressao(Parser *p)
{
    TipoDado esq = termo(p);
    return exprLinha(p, esq);
}

/* Condicao -> Expressao RelOp Expressao
   Regra 8: os dois lados sao numericos; os tipos podem ser diferentes. */
static void condicao(Parser *p)
{
    expressao(p);

    if (!ehOperadorRelacional(p->tokenAtual.tipo)) {
        erroSintaticoAbortar(p,
            "operador relacional (<, <=, >, >=, ==, !=)");
    }
    avancarEntrada(p);

    expressao(p);
}

/* Atribuicao -> ID = Expressao ; */
static void atribuicao(Parser *p)
{
    Token id = p->tokenAtual;
    casar(p, TOK_IDENTIFIER);

    EntradaSimbolo *s = buscarSimbolo(p->escopoAtual, id.lexema);
    if (s == NULL) {
        erroSem(p, id.linha, "variavel '%s' nao declarada.", id.lexema);
    }

    casar(p, TOK_ASSIGN);
    TipoDado tipoExpr = expressao(p);
    casar(p, TOK_SEMICOLON);

    /* Regra 7: tipos iguais, sem conversao implicita. */
    if (s != NULL && tipoExpr != TIPO_ERRO && s->tipo != tipoExpr) {
        erroSem(p, id.linha,
                "tipo incompativel na atribuicao: '%s' e %s, mas a expressao e %s.",
                id.lexema, nomeTipo(s->tipo), nomeTipo(tipoExpr));
    }
}

/* Print -> print ( Expressao ) ;   (Regra 9: expressao sempre numerica) */
static void comandoPrint(Parser *p)
{
    casar(p, TOK_PRINT);
    casar(p, TOK_LPAREN);
    expressao(p);
    casar(p, TOK_RPAREN);
    casar(p, TOK_SEMICOLON);
}

/* If -> if ( Condicao ) Bloco OpcionalElse */
static void comandoIf(Parser *p)
{
    casar(p, TOK_IF);
    casar(p, TOK_LPAREN);
    condicao(p);
    casar(p, TOK_RPAREN);
    bloco(p);

    if (p->tokenAtual.tipo == TOK_ELSE) {
        avancarEntrada(p);
        bloco(p);
    }
}

/* While -> while ( Condicao ) Bloco */
static void comandoWhile(Parser *p)
{
    casar(p, TOK_WHILE);
    casar(p, TOK_LPAREN);
    condicao(p);
    casar(p, TOK_RPAREN);
    bloco(p);
}

/* Bloco -> { ListaDecl ListaCmd }   (cada bloco abre um novo escopo) */
static void bloco(Parser *p)
{
    casar(p, TOK_LBRACE);
    entrarEscopo(p);
    listaDecl(p);
    listaCmd(p);
    casar(p, TOK_RBRACE);
    sairEscopo(p);
}

/* Comando -> Atribuicao | Print | If | While | Bloco */
static void comando(Parser *p)
{
    switch (p->tokenAtual.tipo) {
        case TOK_IDENTIFIER: atribuicao(p);   break;
        case TOK_PRINT:      comandoPrint(p); break;
        case TOK_IF:         comandoIf(p);    break;
        case TOK_WHILE:      comandoWhile(p); break;
        case TOK_LBRACE:     bloco(p);        break;
        default:
            erroSintaticoAbortar(p, "um comando");
    }
}

/* ListaCmd -> Comando ListaCmd | epsilon */
static void listaCmd(Parser *p)
{
    while (ehInicioDeComando(p->tokenAtual.tipo)) {
        comando(p);
    }
}

/* Programa -> ListaDecl ListaCmd EOF
   Retorna 1 se o programa e valido (sem erros) e 0 caso contrario. */
int analisarPrograma(Parser *parser)
{
    parser->escopoAtual = NULL;
    parser->errosSemanticos = 0;

    if (setjmp(saidaErro) != 0) {
        /* Erro lexico/sintatico: libera os escopos que ficaram abertos. */
        while (parser->escopoAtual != NULL) {
            parser->escopoAtual = destruirEscopo(parser->escopoAtual);
        }
        return 0;
    }

    entrarEscopo(parser);          /* escopo global */
    avancarEntrada(parser);        /* le o primeiro token */

    listaDecl(parser);
    listaCmd(parser);
    casar(parser, TOK_EOF);

    sairEscopo(parser);

    return parser->errosSemanticos == 0;
}

/* -------------------- Programa principal -------------------- */

static void uso(const char *prog)
{
    fprintf(stderr, "Uso: %s [-l] [-t] arquivo.csmall\n", prog);
    fprintf(stderr, "  -l  apenas lista os tokens (teste do lexer)\n");
    fprintf(stderr, "  -t  imprime a tabela de simbolos de cada escopo\n");
}

int main(int argc, char **argv)
{
    int apenasLexer = 0;
    int mostrarTabela = 0;
    const char *nomeArquivo = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            apenasLexer = 1;
        } else if (strcmp(argv[i], "-t") == 0) {
            mostrarTabela = 1;
        } else if (nomeArquivo == NULL) {
            nomeArquivo = argv[i];
        } else {
            uso(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (nomeArquivo == NULL) {
        uso(argv[0]);
        return EXIT_FAILURE;
    }

    CodigoFonte codigo;
    if (!abrirCodigoFonte(&codigo, nomeArquivo)) {
        return EXIT_FAILURE;
    }

    if (apenasLexer) {
        printf("%-14s %-20s %s\n", "TOKEN", "LEXEMA", "LINHA");
        printf("-----------------------------------------------\n");

        Token token;
        do {
            token = proximoToken(&codigo);
            imprimirToken(token);
        } while (token.tipo != TOK_EOF && token.tipo != TOK_INVALID);

        int invalido = (token.tipo == TOK_INVALID);
        if (invalido) {
            erroLexico(token.linha, token.lexema);
        }
        fecharCodigoFonte(&codigo);
        return invalido ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    Parser parser;
    parser.codigo = &codigo;
    parser.mostrarTabela = mostrarTabela;

    int ok = analisarPrograma(&parser);
    fecharCodigoFonte(&codigo);

    if (ok) {
        printf("\nCompilacao concluida: nenhum erro encontrado.\n");
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "\nCompilacao falhou.\n");
    return EXIT_FAILURE;
}
