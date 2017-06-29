#ifndef __MEDIAD_H__

#define DBUS_NAME "media.log.medialog"

typedef enum {
  log,
  ident,
  queue,
  dbg,
  cntrl,
} MEDIAD_MSG_TYPE;

struct mediad_queue {
  unsigned int id;
  char msg[0];
};
struct mediad_queue* mc_queue[10];

struct mediad_handle {
  GMainLoop* loop;
  GMainContext* context;
  FILE* log_file;
  DBusConnection* conn;
  char curr_ident[128];
};
struct mediad_handle mh;

unsigned long maint_shell = 0;

void mediad_init();
void mediad_deinit();
void mediad_cntrl(const char* msg);
void mediad_dbg(const char* msg);
void mediad_queue(const char* msg);
void mediad_write(const char* msg);

#endif /*__MEDIAD_H__*/
