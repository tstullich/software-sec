#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include "mediacenter.h"

static struct mevhdl {
  struct mevhdl *next;
  int fd;
  void (*rfn)();
  void (*wfn)();
  void (*efn)();
} *mevh;

static fd_set master_fds[3];
static int master_maxfd;

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif

void mev_add(int fd, void (*r)(), void (*w)(), void (*e)()) {
  struct mevhdl **mprev;

  struct mevhdl *M = malloc(sizeof(*M));
  *M = (struct mevhdl){ NULL, fd, r, w, e };
  if (r) FD_SET(fd, master_fds+0);
  if (w) FD_SET(fd, master_fds+1);
  if (e) FD_SET(fd, master_fds+2);
  master_maxfd = max(master_maxfd, fd);

  for (mprev = &mevh; *mprev; mprev = &((*mprev)->next)) ;
  *mprev = M;
}

void mev_del(int fd) {
  struct mevhdl *m;

  FD_CLR(fd, master_fds+0);
  FD_CLR(fd, master_fds+1);
  FD_CLR(fd, master_fds+2);

  if (mevh->fd == fd) {
    m = mevh;
    mevh = m->next;
    free(m);
    return;
  }

  for (m = mevh; m->next; m = m->next) {
    if (m->next->fd == fd) {
      struct mevhdl *n = m->next;
      m->next = m->next->next;
      free(n);
      return;
    }
  }
}

void mev_run() {
  struct mevhdl *m;
  fd_set FC[3];
  int nfd;

  do {
    memcpy(FC, master_fds, 3 * sizeof(fd_set));
    nfd = select(master_maxfd + 1, FC+0, FC+1, FC+2, NULL);

    if (nfd <= 0) continue;

    for (m = mevh; m; m = m->next) {
      if (m->rfn && FD_ISSET(m->fd, FC+0)) {
        m->rfn();
      } else if (m->wfn && FD_ISSET(m->fd, FC+1)) {
        m->wfn();
      } else if (m->efn && FD_ISSET(m->fd, FC+2)) {
        m->efn();
      }
    }

  } while (1);
}
