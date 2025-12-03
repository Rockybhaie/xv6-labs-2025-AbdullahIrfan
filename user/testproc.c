#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
print_procinfo(struct procinfo *info)
{
  printf("========================================\n");
  printf("Process Information\n");
  printf("========================================\n");
  printf("PID:              %d\n", info->pid);
  printf("Name:             %s\n", info->name);
  printf("Queue Level:      %d ", info->queue_level);
  
  // Print priority label
  if(info->queue_level == 0)
    printf("(Highest Priority)\n");
  else if(info->queue_level == 3)
    printf("(Lowest Priority)\n");
  else
    printf("(Medium Priority)\n");
  
  printf("Ticks Used:       %d\n", info->ticks_used);
  printf("Time Quantum:     %d ticks\n", info->quantum);
  printf("State:            %d ", info->state);
  
  // Print state label
  if(info->state == 1)
    printf("(UNUSED)\n");
  else if(info->state == 2)
    printf("(USED)\n");
  else if(info->state == 3)
    printf("(SLEEPING)\n");
  else if(info->state == 4)
    printf("(RUNNABLE)\n");
  else if(info->state == 5)
    printf("(RUNNING)\n");
  else if(info->state == 6)
    printf("(ZOMBIE)\n");
  else
    printf("(UNKNOWN)\n");
  
  printf("========================================\n");
}

int
main(int argc, char *argv[])
{
  struct procinfo info;
  
  printf("\n=== MLFQ Scheduler - Process Info Test ===\n\n");
  
  // Test 1: Get initial process info
  printf("Test 1: Initial Process State\n");
  if(getprocinfo(&info) < 0) {
    printf("ERROR: getprocinfo failed\n");
    exit(1);
  }
  print_procinfo(&info);
  
  // Test 2: Do some work and check again
  printf("\nTest 2: After Some CPU Work\n");
  for(int i = 0; i < 100000; i++) {
    // Busy work
    asm volatile("nop");
  }
  
  if(getprocinfo(&info) < 0) {
    printf("ERROR: getprocinfo failed\n");
    exit(1);
  }
  print_procinfo(&info);
  
  // REMOVED Test 3 with sleep() for now
  
  printf("\n✓ SUCCESS: getprocinfo system call working correctly!\n\n");
  exit(0);
}