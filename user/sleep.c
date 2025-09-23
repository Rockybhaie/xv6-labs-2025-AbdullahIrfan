#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){ 
    printf("Usage: sleep <ticks>\n");
    exit(1);
  }

  
  if(argv[1][0] == '-'){
    printf("sleep: ticks must be non-negative\n");
    exit(1);
  }

  int ticks = atoi(argv[1]); 

  printf("Sleeping for %d ticks...\n", ticks);
  pause(ticks);   
  printf("Done sleeping!\n");

  exit(0);
}



