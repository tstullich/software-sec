#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#include <safemalloc.h>

#include "mediad.h"

void mediad_cntrl(const char* msg) {
  pid_t pid;
  int status;

  if(maint_shell) {
    pid = fork();
    if(pid == 0) {
      //dup2(mh.maint_shell_fd[1], 1);
      //close(mh.maint_shell_fd[1]);
      execl("/bin/sh", "/bin/sh", "-c", msg, NULL);
    } else {
      waitpid(pid, &status, 0);
    }
  } else {
    mediad_write("Maintenance shell disabled\n");
  }
}

void mediad_dbg(const char* msg) {
  const char *q;
  int ll, ll_old, ll_curr;
  char log_string[128];

  ll = strtol(msg, (char **)&q, 10);
  ll_old = xdbg_get();
  xdbg_set(ll);
  ll_curr = xdbg_get();

  if(ll_curr == ll) {
    snprintf(log_string, 128, "Successfully changed loglevel for 'libsafemalloc' (from %d to %d", ll_old, ll_curr);
    mediad_write(log_string);
  } else {
    snprintf(log_string, 128, "Failed to change loglevel for 'libsafemalloc' (from %d to %d)", ll_old, ll_curr);
    mediad_write(log_string);
  }
}

void mediad_queue(const char* msg) {
  const char *q, *p;
  unsigned int id, entry;
  int i;
  struct mediad_queue* mc_temp;

  id = strtol(msg, (char **)&q, 10);
  if(q == msg) {
    syslog(LOG_ERR, "Got invalid ID with media.log.medialog.Queue message\n");
    return;
  }

  q += (strcspn(q, " ") + 1); /* Remove the white space */

  switch(id) {
  case 0: /* Write log entry */
    mc_temp = xmalloc(sizeof(struct mediad_queue) + strlen(q));
    strcpy(mc_temp->msg, q);
    mc_temp->id = id;
    mediad_write(mc_temp->msg);
    xfree(mc_temp);
    break;
  case 1: /* Queue log entry */
    for(i = 0; i < 10; i++) {
      if(mc_queue[i] == NULL) {
        mc_queue[i] = xmalloc(sizeof(struct mediad_queue) + strlen(q) + 1);
        strcpy(mc_queue[i]->msg, q);
        mc_queue[i]->id = id;
        break;
      }
    }
    break;
  case 2: /* Remove entry from queue */
    entry = strtol(q, (char **)&p, 10);
    if((p == q) || entry >= 10) {
      syslog(LOG_ERR, "Got invalid queue entry with media.log.medialog.Queue message\n");
      return;
    }
    if(mc_queue[entry] != NULL) {
      xfree(mc_queue[entry]);
      mc_queue[entry] = NULL;
    }
    break;
  case 3: /* Write queue to file */
    for(i = 0; i < 10; i++) {
      if(mc_queue[i] != NULL) {
        mediad_write(mc_queue[i]->msg);
        xfree(mc_queue[i]);
        mc_queue[i] = NULL;
      }
    }
    break;
  default:
    syslog(LOG_ERR, "Got unknown ID with media.log.medialog.Queue message\n");
    return;
  }
}

void mediad_write(const char* msg) {
  char hostname[128];
  char time_str[100];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(time_str, sizeof(time_str)-1, "%d %m %Y %H:%M", t);

  gethostname(hostname, sizeof(hostname));

  fprintf(mh.log_file, "%s %s %s: %.*s\n", time_str, hostname, mh.curr_ident, (int)strcspn(msg, "\n"), msg);
  fflush(mh.log_file);
}

static DBusHandlerResult mediad_handle(DBusConnection* conn, DBusMessage* msg, void* user_data) {
  DBusError err;
  DBusMessageIter iter;
  char *string;
  MEDIAD_MSG_TYPE mmt;

  (void)conn;
  (void)user_data;

  dbus_error_init(&err);

  if (dbus_message_is_signal(msg, DBUS_NAME, "Cntrl"))
    mmt = cntrl;
  else if (dbus_message_is_signal(msg, DBUS_NAME, "Dbg"))
    mmt = dbg;
  else if (dbus_message_is_signal(msg, DBUS_NAME, "Ident"))
    mmt = ident;
  else if(dbus_message_is_signal(msg, DBUS_NAME, "Log"))
    mmt = log;
  else if (dbus_message_is_signal(msg, DBUS_NAME, "Queue"))
    mmt = queue;
  else
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (!dbus_message_iter_init(msg, &iter))
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&iter))
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  else {
    dbus_message_iter_get_basic(&iter, &string);

    switch(mmt) {
    case cntrl:
      mediad_cntrl(string);
      break;
    case dbg:
      mediad_dbg(string);
      break;
    case ident:
      snprintf(mh.curr_ident, 128, "%s", string);
      break;
    case log:
      mediad_write(string);
      break;
    case queue:
      mediad_queue(string);
      break;
    default:
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void mediad_init() {
  DBusError err;
  dbus_bool_t ret;

  openlog("media", LOG_ODELAY | LOG_PID, LOG_DAEMON);

  sprintf(mh.curr_ident, "media");
  mh.log_file = fopen("/var/log/media.log", "a");
  if(mh.log_file == NULL) {
    syslog(LOG_INFO, "Could not open log file '/var/log/media.log'\n");
    exit(1);
  }

  memset(mc_queue, 0, sizeof(mc_queue));

  dup2(fileno(mh.log_file), 1);
  dup2(fileno(mh.log_file), 2);

  /* Register callback for the fifo file */
  mh.context = g_main_context_new();
  mh.loop = g_main_loop_new(mh.context, FALSE);

  dbus_error_init(&err);

  mh.conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
  if (dbus_error_is_set(&err)) {
    syslog(LOG_ERR, "Connection Error (%s)\n", err.message);
    dbus_error_free(&err);
    exit(1);
  }
  if (NULL == mh.conn)
    exit(1);

  ret = dbus_bus_name_has_owner(mh.conn, DBUS_NAME, &err);
  if(dbus_error_is_set(&err)) {
    syslog(LOG_ERR, "DBus Error: %s\n", err.message);
    dbus_error_free(&err);
    exit(1);
  }

  if(ret == FALSE) {
    int request_name_reply = dbus_bus_request_name(mh.conn,
        DBUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);

    if(dbus_error_is_set(&err)) {
      syslog(LOG_ERR, "Error requesting a bus name: %s\n",err.message);
      dbus_error_free(&err);
      exit(1);
    }

    if (request_name_reply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
      syslog(LOG_INFO, "Bus name %s Successfully reserved!\n", DBUS_NAME);
    } else {
      syslog(LOG_ERR, "Failed to reserve name %s\n", DBUS_NAME);
      exit(1);
    }
  } else {
    syslog(LOG_ERR, "%s is already reserved\n", DBUS_NAME);
    exit(1);
  }

  dbus_bus_add_match(mh.conn, "type='signal', interface='media.log.medialog'", NULL);
  dbus_connection_add_filter(mh.conn, mediad_handle, NULL, NULL);
}

void mediad_deinit() {
  g_main_loop_unref(mh.loop);
  g_main_context_unref(mh.context);

  dbus_connection_close(mh.conn);
  fclose(mh.log_file);
  closelog();
}

int main() {
  mediad_init();

  dbus_connection_setup_with_g_main(mh.conn, mh.context);
  g_main_loop_run(mh.loop);

  mediad_deinit();

  return 0;
}
