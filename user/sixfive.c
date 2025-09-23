#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int sep(char c) {
  char *s = " -\r\t\n./,";
  for (; *s; s++) {
    if (c == *s) return 1;
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "usage: sixfive file...\n");
    exit(1);
  }

  char buf[256], num[32];
  int fd, n, k;

  for (int a = 1; a < argc; a++) {
    fd = open(argv[a], 0);
    if (fd < 0) {
      fprintf(2, "sixfive: cannot open %s\n", argv[a]);
      continue;
    }
    k = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
      for (int i = 0; i < n; i++) {
        char c = buf[i];
        if (c >= '0' && c <= '9') {
          num[k++] = c;
        } else {
          if (k > 0) {
            num[k] = 0;
            int v = atoi(num);
            if (v % 5 == 0 || v % 6 == 0) printf("%d\n", v);
            k = 0;
          }
        }
      }
    }
    if (k > 0) {
      num[k] = 0;
      int v = atoi(num);
      if (v % 5 == 0 || v % 6 == 0) printf("%d\n", v);
    }
    close(fd);
  }
  exit(0);
}


