#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    printf("Usage: execdemo program [args...]\n");
    exit(1);
  }

  // Replace current process with the program given in argv[1]
  // Pass the rest of argv as arguments to that program
  exec(argv[1], &argv[1]);

  // If exec fails, this line will run
  printf("exec %s failed\n", argv[1]);
  exit(1);
}

