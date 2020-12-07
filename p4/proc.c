#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

#include "spinlock.h"

struct {
    struct spinlock lock;
    struct proc proc[NPROC];
    // four priority queues
    int q0[64];
    int q1[64];
    int q2[64];
    int q3[64];
    // index of top priority proc in that queue
    int head3;
    int head2;
    int head1;
    int head0;
    // total num of procs in that queue
    int count0;
    int count1;
    int count2;
    int count3;
} ptable;


static struct proc *initproc;
//added
//static struct pstat *pstat;
int nextpid = 1;

extern void forkret(void);

extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void) {
    initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
    return mycpu() - cpus;
}

int
addElement(int *queue, int *head, int *count, int newoffset) {

    int add = 0;
    for (int i = 0; i < *count; i++){
        if(queue[i] == newoffset){
            return 0;
        }
    }
    // new proc offset will be add to the end of queue
    if (*head == 0) {
        queue[*count] = newoffset;
        add = 1;
    } else {
        for (int j = *count; j > *head; j--) {
            queue[j] = queue[j - 1];
        }
        queue[*head] = newoffset;
        *head = *head + 1;
        add = 1;
    }
    if (add) {
        *count = *count + 1;
        return 1;
    }
    return 0;
}

void
removeElement(int *queue, int *head, int *count, int offset) {

    int remove = 0;
    // new proc offset will be add to the end of queue
    int j = 0;
    for (j = 0; j < *count; j++) {
        if (queue[j] == offset)
            break;
    }
    int i = 0;
    for (i = j; i < (*count); i++) {
        queue[i] = queue[i + 1];
        remove = 1;
    }
    if (remove) {
        *count = *count - 1;
        if (*head == *count) {
            *head = 0;
        }
    }
    return;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void) {
    int apicid, i;

    if (readeflags() & FL_IF)
        panic("mycpu called with interrupts enabled\n");

    apicid = lapicid();
    // APIC IDs are not guaranteed to be contiguous. Maybe we should have
    // a reverse map, or reserve a register to store &cpus[i].
    for (i = 0; i < ncpu; ++i) {
        if (cpus[i].apicid == apicid)
            return &cpus[i];
    }
    panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void) {
    struct cpu *c;
    struct proc *p;
    pushcli();
    c = mycpu();
    p = c->proc;
    popcli();
    return p;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void) {

    struct proc *p;
    char *sp;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == UNUSED)
            goto found;

    release(&ptable.lock);
    return 0;

    found:
    p->state = EMBRYO;
    p->pid = nextpid++;

    release(&ptable.lock);

    // Allocate kernel stack.
    if ((p->kstack = kalloc()) == 0) {
        p->state = UNUSED;
        return 0;
    }
    sp = p->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *p->tf;
    p->tf = (struct trapframe *) sp;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    sp -= 4;
    *(uint *) sp = (uint) trapret;

    sp -= sizeof *p->context;
    p->context = (struct context *) sp;
    memset(p->context, 0, sizeof *p->context);
    p->context->eip = (uint) forkret;

    //adding offset
    p->offset = p - &ptable.proc[0];
    return p;
}

