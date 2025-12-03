#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("\n=== Starvation Prevention Test ===\n");
  printf("Creating multiple CPU-bound processes\n");
  printf("All should eventually run due to priority boosting\n\n");
  
  // Create 3 CPU-bound processes
  for(int i = 0; i < 3; i++) {
    int pid = fork();
    if(pid == 0) {
      // Child process
      printf("Process %d (PID=%d) starting\n", i, getpid());
      
      struct procinfo info;
      int last_queue = 0;
      
      // Run for a while
      for(long j = 0; j < 500000000; j++) {
        asm volatile("nop");
        
        // Check priority occasionally
        if(j % 100000000 == 0) {
          if(getprocinfo(&info) == 0) {
            // Print when queue changes
            if(info.queue_level != last_queue) {
              printf("  Process %d: queue %d → %d\n", 
                     i, last_queue, info.queue_level);
              last_queue = info.queue_level;
            }
            
            // Check if we got boosted back to 0
            if(last_queue == 3 && info.queue_level == 0) {
              printf("  Process %d: BOOSTED back to queue 0!\n", i);
            }
          }
        }
      }
      
      printf("Process %d finished\n", i);
      exit(0);
    }
  }
  
  // Parent waits for all children
  for(int i = 0; i < 3; i++) {
    wait(0);
  }
  
  printf("\n=== Test Complete ===\n");
  printf("✓ All processes completed (no starvation!)\n\n");
  
  exit(0);
}