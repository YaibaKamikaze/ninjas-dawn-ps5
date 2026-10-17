// [ps5 port] stat("") and lstat("") as Linux does them.
//
// The PS5's libc answers an empty path with something other than ENOENT,
// which std::filesystem turns into an exception where Linux code expects a
// plain "does not exist". Linked with --wrap=stat --wrap=lstat, so every
// caller in the program comes through here first.
#include <errno.h>
#include <sys/stat.h>

int __real_stat(const char* path, struct stat* st);
int __real_lstat(const char* path, struct stat* st);

int __wrap_stat(const char* path, struct stat* st) {
  if (path == 0 || path[0] == '\0') {
    errno = ENOENT;
    return -1;
  }
  return __real_stat(path, st);
}

int __wrap_lstat(const char* path, struct stat* st) {
  if (path == 0 || path[0] == '\0') {
    errno = ENOENT;
    return -1;
  }
  return __real_lstat(path, st);
}
