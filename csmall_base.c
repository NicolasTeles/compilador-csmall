#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        case TOK_INT: 
            return "INT";
        case TOK_FLOAT: 
            return "FLOAT";
        case TOK_IF: 
            return "IF";
        case TOK_ELSE: 
            return "ELSE";
        case TOK_WHILE: 
            return "WHILE";
        case TOK_PRINT: 
            return "PRINT";
        case TOK_IDENTIFIER: 
            return "IDENTIFIER";
        case TOK_INT_CONST: 
            return "INT_CONST";
        case TOK_FLOAT_CONST: 
            return "FLOAT_CONST";
        case TOK_PLUS: 
            return "PLUS";
        case TOK_MINUS: 
            return "MINUS";
        case TOK_MULT: 
            return "MULT";
        case TOK_DIV: 
            return "DIV";
        case TOK_ASSIGN: 
            return "ASSIGN";
        case TOK_LT: 
            return "LT";
        case TOK_LE: 
            return "LE";
        case TOK_GT: 
            return "GT";
        case TOK_GE: 
            return "GE";
        case TOK_EQ: 
            return "EQ";
        case TOK_NE: 
            return "NE";
        case TOK_LPAREN: 
            return "LPAREN";
        case TOK_RPAREN: 
            return "RPAREN";
        case TOK_LBRACE: 
            return "LBRACE";
        case TOK_RBRACE: 
            return "RBRACE";
        case TOK_SEMICOLON: 
            return "SEMICOLON";
        case TOK_EOF: 
            return "EOF";
        case TOK_INVALID: 
            return "INVALID";
        default: 
            return "UNKNOWN";
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

/* Lê um caractere para consulta e depois o devolve ao arquivo (lookahead). */
int olharCaractere(CodigoFonte *codigo)
{
    int caractere = lerCaractere(codigo);
    devolverCaractere(codigo, caractere);
    return caractere;
}

/* -------------------- Mensagens de erro -------------------- */

void erroLexico(int linha)
{
    fprintf(stderr, "Erro lexico na linha %d.\n", linha);
}

void erroSintatico(int linha, TokenType esperado, TokenType encontrado)
{
    fprintf(stderr, "Erro sintatico na linha %d: esperado %s, encontrado %s.\n",
            linha, nomeToken(esperado), nomeToken(encontrado));
}

void erroSemantico(int linha, const char *mensagem)
{
    fprintf(stderr, "Erro semantico na linha %d: %s\n", linha, mensagem);
}

/* -------------------- Lexer: IMPLEMENTAR -------------------- */

Token proximoToken(CodigoFonte *codigo)
{
    /*
     TODO: implementar o reconhecimento de tokens.
     Sugestao: use lerCaractere(), olharCaractere() e devolverCaractere().
     */
    Token token = { TOK_EOF, "", codigo->linha };
    return token;
}

/* -------------------- Parser: estrutura inicial -------------------- */

typedef struct {
    CodigoFonte *codigo;
    Token tokenAtual;
} Parser;

void avancarEntrada(Parser *parser)
{
    parser->tokenAtual = proximoToken(parser->codigo);
}

void casar(Parser *parser, TokenType esperado)
{
    if (parser->tokenAtual.tipo == esperado) {
        avancarEntrada(parser);
    } else {
        erroSintatico(parser->tokenAtual.linha, esperado,
                      parser->tokenAtual.tipo);
    }
}

int analisarPrograma(Parser *parser)
{
    /* TODO: implementar a gramatica da Csmall por descida recursiva. */
    (void)parser;
    return 0;
}

/* -------------------- Tabela de simbolos: estruturas iniciais -------------------- */

typedef enum {
    TIPO_INT,
    TIPO_FLOAT,
    TIPO_ERRO
} TipoDado;

typedef struct EntradaSimbolo {
    char lexema[TAM_LEXEMA];
    TipoDado tipo;
    int linha;
    struct EntradaSimbolo *prox;
} EntradaSimbolo;

typedef struct TabelaSimbolos {
    EntradaSimbolo *entradas;
    struct TabelaSimbolos *anterior;
} TabelaSimbolos;

/* TODO: implementar insercao, busca e controle de escopos. */
int inserirSimbolo(TabelaSimbolos *tabela, const char *lexema,
                   TipoDado tipo, int linha);
EntradaSimbolo *buscarSimbolo(TabelaSimbolos *tabela, const char *lexema);

/* -------------------- Programa principal: testar o lexer -------------------- */

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Uso: %s arquivo.csmall\n", argv[0]);
        return EXIT_FAILURE;
    }

    CodigoFonte codigo;
    if (!abrirCodigoFonte(&codigo, argv[1])) {
        return EXIT_FAILURE;
    }

    printf("%-14s %-20s %s\n", "TOKEN", "LEXEMA", "LINHA");
    printf("-----------------------------------------------\n");

    Token token;
    do {
        token = proximoToken(&codigo);
        imprimirToken(token);
    } while (token.tipo != TOK_EOF && token.tipo != TOK_INVALID);

    fecharCodigoFonte(&codigo);
    return EXIT_SUCCESS;
}