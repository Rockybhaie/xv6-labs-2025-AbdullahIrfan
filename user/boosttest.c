#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("\n=== Priority Boosting Test ===\n");
  printf("Running long CPU work to see boosting in action\n");
  printf("Process should demote to queue 3, then boost back to queue 0\n\n");
  
  struct procinfo info;
  int boost_count = 0;
  int last_queue = 0;
  
  // Run for a LONG time to see multiple boosts
  for(long i = 0; i < 2000000000; i++) {  // 2 billion!
    asm volatile("nop");
    
    // Check every 100 million ops
    if(i % 100000000 == 0) {
      if(getprocinfo(&info) == 0) {
        // Detect boost (went from queue 3 back to queue 0)
        if(last_queue == 3 && info.queue_level == 0) {
          boost_count++;
          printf("[BOOST #%d] Process boosted from queue 3 → 0!\n", boost_count);
        }
        
        // Print queue changes
        if(info.queue_level != last_queue) {
          printf("  Queue: %d → %d\n", last_queue, info.queue_level);
          last_queue = info.queue_level;
        }
      }
    }
  }
  
  printf("\n=== Test Complete ===\n");
  printf("Total boosts observed: %d\n", boost_count);
  
  if(boost_count > 0) {
    printf("✓ SUCCESS: Priority boosting is working!\n");
  } else {
    printf("✗ FAILED: No boosts detected\n");
  }
  
  exit(0);
}