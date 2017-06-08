#define _GNU_SOURCE
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include "mediacenter.h"

static int noti_fd;
static void handle_notify(void);

struct watched_dir {
  struct watched_dir *next;
  char path[1024];
  int watch_id;
  int pending_rescan;
};

static struct watched_dir *watchlist = NULL;
static void *rescanner_stack_page;
static int rescanner_thread(void *);

static int tid_master, tid_scanner;

#define	CHILD_STACK_SIZE		32*4096

void init_notify() {
  noti_fd = inotify_init();

  if (noti_fd >= 0) {
    mev_add(noti_fd, &handle_notify, NULL, NULL);
  }

  rescanner_stack_page = mmap(NULL, CHILD_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  tid_master = syscall(__NR_gettid);

  clone(&rescanner_thread, (rescanner_stack_page + (CHILD_STACK_SIZE - 4)), CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_THREAD | CLONE_SIGHAND | CLONE_SYSVSEM | CLONE_VM, NULL);
}

void add_notify(const char *dir) {
  struct watched_dir **wdpp = &watchlist;
  struct watched_dir *newdir = malloc(sizeof(struct watched_dir));

  while (*wdpp) wdpp = &((*wdpp)->next);

  strcpy(newdir->path, dir);

  newdir->watch_id = inotify_add_watch(noti_fd, dir, IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
  newdir->next = NULL;
  newdir->pending_rescan = 0;

  *wdpp = newdir;

  mediacenter_log(LOG_INFO, "Directory added to inotify as ID %x.\n", newdir->watch_id);
}

void del_notify(const char *dir) {
  struct watched_dir **wdpp;

  for (wdpp = &watchlist; *wdpp; wdpp = &((*wdpp)->next)) {
    if (strcmp((*wdpp)->path, dir) == 0) {
      struct watched_dir *drop = *wdpp;
      *wdpp = (*wdpp)->next;
      inotify_rm_watch(noti_fd, drop->watch_id);
      mediacenter_log(LOG_INFO, "Inotify ID %x removed.\n", drop->watch_id);
      free(drop);
      return;
    }
  }
}

static void handle_notify() {
  struct watched_dir *wdp;
  void *mem;
  struct inotify_event *ev;
  int evlen;

  (void)ioctl(noti_fd, FIONREAD, &evlen);

  if (evlen <= 0) goto ret;

  mem = malloc(evlen);
  if (read(noti_fd, mem, evlen) <= 0) goto free_ret;

  for (ev = mem; (void *)ev - mem < evlen; ev = (void *)ev + sizeof(*ev) + ev->len) {
    for (wdp = watchlist; wdp; wdp = wdp->next) {
      if (wdp->watch_id == ev->wd) {
        wdp->pending_rescan = 1;
        break;
      }
    }

    if (wdp) {
      mediacenter_log(LOG_INFO, "Inotify triggered (ID %x, mask %x) - flagging directory for rescan.\n", ev->wd, ev->mask);
    } else {
      mediacenter_log(LOG_INFO, "Unknown inotify trigger (ID %x, mask %x)!\n", ev->wd, ev->mask);
    }
  }

free_ret:
  free(mem);
ret:
  return;
}

static int rescanner_thread(void *dummy) {
  int tgid = getpid();
  tid_scanner = syscall(__NR_gettid);
  (void)dummy;

  while (syscall(__NR_tgkill, tgid, tid_master, 0) == 0) {
    struct watched_dir *wd;
    sleep(10);
    for (wd = watchlist; wd; wd = wd->next) {
      if (wd->pending_rescan == 1) {
        forget_dir(wd->path);
        scan_dir(wd->path);
        wd->pending_rescan = 0;
      }
    }
  }

  return 0;
}

void rescan_all(void) {
  struct watched_dir *wd;

  for (wd = watchlist; wd; wd = wd->next) {
    if (wd->pending_rescan == 1) {
      forget_dir(wd->path);
      scan_dir(wd->path);
      wd->pending_rescan = 0;
    }
  }
}
