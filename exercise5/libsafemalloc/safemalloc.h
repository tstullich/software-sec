void *xmalloc(unsigned int size);
void xfree(void *ptr);

void xdbg_set(int ll);
int xdbg_get();

static inline void xmalloc_test(void) {
  int t;
  void *x[16];

  memset(x, 0, sizeof(void *) * 16);

  for (t=0; t<0x100000; t++) {
    int R = random();
    int i = R & 0xf;
    int S = (R >> 4) & 0xff;

    if (x[i] == NULL) {
	x[i] = xmalloc(S);
    } else {
	xfree(x[i]);
	x[i] = 0;
    }

    if (!(t & 0x1ffff)) printf(".\n");
  }

  for (t=0; t<16; t++) {
    if (x[t]) xfree(x[t]);
  }

}
