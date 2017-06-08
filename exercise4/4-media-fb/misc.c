#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>
#include <medialog.h>
#include "mediacenter.h"

static char buffer[1024] = { 0, };

void mediacenter_log(int level, const char *format, ...) {
  va_list v;
  va_start(v, format);

  vsnprintf(buffer + strlen(buffer), 1024 - strlen(buffer), format, v);

  if (format[strlen(format)-1] == '\n') {
    medialog(level, "%s", buffer);
    *buffer = 0;
  }
}
