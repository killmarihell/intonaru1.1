#include "rec.h"
#include <stdio.h>
#include <string.h>

static void w32(uint8_t *p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
static void w16(uint8_t *p, uint32_t v) { p[0]=v; p[1]=v>>8; }

static void header(uint8_t *h, uint32_t rate, uint32_t frames) {
    uint32_t ds = frames * 4;
    memcpy(h, "RIFF", 4); w32(h + 4, 36 + ds); memcpy(h + 8, "WAVEfmt ", 8); w32(h + 16, 16);
    w16(h + 20, 1); w16(h + 22, 2); w32(h + 24, rate); w32(h + 28, rate * 4); w16(h + 32, 4); w16(h + 34, 16);
    memcpy(h + 36, "data", 4); w32(h + 40, ds);
}

int rec_open(Rec *r, const char *dir, uint32_t rate, int16_t *ring_mem) {
    memset(r, 0, sizeof *r);
    r->ring = ring_mem; r->rate = rate;
    for (int i = 1; i < 10000; i++) {
        snprintf(r->path, sizeof r->path, "%s/REC_%04d.wav", dir, i);
        FILE *t = fopen(r->path, "rb");
        if (t) { fclose(t); continue; }
        break;
    }
    FILE *f = fopen(r->path, "wb");
    if (!f) { r->error = 1; return -1; }
    uint8_t h[44]; header(h, rate, 0);
    if (fwrite(h, 1, 44, f) != 44) { fclose(f); r->error = 1; return -1; }
    r->fp = f; r->active = 1;
    return 0;
}

void rec_push(Rec *r, const int16_t *st, int frames) {
    if (!r->active) return;
    uint32_t w = r->wr, used = w - r->rd;
    if (used + (uint32_t)frames > REC_RING_FRAMES) { r->dropped += (uint32_t)frames; return; }
    for (int i = 0; i < frames; i++) {
        uint32_t k = (w + (uint32_t)i) & (REC_RING_FRAMES - 1);
        r->ring[k * 2] = st[i * 2]; r->ring[k * 2 + 1] = st[i * 2 + 1];
    }
    __sync_synchronize();
    r->wr = w + (uint32_t)frames;
}

int rec_pump(Rec *r) {
    if (!r->fp) return 0;
    int total = 0;
    for (;;) {
        __sync_synchronize();
        uint32_t avail = r->wr - r->rd;
        if (!avail) break;
        uint32_t k = r->rd & (REC_RING_FRAMES - 1), n = avail;
        if (n > REC_RING_FRAMES - k) n = REC_RING_FRAMES - k;      /* trecho contiguo */
        if (n > 4096) n = 4096;
        if (fwrite(r->ring + k * 2, 4, n, (FILE *)r->fp) != n) { r->error = 1; r->active = 0; return total; }
        r->rd += n; r->frames += n; total += (int)n;
    }
    return total;
}

int rec_close(Rec *r) {
    r->active = 0;
    if (!r->fp) return -1;
    rec_pump(r);
    FILE *f = (FILE *)r->fp; uint8_t h[44]; header(h, r->rate, r->frames);
    int ok = (fseek(f, 0, SEEK_SET) == 0 && fwrite(h, 1, 44, f) == 44);
    fclose(f); r->fp = NULL;
    return (ok && !r->error) ? 0 : -1;
}

float rec_seconds(const Rec *r) { return r->rate ? (float)(r->frames + (r->wr - r->rd)) / (float)r->rate : 0.0f; }

int rec_wav_info(const char *path, uint32_t *rate, float *secs) {
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    uint8_t h[44]; size_t n = fread(h, 1, 44, f);
    long sz = 0; if (n == 44 && fseek(f, 0, SEEK_END) == 0) sz = ftell(f);
    fclose(f);
    if (n != 44 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return -1;
    uint32_t sr = h[24] | h[25] << 8 | h[26] << 16 | (uint32_t)h[27] << 24;
    if (!sr) return -1;
    *rate = sr; *secs = (float)((sz - 44) / 4) / (float)sr;
    return 0;
}

/* ---------------- reproducao ---------------- */
int play_open(Player *p, const char *path, int16_t *ring_mem) {
    memset(p, 0, sizeof *p);
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    uint8_t h[44];
    if (fread(h, 1, 44, f) != 44 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVEfmt ", 8) || memcmp(h + 36, "data", 4)) { fclose(f); return -1; }
    uint32_t fmt = h[20] | h[21] << 8, ch = h[22] | h[23] << 8, sr = h[24] | h[25] << 8 | h[26] << 16 | (uint32_t)h[27] << 24, bits = h[34] | h[35] << 8;
    uint32_t ds = h[40] | h[41] << 8 | h[42] << 16 | (uint32_t)h[43] << 24;
    if (fmt != 1 || bits != 16 || (ch != 1 && ch != 2) || sr != 32728) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f); if (sz < 44) { fclose(f); return -1; }
    if (ds > (uint32_t)(sz - 44)) ds = (uint32_t)(sz - 44);            /* cabecalho de gravacao interrompida */
    if (fseek(f, 44, SEEK_SET) != 0) { fclose(f); return -1; }
    p->ring = ring_mem; p->rate = sr; p->ch = (int)ch; p->total = ds / (ch * 2);
    snprintf(p->path, sizeof p->path, "%s", path);
    p->fp = f; p->active = 1;
    return 0;
}

int play_pump(Player *p) {
    if (!p->fp) return 0;
    int total = 0;
    for (;;) {
        __sync_synchronize();
        uint32_t used = p->wr - p->rd;
        if (p->read >= p->total) { p->eof = 1; break; }
        if (used >= PLAY_RING_FRAMES) break;
        uint32_t k = p->wr & (PLAY_RING_FRAMES - 1), n = PLAY_RING_FRAMES - used;
        if (n > PLAY_RING_FRAMES - k) n = PLAY_RING_FRAMES - k;
        if (n > 2048) n = 2048;
        if (n > p->total - p->read) n = p->total - p->read;
        size_t got;
        if (p->ch == 2) got = fread(p->ring + k * 2, 4, n, (FILE *)p->fp);
        else {
            int16_t tmp[2048]; got = fread(tmp, 2, n, (FILE *)p->fp);
            for (size_t i = 0; i < got; i++) { p->ring[(k + i) * 2] = tmp[i]; p->ring[(k + i) * 2 + 1] = tmp[i]; }
        }
        if (got == 0) { p->read = p->total; continue; }
        p->read += (uint32_t)got; __sync_synchronize(); p->wr += (uint32_t)got; total += (int)got;
    }
    return total;
}

int play_pull(Player *p, int16_t *out, int frames) {
    if (!p->active) { memset(out, 0, (size_t)frames * 4); return 0; }
    __sync_synchronize();
    uint32_t avail = p->wr - p->rd; int n = (int)(avail < (uint32_t)frames ? avail : (uint32_t)frames);
    for (int i = 0; i < n; i++) {
        uint32_t k = (p->rd + (uint32_t)i) & (PLAY_RING_FRAMES - 1);
        out[i * 2] = p->ring[k * 2]; out[i * 2 + 1] = p->ring[k * 2 + 1];
    }
    if (n < frames) memset(out + n * 2, 0, (size_t)(frames - n) * 4);
    __sync_synchronize(); p->rd += (uint32_t)n;
    if (p->eof && p->rd == p->wr) p->active = 0;           /* acabou */
    return n;
}

void play_close(Player *p) {
    p->active = 0;
    if (p->fp) { fclose((FILE *)p->fp); p->fp = NULL; }
}

float play_seconds(const Player *p) { return p->rate ? (float)p->rd / (float)p->rate : 0.0f; }
float play_length(const Player *p) { return p->rate ? (float)p->total / (float)p->rate : 0.0f; }
