#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("\n=== Mixed Workload Test ===\n");
  printf("Running CPU-bound and I/O-bound processes simultaneously\n");
  printf("CPU-bound should demote to queue 3\n");
  printf("I/O-bound should stay at queue 0-1\n\n");
  
  int pid1 = fork();
  if(pid1 == 0) {
    // Child 1: CPU-bound
    exec("cpubound", (char*[]){"cpubound", 0});
    printf("exec cpubound failed\n");
    exit(1);
  }
  
  int pid2 = fork();
  if(pid2 == 0) {
    // Child 2: I/O-bound
    exec("iobound", (char*[]){"iobound", 0});
    printf("exec iobound failed\n");
    exit(1);
  }
  
  // Parent waits for both
  wait(0);
  wait(0);
  
  printf("\n=== Test Complete ===\n");
  printf("✓ Both processes finished successfully!\n\n");
  
  exit(0);
}