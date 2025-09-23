#include "kernel/types.h"
#include "user/user.h"

int main(void) {
  printf("%d\n", uptime());  // uptime() returns ticks since boot
  exit(0);
}