// Set up first user process.
void
userinit(void) {

    struct proc *p;
    extern char _binary_initcode_start[], _binary_initcode_size[];

    ptable.head3 = 0;
    ptable.head2 = 0;
    ptable.head1 = 0;
    ptable.head0 = 0;
    ptable.count3 = 0;
    ptable.count2 = 0;
    ptable.count1 = 0;
    ptable.count0 = 0;

    p = allocproc();

    p->pri = 3;
    p->qtail[3] += 1;

    addElement(&ptable.q3[0], &ptable.head3, &ptable.count3, 0);

    initproc = p;
    if ((p->pgdir = setupkvm()) == 0)
        panic("userinit: out of memory?");
    inituvm(p->pgdir, _binary_initcode_start, (int) _binary_initcode_size);
    p->sz = PGSIZE;
    memset(p->tf, 0, sizeof(*p->tf));
    p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
    p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
    p->tf->es = p->tf->ds;
    p->tf->ss = p->tf->ds;
    p->tf->eflags = FL_IF;
    p->tf->esp = PGSIZE;
    p->tf->eip = 0;  // beginning of initcode.S

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");

    // this assignment to p->state lets other cores
    // run this process. the acquire forces the above
    // writes to be visible, and the lock is also needed
    // because the assignment might not be atomic.
    acquire(&ptable.lock);

    p->state = RUNNABLE;
    release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n) {
    uint sz;
    struct proc *curproc = myproc();

    sz = curproc->sz;
    if (n > 0) {
        if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
            return -1;
    } else if (n < 0) {
        if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
            return -1;
    }
    curproc->sz = sz;
    switchuvm(curproc);
    return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void) {
    struct proc *curproc = myproc();

    return fork2(getpri(curproc->pid));
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void) {
    struct proc *curproc = myproc();
    struct proc *p;
    int fd;

    if (curproc == initproc)
        panic("init exiting");

    // Close all open files.
    for (fd = 0; fd < NOFILE; fd++) {
        if (curproc->ofile[fd]) {
            fileclose(curproc->ofile[fd]);
            curproc->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;

    acquire(&ptable.lock);

    // Parent might be sleeping in wait().
    wakeup1(curproc->parent);

    // Pass abandoned children to init.
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->parent == curproc) {
            p->parent = initproc;
            if (p->state == ZOMBIE)
                wakeup1(initproc);
        }
    }

    // Jump into the scheduler, never to return.
    curproc->state = ZOMBIE;
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void) {
    struct proc *p;
    int havekids, pid;
    struct proc *curproc = myproc();

    acquire(&ptable.lock);
    for (;;) {
        // Scan through table looking for exited children.
        havekids = 0;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->parent != curproc)
                continue;
            havekids = 1;
            if (p->state == ZOMBIE) {
                // Found one.
                pid = p->pid;
                kfree(p->kstack);
                p->kstack = 0;
                freevm(p->pgdir);
                p->pid = 0;
                p->parent = 0;
                p->name[0] = 0;
                p->killed = 0;
                p->state = UNUSED;
                release(&ptable.lock);
                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || curproc->killed) {
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(curproc, &ptable.lock);  //DOC: wait-sleep
    }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void) {
    struct proc *p;
    struct cpu *c = mycpu();
    c->proc = 0;
    // pointer to the queue that will run
    int *currentQ;
    // count of process in current queue
    int *count = 0;
    // how many ticks in a timeslice in current queue
    int tickCount;
    int *head = 0;
    int queue = 0;
    for (;;) {
        // Enable interrupts on this processor.
        sti();
        //break;
        acquire(&ptable.lock);
        struct proc *r;
        int max = -1;
   
        for (r = ptable.proc; r < &ptable.proc[NPROC]; r++) {
            if (r->state != RUNNABLE) {
                switch (r->pri) {
                    case 0:
                        removeElement(ptable.q0, &ptable.head0, &ptable.count0, r->offset);
                        break;
                    case 1:
                        removeElement(ptable.q1, &ptable.head1, &ptable.count1, r->offset);
                        break;
                    case 2:
                        removeElement(ptable.q2, &ptable.head2, &ptable.count2, r->offset);
                        break;
                    case 3:
                        removeElement(ptable.q3, &ptable.head3, &ptable.count3, r->offset);
                        break;
                    default:
                        continue;
                }
                continue;
            }
            if (r->pri > max) {
                max = r->pri;
            }
            switch(r->pri) {
                case 0:
                    addElement(ptable.q0, &ptable.head0, &ptable.count0, r->offset);
                    break;
                case 1:
                    addElement(ptable.q1, &ptable.head1, &ptable.count1, r->offset);
                    break;
                case 2:
                    addElement(ptable.q2, &ptable.head2, &ptable.count2, r->offset);
                    break;
                case 3:
                    addElement(ptable.q3, &ptable.head3, &ptable.count3, r->offset);
                    break;
            }
        }
        switch (max) {
            case 0:
                currentQ = ptable.q0;
                count = &ptable.count0;
                tickCount = 20;
                head = &ptable.head0;
                queue = 0;
                break;
            case 1:
                currentQ = ptable.q1;
                count = &ptable.count1;
                tickCount = 16;
                head = &ptable.head1;
                queue =1;
                break;
            case 2:
                currentQ = ptable.q2;
                count = &ptable.count2;
                tickCount = 12;
                head = &ptable.head2;
                queue =2;
                break;
            case 3:
                currentQ = ptable.q3;
                count = &ptable.count3;
                tickCount = 8;
                head = &ptable.head3;
                queue =3;
                break;
            default :
                release(&ptable.lock);
                continue;
        }
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        int m = 0;
        for (m = 0; m < *count; m++) {
            if (ptable.proc[currentQ[(m+ *head)%(*count)]].state == RUNNABLE)
                break;
        }

        *head = (m + *head)%(*count);

        p = &ptable.proc[currentQ[*head]];
        c->proc = p;

        switchuvm(p);
        p->state = RUNNING;
        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        p->ticks = p->ticks + 1;
        p->totalTicks[queue] += 1;

        // ?
        if (p->ticks == tickCount) {
            p->ticks = 0;
            if(p->state == RUNNABLE){
            *head += 1;
            p->qtail[queue] += 1;
            if (*head == *count) {
                *head = 0;
            }
            }
        }
        release(&ptable.lock);
    }
}


// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void) {
    int intena;
    struct proc *p = myproc();

    if (!holding(&ptable.lock))
        panic("sched ptable.lock");
    if (mycpu()->ncli != 1)
        panic("sched locks");
    if (p->state == RUNNING)
        panic("sched running");
    if (readeflags() & FL_IF)
        panic("sched interruptible");
    intena = mycpu()->intena;
    swtch(&p->context, mycpu()->scheduler);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void) {
    acquire(&ptable.lock);  //DOC: yieldlock
    myproc()->state = RUNNABLE;
    sched();
    release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void) {
    static int first = 1;
    // Still holding ptable.lock from scheduler.
    release(&ptable.lock);

    if (first) {
        // Some initialization functions must be run in the context
        // of a regular process (e.g., they call sleep), and thus cannot
        // be run from main().
        first = 0;
        iinit(ROOTDEV);
        initlog(ROOTDEV);
    }

    // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk) {
    struct proc *p = myproc();

    if (p == 0)
        panic("sleep");

    if (lk == 0)
        panic("sleep without lk");

    // Must acquire ptable.lock in order to
    // change p->state and then call sched.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.
    if (lk != &ptable.lock) {  //DOC: sleeplock0
        acquire(&ptable.lock);  //DOC: sleeplock1
        release(lk);
    }
    // Go to sleep.
    p->chan = chan;
    p->state = SLEEPING;

    sched();

    // Tidy up.
    p->chan = 0;

    // Reacquire original lock.
    if (lk != &ptable.lock) {  //DOC: sleeplock2
        release(&ptable.lock);
        acquire(lk);
    }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan) {
    struct proc *p;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == SLEEPING && p->chan == chan){
            p->state = RUNNABLE;
            p->qtail[p->pri]+=1;
            }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan) {
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid) {
    struct proc *p;
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            p->killed = 1;
            // Wake process from sleep if necessary.
            if (p->state == SLEEPING)
                p->state = RUNNABLE;
            release(&ptable.lock);
            return 0;
        }
    }
    release(&ptable.lock);
    return -1;
}
//This sets the priority of the specified PID to pri.
//You should check that both PID and pri are valid; if they are not, return -1.
//When the priority of a process is set, the process should go to the end of the queue at that level
//and should be given a new time-slice of the correct length.
//The priority of a process could be increased, decreased,
//or not changed (in other words, even when the priority of a process
// is set to its current priority, that process should still be moved
//to the end of its queue and given a new timeslice).

//Note that calling setpri() may cause a new process to have
//the highest priority in the system and thus need to be scheduled when the next timer tick occurs.

// (When testing your pinfo() statistics below, we will not examine how you account
// for the case when setpri() is applied to the calling process.)
int 
setpri(int PID, int pri) {
    //cprintf("pri %d\n", pri);
    //cprintf("PID %d\n", PID);
    if (pri < 0 || pri > 3) {
        return -1;
    } 
     //cprintf("set pri\n");
    acquire(&ptable.lock);
     //cprintf("post set pri\n");
    struct proc *p;
    
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    //cprintf("old %d\n",p->pri);
        if (p->pid == PID) {
            
            // delete it from original queue array
            switch (p->pri) {
                case 0:
                    removeElement(&ptable.q0[0], &ptable.head0, &ptable.count0, p->offset);
                    break;
                case 1:
                    removeElement(&ptable.q1[0], &ptable.head1, &ptable.count1, p->offset);
                    break;
                case 2:
                    removeElement(&ptable.q2[0], &ptable.head2, &ptable.count2, p->offset);
                    break;
                case 3:
                    removeElement(&ptable.q3[0], &ptable.head3, &ptable.count3, p->offset);
                    break;
            }
            
            // set to new pri
            //cprintf("old %d\n",p->pri);
            p->pri = pri;
            //cprintf("new %d\n",p->pri);
            p->qtail[p->pri] += 1;
            // add it to new queue array
            switch (p->pri) {
                case 0:
                    p->ticks=0;
                    addElement(&ptable.q0[0], &ptable.head0, &ptable.count0, p->offset);
                    break;
                case 1:
                    p->ticks=0;
                    addElement(&ptable.q1[0], &ptable.head1, &ptable.count1, p->offset);
                    break;
                case 2:
                    p->ticks=0;
                    addElement(&ptable.q2[0], &ptable.head2, &ptable.count2, p->offset);
                    break;
                case 3:
                    p->ticks=0;
                    addElement(&ptable.q3[0], &ptable.head3, &ptable.count3, p->offset);
                    break;
            }
            release(&ptable.lock);
            return 0;
        }
    }
    release(&ptable.lock);
    return -1;

}

