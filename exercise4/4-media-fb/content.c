#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <syslog.h>
#include "mediacenter.h"

#define	COMPILE_TIME_HANDLERS		{ "audio/ogg", "/usr/bin/ogg123", \
					"audio/mpeg", "/usr/bin/mpg123", \
					NULL }

#ifdef CONFIG_CONFIGURABLE_HANDLERS
static const char **media_handlers;
static const char *media_default_handlers[] = COMPILE_TIME_HANDLERS;
#else
static const char *media_handlers[] = COMPILE_TIME_HANDLERS;
#endif

static void generic_exec(const char *e, const char *a) {
  const char * const av[3] = { e, a, NULL };
  int F;

  F = fork();
  if (F < 0) return;
  if (F == 0) {
    execve(e, (char * const *)av, NULL);
    exit(0);
  }
}

// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#ifdef CONFIG_CONFIGURABLE_HANDLERS
/**
 * Read a configurable list of installed media file handlers from conffile
 * and add compile-time defaults at the end.
 */
void init_play_subsystem() {
  media_handlers = malloc(512);
  do {
    int cf = open("/etc/media.mime", O_RDONLY);
    char *cfb = malloc(4096);
    char *cfp;
    int HI;

    memset(media_handlers, 0, 512);
    memcpy(media_handlers, media_default_handlers, sizeof(media_default_handlers));
    for (HI = 0; media_handlers[HI]; HI++)
      ;

    if (cf < 0) break;
    memset(cfb, 0, 4096);
    read(cf, cfb, 4095);
    close(cf);

    cfp = cfb;
    while (*cfp) {
      char *nextnl = strchr(cfp, '\n');
      char *colon = strchr(cfp, ':');
      if (nextnl == NULL) break;
      if (colon != NULL && colon < nextnl) {
        media_handlers[HI] = cfp;
        *colon = 0;
        media_handlers[HI+1] = colon+1;
        *nextnl = 0;
        HI += 2;
      }
      cfp = nextnl + 1;
      if (HI * sizeof(char *) >= 510) break;
    }
  } while (0);
}

#else

void init_play_subsystem() { }

#endif

void play_file(struct media_file *mf) {
  char F[256];
  struct media_file_chunk *mc = file_to_chunk(mf);

  sprintf(F, "%s/%s", mc->dir, mf->name);
  generic_exec(media_handlers[(mf->type << 1) + 1], F);
}

int find_handler(const char *mime) {
  int HI;

  for (HI = 0; media_handlers[HI]; HI += 2) {
    if (strcmp(media_handlers[HI], mime) == 0) {
      return HI >> 1;
    }
  }

  return -1;
}

