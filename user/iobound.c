#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("I/O-bound process starting (PID: %d)\n", getpid());
  printf("This will frequently sleep and should stay at queue 0\n\n");
  
  struct procinfo info;
  
  // More iterations to see the effect
  for(int i = 0; i < 100; i++) {
    // Simulate I/O wait - sleep frequently
    sleep(10);
    
    // Check every 10 iterations
    if(i % 10 == 0) {
      if(getprocinfo(&info) == 0) {
        printf("[I/O] Iteration %d: queue=%d, quantum=%d\n", 
               i, info.queue_level, info.quantum);
      }
    }
  }
  
  if(getprocinfo(&info) == 0) {
    printf("\n[I/O] DONE: Final queue=%d\n", info.queue_level);
    if(info.queue_level <= 1) {
      printf("✓ SUCCESS: I/O-bound process stayed at high priority!\n");
    }
  }
  
  exit(0);
}