//This returns the current priority of the specified PID.  If the PID is not valid, it returns -1.
int 
getpri(int PID) {
    struct proc *p;
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == PID) {
            int pri = p->pri;
            release(&ptable.lock);
            return pri;
        }
    }
    release(&ptable.lock);
    return -1;

}

// copy from fork
int fork2(int pri) {

    int i, pid;
    struct proc *np;
    struct proc *curproc = myproc();
    if (pri < 0 || pri > 3) {
        return -1;
    }
    // Allocate process.
    if ((np = allocproc()) == 0) {
        return -1;
    }

    // Copy process state from proc.
    if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0) {
        kfree(np->kstack);
        np->kstack = 0;
        np->state = UNUSED;
        return -1;
    }
    np->sz = curproc->sz;
    np->parent = curproc;


    *np->tf = *curproc->tf;

    // Clear %eax so that fork returns 0 in the child.
    np->tf->eax = 0;

    for (i = 0; i < NOFILE; i++)
        if (curproc->ofile[i])
            np->ofile[i] = filedup(curproc->ofile[i]);
    np->cwd = idup(curproc->cwd);

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    pid = np->pid;

    acquire(&ptable.lock);

    np->state = RUNNABLE;
    np->totalTicks[0] = 0;
    np->totalTicks[1] = 0;
    np->totalTicks[2] = 0;
    np->totalTicks[3] = 0;
    // adding priority
    np->pri = pri;
    np->ticks = 0;
    np->qtail[np->pri] = 1;
    switch (np->pri) {

        case 0:
            addElement(&ptable.q0[0], &ptable.head0, &ptable.count0, np->offset);
            break;
        case 1:
            addElement(&ptable.q1[0], &ptable.head1, &ptable.count1, np->offset);
            break;
        case 2:
            addElement(&ptable.q2[0], &ptable.head2, &ptable.count2, np->offset);
            break;
        case 3:
            addElement(&ptable.q3[0], &ptable.head3, &ptable.count3, np->offset);
            break;
    }
    release(&ptable.lock);
    return pid;
}

//get each process info
int getpinfo(struct pstat *pstat) {
    if (pstat == 0) { return -1; }
    struct proc *p;
    acquire(&ptable.lock);
    int i = 0;
    //int ticks[NPROC][4];  // total num ticks each process has accumulated at each priority
    //  int qtail[NPROC][4];// total num times moved to tail of this queue (e.g., setprio, end of timeslice, waking)
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED) {
            pstat->inuse[i] = 0;
        } else {
            pstat->inuse[i] = 1;
        }
        pstat->pid[i] = p->pid;
        pstat->priority[i] = p->pri;
        pstat->state[i] = p->state;       
        pstat->ticks[i][0] = p->totalTicks[0];
        pstat->ticks[i][1] = p->totalTicks[1];
        pstat->ticks[i][2] = p->totalTicks[2];
        pstat->ticks[i][3] = p->totalTicks[3];
        pstat->qtail[i][0] = p->qtail[0];
        pstat->qtail[i][1] = p->qtail[1];
        pstat->qtail[i][2] = p->qtail[2];
        pstat->qtail[i][3] = p->qtail[3];            
        
        i++;
    }
    release(&ptable.lock);
    return -1;

}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void) {
    static char *states[] = {
            [UNUSED]    "unused",
            [EMBRYO]    "embryo",
            [SLEEPING]  "sleep ",
            [RUNNABLE]  "runble",
            [RUNNING]   "run   ",
            [ZOMBIE]    "zombie"
    };
    int i;
    struct proc *p;
    char *state;
    uint pc[10];

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        cprintf("%d %s %s", p->pid, state, p->name);
        if (p->state == SLEEPING) {
            getcallerpcs((uint *) p->context->ebp + 2, pc);
            for (i = 0; i < 10 && pc[i] != 0; i++)
                cprintf(" %p", pc[i]);
        }
        cprintf("\n");
    }
}

