#include "bc_dsp.h"
int sim_write(int fd, const void *b, int n);
void *memset(void *d, int c, unsigned n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, unsigned n) { unsigned char *p = d; const unsigned char *q = s; while (n--) *p++ = *q++; return d; }
static void puts_(const char *s) { int n = 0; while (s[n]) ++n; sim_write(1, s, n); }
static void putu(unsigned v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); puts_(b + i); }
static inline unsigned cycles(void) { unsigned c; __asm__ volatile ("%0 = CYCLES;" : "=d"(c)); return c; }
static struct bc_voice V;
static unsigned char fastmem[4096] __attribute__((aligned(32)));
int main(void){
  bc_fast(fastmem);
  static const short K[8] = { 16384, 16384, 16384, 32767, 0, 0, 0, 8000 };
  unsigned pitch[128]; short out[128];
  for (int j=0;j<128;++j) pitch[j]=3185015u; /* C2 */
  for (int cap=1; cap<=13; cap+=4) {
    bc_voice_init(&V,0); bc_note_on(&V); bc_cap = cap;
    unsigned w=0;
    for (int b=0;b<40;++b){ bc_cap=cap; unsigned t=cycles(); bc_render(&V,out,K,pitch); unsigned c=cycles()-t; if(b>20&&c>w)w=c; }
    puts_("cap "); putu(cap); puts_(": "); putu(w); puts_("\n");
  }
  return 0;
}
