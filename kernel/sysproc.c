#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
// #include "printf.c"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  backtrace(); // ADDED BY SAFEGUARD TASK 2
  return 0;
}

/* ADDED BY SAFEGUARD */
uint64 sys_sleep(void) {
  int duration;
  uint start_ticks;

  argint(0, &duration);
  if (duration < 0)
    duration = 0;

  acquire(&tickslock);
  start_ticks = ticks;

  for (;;) {
    if (ticks - start_ticks >= duration)
      break;

    struct proc *p = myproc();
    if (killed(p)) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }

  printf("Calling backtrace from sys_sleep\n");
  backtrace();

  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

/* ADDED BY SAFEGUARD */
uint64 sys_sigalarm(void) {
  uint64 fn_ptr;
  int time_gap;

  argint(0, &time_gap);
  argaddr(1, &fn_ptr);

  struct proc *p = myproc();

  if (time_gap == 0) {
    if (p->alarm_trapframe) {
      kfree(p->alarm_trapframe);
      p->alarm_trapframe = 0;
    }
    p->alarm_interval = 0;
    p->alarm_handler  = 0;
    p->alarm_ticks    = 0;
    p->alarm_enabled  = 0;
    return 0;
  }

  if (time_gap < 0)
    return -1;

  p->alarm_interval = time_gap;
  p->alarm_handler  = (void(*)())fn_ptr;
  p->alarm_ticks    = time_gap;
  p->alarm_enabled  = 1;

  return 0;
}

/* ADDED BY SAFEGUARD */
uint64 sys_sigreturn(void) {
  struct proc *proc_ptr = myproc();

  if (proc_ptr->alarm_trapframe == 0)
    return -1;
  *(proc_ptr->trapframe) = *(proc_ptr->alarm_trapframe);

  kfree(proc_ptr->alarm_trapframe);
  proc_ptr->alarm_enabled = 1;
  proc_ptr->alarm_ticks   = proc_ptr->alarm_interval;
  proc_ptr->alarm_trapframe = 0;

  return proc_ptr->trapframe->a0;
}