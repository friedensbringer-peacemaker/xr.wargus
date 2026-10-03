// Ersatz für <android/log.h> in der Desktop-Vorschau: Ausgabe auf stderr.
#pragma once
#include <stdarg.h>
#include <stdio.h>
enum { ANDROID_LOG_DEBUG = 3, ANDROID_LOG_INFO = 4, ANDROID_LOG_WARN = 5, ANDROID_LOG_ERROR = 6 };
static inline int __android_log_print(int, const char* tag, const char* fmt, ...) {
  va_list a;
  va_start(a, fmt);
  fprintf(stderr, "[%s] ", tag);
  vfprintf(stderr, fmt, a);
  fputc('\n', stderr);
  va_end(a);
  return 0;
}
static inline int __android_log_write(int, const char* tag, const char* text) {
  return fprintf(stderr, "[%s] %s\n", tag, text);
}
