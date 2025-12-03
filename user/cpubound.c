#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("CPU-bound process starting (PID: %d)\n", getpid());
  printf("This will run pure CPU work and should demote to queue 3\n\n");
  
  struct procinfo info;
  
  // MUCH LONGER loop - same as schedtest
  for(long i = 0; i < 1000000000; i++) {
    // Pure CPU work
    asm volatile("nop");
    
    // Check every 200 million iterations
    if(i % 200000000 == 0) {
      if(getprocinfo(&info) == 0) {
        printf("[CPU] After %ld ops: queue=%d, quantum=%d\n", 
               i, info.queue_level, info.quantum);
      }
    }
  }
  
  if(getprocinfo(&info) == 0) {
    printf("\n[CPU] DONE: Final queue=%d\n", info.queue_level);
    if(info.queue_level == 3) {
      printf("✓ SUCCESS: CPU-bound process at lowest priority!\n");
    }
  }
  
  exit(0);
}