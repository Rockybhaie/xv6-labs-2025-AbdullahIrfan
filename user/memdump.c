#include "kernel/types.h"
#include "user/user.h"

void memdump(char *fmt, char *data) {
  while (*fmt) {
    if (*fmt == 'i') {
      int v = *(int*)data;
      printf("%d\n", v);
      data += 4;
    } else if (*fmt == 'p') {
      uint64 v = *(uint64*)data;
      printf("%lx\n", v);
      data += 8;
    } else if (*fmt == 'h') {
      short v = *(short*)data;
      printf("%d\n", v);
      data += 2;
    } else if (*fmt == 'c') {
      char v = *data;
      printf("%c\n", v);
      data += 1;
    } else if (*fmt == 's') {
      uint64 ptr = *(uint64*)data;
      if (ptr)
        printf("%s\n", (char*)ptr);
      data += 8;
    } else if (*fmt == 'S') {
      printf("%s\n", data);
      data += strlen(data) + 1;
      break;
    }
    fmt++;
  }
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    printf("Example 1:\n");
    char d1[6] = {0x02,0xf1,0x00,0x00,0xe9,0x07};
    memdump("ih", d1);

    printf("Example 2:\n");
    char *str = "a string";
    memdump("s", (char*)&str);

    printf("Example 3:\n");
    char d3[8];
    strcpy(d3, "another");
    memdump("S", d3);

    printf("Example 4:\n");
    char d4[32];
    *(short*)d4 = 0x4244;
    *(int*)(d4+2) = 1819438967;
    *(short*)(d4+6) = 100;
    d4[8] = 'z';
    strcpy(d4+9, "xyzzy");
    memdump("hic s", d4);

    printf("Example 5:\n");
    char d5[] = "hello";
    memdump("S", d5);
    for (int i = 0; i < strlen(d5); i++)
      memdump("c", d5+i);
  } else {
    char buf[512];
    int n = read(0, buf, sizeof(buf)-1);
    buf[n] = '\0';
    memdump(argv[1], buf);
  }
  exit(0);
}


