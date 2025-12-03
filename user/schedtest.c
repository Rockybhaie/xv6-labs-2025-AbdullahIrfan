#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("\n=== MLFQ Scheduler Test ===\n\n");
  
  struct procinfo info;
  
  printf("Initial state:\n");
  getprocinfo(&info);
  printf("  Queue: %d, Quantum: %d, Ticks: %d\n\n", 
         info.queue_level, info.quantum, info.ticks_used);
  
  printf("Doing LONG CPU work to trigger demotion...\n");
  printf("(This will take a while...)\n\n");
  
  // Do ONE massive computation instead of multiple rounds
  // This prevents getprocinfo() from interfering
  for(long i = 0; i < 1000000000; i++) {  // 1 BILLION operations!
    // Just burn CPU
    asm volatile("nop");
    
    // Only check every 100 million iterations
    if(i % 100000000 == 0) {
      getprocinfo(&info);
      printf("After %ld ops: Queue=%d, Ticks=%d\n", 
             i, info.queue_level, info.ticks_used);
    }
  }
  
  printf("\n=== Test Complete ===\n");
  getprocinfo(&info);
  printf("Final: Queue=%d, Ticks=%d\n", info.queue_level, info.ticks_used);
  
  exit(0);
}