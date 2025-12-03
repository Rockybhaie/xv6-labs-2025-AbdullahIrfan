#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"


#define MLFQ_LEVELS 4
#define BOOST_INTERVAL 200

#define MLFQ_DEBUG 1           // Master debug flag
#define MLFQ_VERBOSE_TICKS 0   // 0 = summary only, 1 = all ticks


// Queue structure for MLFQ
struct mlfq_queue {
  struct proc *head;           // First process in queue
  struct proc *tail;           // Last process in queue
};

// The 4 priority queues
static struct mlfq_queue mlfq[MLFQ_LEVELS];

// Global boost counter
uint boost_counter = 0;

// Time quantum for each level
static int time_quanta[MLFQ_LEVELS] = {4, 8, 16, 32};

extern char initcode[];
extern uint initcode_sz;

// functon declarations
int get_quantum(int level);
void mlfq_enqueue(struct proc *p, int level);
struct proc* mlfq_dequeue(int level);
void mlfq_remove(struct proc *p);
struct proc* mlfq_next(void);
void mlfq_demote(struct proc *p);
void mlfq_boost(void);





struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }

  // ===== Initialize MLFQ Queues =====
  for(int i = 0; i < MLFQ_LEVELS; i++) {
    mlfq[i].head = 0;
    mlfq[i].tail = 0;
  }
  printf("MLFQ queues initialized\n");  // ADD DEBUG
  // ==================================
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  //Initialize MLFQ Fields
  p->queue_level = 0;           // Start at highest priority
  p->ticks_used = 0;            // No ticks used yet
  p->quantum = time_quanta[0];  // Get quantum for level 0 (4 ticks)
  p->next_proc = 0;             // Not in any queue yet
  

  
  

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// Set up first user process.
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  p = allocproc();
  initproc = p;
  
  p->cwd = namei("/");
  p->state = RUNNABLE;
  
  // ===== ADD THIS LINE =====
  mlfq_enqueue(p, 0);
  // =========================
  
  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if(sz + n > TRAPFRAME) {
      return -1;
    }
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  // Add new process to highest priority queue
  mlfq_enqueue(np, 0);
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
kexit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}


//MLFQ Helper Functions
// Get quantum for a given queue level
int
get_quantum(int level)
{
  if(level < 0 || level >= MLFQ_LEVELS)
    return time_quanta[MLFQ_LEVELS - 1];
  return time_quanta[level];
}


// Add process to end of queue at given level
void
mlfq_enqueue(struct proc *p, int level)
{
  if(level < 0 || level >= MLFQ_LEVELS)
    return;
  
  p->next_proc = 0;  // This will be the last process
  
  if(mlfq[level].tail) {
    // Queue not empty, add to end
    mlfq[level].tail->next_proc = p;
    mlfq[level].tail = p;
  } else {
    // Queue empty, this is first process
    mlfq[level].head = p;
    mlfq[level].tail = p;
  }
}

// Remove and return first process from queue at given level
struct proc*
mlfq_dequeue(int level)
{
  if(level < 0 || level >= MLFQ_LEVELS)
    return 0;
  
  struct proc *p = mlfq[level].head;
  if(p) {
    mlfq[level].head = p->next_proc;
    if(mlfq[level].head == 0) {
      // Queue now empty
      mlfq[level].tail = 0;
    }
    p->next_proc = 0;
  }
  return p;
}


// Remove a specific process from its queue
void
mlfq_remove(struct proc *p)
{
  int level = p->queue_level;
  if(level < 0 || level >= MLFQ_LEVELS)
    return;
  
  struct proc *curr = mlfq[level].head;
  struct proc *prev = 0;
  
  // Find the process in the queue
  while(curr) {
    if(curr == p) {
      // Found it
      if(prev) {
        prev->next_proc = curr->next_proc;
      } else {
        // Removing head
        mlfq[level].head = curr->next_proc;
      }
      
      // Update tail if necessary
      if(mlfq[level].tail == p) {
        mlfq[level].tail = prev;
      }
      
      p->next_proc = 0;
      return;
    }
    prev = curr;
    curr = curr->next_proc;
  }
}


// Get next runnable process from highest priority queue
struct proc*
mlfq_next(void)
{
  struct proc *p;
  
  // Try each level from highest to lowest
  for(int level = 0; level < MLFQ_LEVELS; level++) {
    p = mlfq_dequeue(level);
    if(p) {
      return p;
    }
  }
  
  return 0;  // No runnable process
}


// Demote process to next lower level
void
mlfq_demote(struct proc *p)
{
  if(p->queue_level < MLFQ_LEVELS - 1) {
    p->queue_level++;
    p->quantum = get_quantum(p->queue_level);
  }
  p->ticks_used = 0;  // Reset tick counter
}


// Boost all processes to highest priority
// Boost all processes to highest priority
void
mlfq_boost(void)
{
  struct proc *p;
  
  // Go through all processes
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    
    if(p->state == RUNNABLE || p->state == RUNNING) {
      // Remove from current queue if RUNNABLE
      if(p->state == RUNNABLE) {
        mlfq_remove(p);
      }
      
      // Reset to highest priority
      p->queue_level = 0;
      p->ticks_used = 0;
      p->quantum = get_quantum(0);
      
      // Re-enqueue if RUNNABLE
      if(p->state == RUNNABLE) {
        mlfq_enqueue(p, 0);
      }
    }
    
    release(&p->lock);
  }
}


// Debug function to print MLFQ state
void
mlfq_print_queues(void)
{
  printf("=== MLFQ Queue State ===\n");
  for(int level = 0; level < MLFQ_LEVELS; level++) {
    printf("Queue %d (quantum=%d): ", level, time_quanta[level]);
    
    struct proc *p = mlfq[level].head;
    if(p == 0) {
      printf("empty\n");
    } else {
      while(p != 0) {
        printf("%s(%d) ", p->name, p->pid);
        p = p->next_proc;
      }
      printf("\n");
    }
  }
  printf("======================\n");
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.


//new scheduler implementation
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    // Get next process from highest priority queue
    p = mlfq_next();
    
    if(p != 0) {
      // Found a runnable process
      acquire(&p->lock);
      
      if(p->state == RUNNABLE) {
        // Switch to chosen process
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now
        c->proc = 0;
      }
      
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched RUNNING");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
//updated yield func
// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  
  // Check if process used full quantum (demotion needed)
  if(p->ticks_used >= p->quantum) {
    #if MLFQ_DEBUG
    int old_queue = p->queue_level;
    #endif
    
    mlfq_demote(p);
    
    #if MLFQ_DEBUG
    // Only show queue transitions
    if(old_queue != p->queue_level) {
      printf("[MLFQ] PID=%d: queue %d → %d (quantum now %d)\n", 
             p->pid, old_queue, p->queue_level, p->quantum);
    }
    #endif
  }
  
  p->state = RUNNABLE;
  mlfq_enqueue(p, p->queue_level);
  
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  // Still holding p->lock from scheduler.
  release(&p->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();

    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = kexec("/init", (char *[]){ "/init", 0 });
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  // return to user space, mimicing usertrap()'s return.
  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// Sleep on channel chan, releasing condition lock lk.
// Re-acquires lk when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  // Reset ticks but KEEP priority (reward I/O behavior)
  p->ticks_used = 0;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
      // Re-enqueue at CURRENT priority (no demotion for I/O)
      mlfq_enqueue(p, p->queue_level);
    }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kkill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep if needed.
        p->state = RUNNABLE;
        mlfq_enqueue(p, p->queue_level);
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
