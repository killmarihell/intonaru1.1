/* INTONARUMORI - gravador WAV em fluxo (anel de memoria -> arquivo no SD). */
#ifndef REC_H
#define REC_H
#include <stdint.h>

#define REC_RING_FRAMES 32768     /* ~1 s de audio (128 KB) */

typedef struct {
    void *fp;                      /* FILE* */
    int16_t *ring;                 /* quadros estereo intercalados */
    volatile uint32_t wr, rd;      /* indices crescentes (em quadros) */
    volatile int active;           /* 1 = audio thread pode empurrar */
    volatile uint32_t dropped;     /* quadros perdidos por anel cheio */
    uint32_t frames;               /* quadros ja escritos no arquivo */
    uint32_t rate;
    int error;                     /* != 0 se escrita falhou */
    char path[96];
} Rec;

int  rec_open(Rec *r, const char *dir, uint32_t rate, int16_t *ring_mem);  /* cria REC_nnnn.wav */
void rec_push(Rec *r, const int16_t *st, int frames);  /* thread de audio */
int  rec_pump(Rec *r);                                  /* thread de escrita; devolve quadros gravados */
int  rec_close(Rec *r);                                 /* esvazia, corrige cabecalho, fecha */
float rec_seconds(const Rec *r);
int  rec_wav_info(const char *path, uint32_t *rate, float *secs);   /* le cabecalho */

/* 1.1: reproducao das gravacoes (WAV 16-bit estereo/mono a 32728 Hz, cabecalho de 44 bytes como o do gravador).
 * O arquivo e lido aos poucos para um anel (thread de escrita) e a thread de audio puxa os quadros. */
#define PLAY_RING_FRAMES 16384    /* ~0,5 s */
typedef struct {
    void *fp;
    int16_t *ring;                 /* quadros estereo intercalados */
    volatile uint32_t wr, rd;      /* indices crescentes (em quadros) */
    volatile int active;           /* 1 = tocando */
    volatile int eof;              /* arquivo todo lido */
    uint32_t total, read, rate;    /* quadros do arquivo, lidos do arquivo */
    int ch;
    char path[96];
} Player;
int   play_open(Player *p, const char *path, int16_t *ring_mem);   /* 0 = ok; -1 = arquivo invalido / taxa diferente */
int   play_pump(Player *p);                                        /* thread de escrita: enche o anel */
int   play_pull(Player *p, int16_t *out, int frames);              /* thread de audio: devolve quadros (o resto = 0); fim = active 0 */
void  play_close(Player *p);
float play_seconds(const Player *p);                               /* posicao */
float play_length(const Player *p);
#endif
