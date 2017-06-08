#include <sys/socket.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "mediacenter.h"

static int ss, cs;
static struct sockaddr_in sun;
struct sockaddr_in sun_client;
unsigned int sun_client_len;

#define xmit(data)	send(cs, data, strlen(data), 0)

static const char *helptxt[] = {
  "Available commands:\n",
  "  - help    Displays this text\n",
  "  - list    Emits the current list of multimedia files\n",
  "  - next    Advances the cursor to the next file\n",
  "  - play    Plays the file at the cursor, then advances\n"
  "  - scan    Force rescan of media directories\n"
};

static void unix_command(const char *cmd) {
  if (strncmp(cmd, "list", 4) == 0) {
    char intbuf[16];
    struct media_file *iter;
    sprintf(intbuf, "%d\n", total_count);
    xmit(intbuf);
    for (iter = next_file(NULL); iter; iter = next_file(iter)) {
      xmit((iter == cursor) ? "=> " : "   ");
      xmit(file_to_chunk(iter)->dir);
      xmit("/");
      xmit(iter->name);
      xmit("\n");
    }
  } else if (strncmp(cmd, "help", 4) == 0) {
    unsigned int i;
    for (i = 0; i < sizeof(helptxt) / sizeof(char *); i++)
      xmit(helptxt[i]);
  } else if (strncmp(cmd, "next", 4) == 0) {
    advance_cursor();
  } else if (strncmp(cmd, "play", 4) == 0) {
    if (cursor)
      play_file(cursor);
    advance_cursor();
  } else if (strncmp(cmd, "scan", 4) == 0) {
    rescan_all();
  }
}

static void handle_client_read() {
  char *buffer = malloc(1600);
  int rlen;

  memset(buffer, 0, 1600);
  rlen = recv(cs, buffer, 1600, 0);
  if (rlen > 0) {
    for (rlen--; rlen; rlen--) {
      if (buffer[rlen] == 0x0a) buffer[rlen] = 0;
    }
    mediacenter_log(LOG_INFO, "Received command \"%s\" from %s.\n",
        buffer, inet_ntoa(sun_client.sin_addr));
    unix_command(buffer);
  }

  free(buffer);
}

static void handle_client_connection() {
  sun_client_len = sizeof(struct sockaddr_in);
  if ((cs = accept(ss, (struct sockaddr *)&sun_client,
    &sun_client_len)) < 0) {
    return;
  }

  mev_add(cs, &handle_client_read, NULL, NULL);

}

int network_open() {
  ss = socket(AF_INET, SOCK_STREAM, 0);
  if (ss < 0)
    return 1;

  sun.sin_family = AF_INET;
  sun.sin_addr.s_addr = htonl(INADDR_ANY);
  sun.sin_port = htons(1337);

  if (bind(ss, (struct sockaddr *)&sun, sizeof(sun)) != 0)
    return 1;

  if (listen(ss, 5) != 0)
    return 1;

  mev_add(ss, &handle_client_connection, NULL, NULL);

  return 0;
}

void network_close() {
  // TODO: close server socket
}

