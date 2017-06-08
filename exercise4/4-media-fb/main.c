#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <stdarg.h>
#include <medialog.h>

#include "mediacenter.h"

static int dd_fd;

static void default_reaper(int s) {
  (void)s;
  while (waitpid(-1, NULL, WNOHANG) > 0) ;
}

static void handle_mount() {
  struct inotify_event *ev;
  void *mem;
  int evlen;
  char dev_name[256];

  (void)ioctl(dd_fd, FIONREAD, &evlen);

  if (evlen <= 0) goto ret;

  mem = malloc(evlen);
  if (read(dd_fd, mem, evlen) <= 0) goto free_ret;

  for (ev = mem; (void *)ev - mem < evlen; ev = (void *)ev + sizeof(*ev) + ev->len) {
    snprintf(dev_name, 256, "%s/%s", media_path, ev->name);

    if(ev->mask & IN_CREATE) {
      sleep(1);
      add_notify(dev_name);
      scan_dir(dev_name);
    } else if(ev->mask & IN_DELETE) {
      del_notify(dev_name);
      forget_dir(dev_name);
    }
  }

free_ret:
  free(mem);
ret:
  return;
}

int main() {
  int watch_fd;

  openmedialog("media", LOG_ODELAY | LOG_PID, LOG_DAEMON);

  signal(SIGCHLD, &default_reaper);
  signal(SIGPIPE, SIG_IGN);

  init_notify();		// init inotify master socket for live-monitoring mounted USB sticks

  init_play_subsystem();
  init_scan_subsystem();

  dd_fd = inotify_init();	// init inotify socket for monitoring /media for new mounts
  if (dd_fd < 0) return 1;

  watch_fd = inotify_add_watch(dd_fd, media_path, IN_CREATE | IN_DELETE);

  mev_add(dd_fd, &handle_mount, &handle_mount, NULL);

  if (network_open() != 0) return 1;

  mev_run();

  inotify_rm_watch(dd_fd, watch_fd);
  close(dd_fd);

  return 0;
}
