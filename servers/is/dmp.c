/* This file contains information dump procedures. During the initialization 
 * of the Information Service 'known' function keys are registered at the TTY
 * server in order to receive a notification if one is pressed. Here, the 
 * corresponding dump procedure is called.  
 *
 * The entry points into this file are
 *   handle_fkey:	handle a function key pressed notification
 */


/* Several header files from the kernel are included here so that we can use
 * the constants, macros, and types needed to make the debugging dumps.
 */
#include "is.h"
#include <timers.h>
#include <string.h>
#include <ibm/interrupt.h>
#include "../../kernel/const.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"
#include "../../kernel/sendmask.h"

#define click_to_round_k(n) \
	((unsigned) ((((unsigned long) (n) << CLICK_SHIFT) + 512) / 1024))


/* Declare some local dump procedures. */
FORWARD _PROTOTYPE( char *proc_name, (int proc_nr)		);
FORWARD _PROTOTYPE( void proctab_dmp, (void)			);
FORWARD _PROTOTYPE( void memmap_dmp, (void)			);
FORWARD _PROTOTYPE( void sendmask_dmp, (void)			);
FORWARD _PROTOTYPE( void image_dmp, (void)			);
FORWARD _PROTOTYPE( void irqtab_dmp, (void)			);
FORWARD _PROTOTYPE( void kmessages_dmp, (void)			);
FORWARD _PROTOTYPE( void diagnostics_dmp, (void)		);
FORWARD _PROTOTYPE( void sched_dmp, (void)			);
FORWARD _PROTOTYPE( void monparams_dmp, (void)			);
FORWARD _PROTOTYPE( void kenv_dmp, (void)			);
FORWARD _PROTOTYPE( void memchunks_dmp, (void)			);

/* Some global data that is shared among several dumping procedures. 
 * Note that the process table copy has the same name as in the kernel
 * so that most macros and definitions from proc.h also apply here.
 */
struct proc proc[NR_TASKS + NR_PROCS];
struct system_image image[IMAGE_SIZE];


/*===========================================================================*
 *				handle_fkey				     *
 *===========================================================================*/
PUBLIC int do_fkey_pressed(message *m)
{
#if DEAD_CODE
    if (F1 <= m->FKEY_CODE && m->FKEY_CODE <= F12) {
        switch(m->FKEY_CODE) {
#else
    if (F1 <= m->NOTIFY_ARG && m->NOTIFY_ARG <= F12) {
        switch(m->NOTIFY_ARG) {
#endif
            case  F1:	proctab_dmp();		break;
            case  F2:	memmap_dmp();		break;
            case  F3:	image_dmp();		break;
            case  F4:	sendmask_dmp();		break;
            case  F5:	monparams_dmp();	break;
            case  F6:	irqtab_dmp();		break;
            case  F7:	kmessages_dmp();	break;
            case  F9:	diagnostics_dmp();	break;
            case F10:	kenv_dmp();		break;
            case F11:	memchunks_dmp();	break;
            case F12:	sched_dmp();		break;
            default: 
#if DEAD_CODE
            	printf("IS: unhandled notification for F%d\n", m->FKEY_NUM);
#else
            	printf("IS: unhandled notify for F%d (code %d)\n", 
            		m->NOTIFY_FLAGS, m->NOTIFY_ARG);
#endif
        }
    }
    return(EDONTREPLY);
}


/*===========================================================================*
 *				diagnostics_dmp				     *
 *===========================================================================*/
PRIVATE void diagnostics_dmp()
{
  char print_buf[DIAG_BUF_SIZE+1];	/* buffer used to print */
  int start;				/* calculate start of messages */
  int size, r;

  /* Try to print the kernel messages. First determine start and copy the
   * buffer into a print-buffer. This is done because the messages in the
   * copy may wrap (the kernel buffer is circular).
   */
  start = ((diag_next + DIAG_BUF_SIZE) - diag_size) % DIAG_BUF_SIZE;
  r = 0;
  size = diag_size;
  while (size > 0) {
  	print_buf[r] = diag_buf[(start+r) % DIAG_BUF_SIZE];
  	r ++;
  	size --;
  }
  print_buf[r] = 0;		/* make sure it terminates */
  printf("Dump of diagnostics from device drivers and servers.\n\n"); 
  printf(print_buf);		/* print the messages */
}


/*===========================================================================*
 *				kmessages_dmp				     *
 *===========================================================================*/
PRIVATE void kmessages_dmp()
{
  struct kmessages kmess;		/* get copy of kernel messages */
  char print_buf[KMESS_BUF_SIZE+1];	/* this one is used to print */
  int next, size;			/* vars returned by sys_kmess() */
  int start;				/* calculate start of messages */
  int r;

  /* Try to get a copy of the kernel messages. */
  if ((r = sys_getkmessages(&kmess)) != OK) {
      report("warning: couldn't get copy of kmessages", r);
      return;
  }

  /* Try to print the kernel messages. First determine start and copy the
   * buffer into a print-buffer. This is done because the messages in the
   * copy may wrap (the kernel buffer is circular).
   */
  start = ((kmess.km_next + KMESS_BUF_SIZE) - kmess.km_size) % KMESS_BUF_SIZE;
  r = 0;
  while (kmess.km_size > 0) {
  	print_buf[r] = kmess.km_buf[(start+r) % KMESS_BUF_SIZE];
  	r ++;
  	kmess.km_size --;
  }
  print_buf[r] = 0;		/* make sure it terminates */
  printf("Dump of all messages generated by the kernel.\n\n"); 
  printf(print_buf);		/* print the messages */
}


/*===========================================================================*
 *				monparams_dmp				     *
 *===========================================================================*/
PRIVATE void monparams_dmp()
{
  char val[1024];
  char *e;
  int r;

  /* Try to get a copy of the boot monitor parameters. */
  if ((r = sys_getmonparams(val, sizeof(val))) != OK) {
      report("warning: couldn't get copy of monitor params", r);
      return;
  }

  /* Append new lines to the result. */
  e = val;
  do {
	e += strlen(e);
	*e++ = '\n';
  } while (*e != 0); 

  /* Finally, print the result. */
  printf("Dump of kernel environment strings set by boot monitor.\n");
  printf("\n%s\n", val);
}


/*===========================================================================*
 *				irqtab_dmp				     *
 *===========================================================================*/
PRIVATE void irqtab_dmp()
{
  int i,j,r;
  struct irq_hook irq_hooks[NR_IRQ_HOOKS];
  struct irq_hook *e;	/* irq tab entry */
  char *irq[] = {
  	"clock",	/* 00 */
  	"keyboard",	/* 01 */
  	"cascade",	/* 02 */
  	"eth/rs232",	/* 03 */
  	"rs232",	/* 04 */
  	"xt_wini",	/* 05 */
  	"floppy",	/* 06 */
  	"printer",	/* 07 */
  	"",	/* 08 */
  	"",	/* 09 */
  	"",	/* 10 */
  	"",	/* 11 */
  	"",	/* 12 */
  	"",	/* 13 */
  	"at_wini_0",	/* 14 */
  	"at_wini_1",	/* 15 */
  };

  if ((r = sys_getirqhooks(irq_hooks)) != OK) {
      report("warning: couldn't get copy of irq hooks", r);
      return;
  }

  printf("IRQ policies dump shows use of kernel's IRQ hooks.\n");
  printf("-h.id- -proc.nr- -IRQ vector (nr.)- -policy- \n");
  for (i=0; i<NR_IRQ_HOOKS; i++) {
  	e = &irq_hooks[i];
  	printf("%3d", i);
  	if (e->proc_nr==NONE) {
  	    printf("    <unused>\n");
  	    continue;
  	}
  	printf("%10d  ", e->proc_nr); 
  	printf("    %9.9s (%02d) ", irq[e->irq], e->irq); 
  	printf("  %s\n", (e->policy & IRQ_REENABLE) ? "reenable" : "-");
  }
  printf("\n");
}


/*===========================================================================*
 *				image_dmp				     *
 *===========================================================================*/
PRIVATE void image_dmp()
{
  int i,j,r;
  struct system_image *ip;
  char maskstr[NR_TASKS + NR_PROCS] = "mask";
  char* types[] = {"task", "system","driver", "server", "user", "idle"};
  char* priorities[] = {"task", "higher","high", "normal", "low", "lower", "user","idle"};
	
  if ((r = sys_getimage(image)) != OK) {
      report("warning: couldn't get copy of image table", r);
      return;
  }
  printf("Image table dump showing all processes included in system image.\n");
  printf("---name-- -nr- --type- -priority- ----pc- -stack- ------sendmask-------\n");
  for (i=0; i<IMAGE_SIZE; i++) { 
      ip = &image[i];
      for (j=-NR_TASKS; j<INIT_PROC_NR+2; j++) 
         maskstr[j+NR_TASKS] = (isallowed(ip->sendmask, j)) ? '1' : '0';
      maskstr[j+NR_TASKS] = '\0';
      printf("%8s %4d %7s %10s %7lu %7lu   %s\n",
          ip->proc_name, ip->proc_nr, types[ip->type], priorities[ip->priority], 
          (long)ip->initial_pc, ip->stksize, maskstr); 
  }
  printf("\n");
}

/*===========================================================================*
 *				sched_dmp    				     *
 *===========================================================================*/
PRIVATE void sched_dmp()
{
  struct proc *rdy_head[NR_SCHED_QUEUES];
  char *types[] = {"task","higher","high","normal","low","lower","user","idle"};
  struct kinfo kinfo;
  register struct proc *rp;
  vir_bytes ptr_diff;
  int r;

  /* First obtain a scheduling information. */
  if ((r = sys_getschedinfo(proc, rdy_head)) != OK) {
      report("warning: couldn't get copy of process table", r);
      return;
  }
  /* Then obtain kernel addresses to correct pointer information. */
  if ((r = sys_getkinfo(&kinfo)) != OK) {
      report("warning: couldn't get kernel addresses", r);
      return;
  }

  /* Update all pointers. Nasty pointer algorithmic ... */
  ptr_diff = (vir_bytes) proc - (vir_bytes) kinfo.proc_addr;
  for (r=0;r<NR_SCHED_QUEUES; r++)
      if (rdy_head[r] != NIL_PROC)
          rdy_head[r] = 
              (struct proc *)((vir_bytes) rdy_head[r] + ptr_diff);
  for (rp=BEG_PROC_ADDR; rp < END_PROC_ADDR; rp++)
      if (rp->p_nextready != NIL_PROC)
          rp->p_nextready =
               (struct proc *)((vir_bytes) rp->p_nextready + ptr_diff);

  /* Now show scheduling queues. */
  printf("Dumping scheduling queues.\n");

  for (r=0;r<NR_SCHED_QUEUES; r++) {
      printf("* %6s: ", types[r]);
      rp = rdy_head[r];
      while (rp != NIL_PROC) {
          printf("%3d, ", rp->p_nr);
          rp = rp->p_nextready;
      }
      printf("NIL\n");
  }
  printf("\n");
}

/*===========================================================================*
 *				kenv_dmp				     *
 *===========================================================================*/
PRIVATE void kenv_dmp()
{
    struct kinfo kinfo;
    struct machine machine;
    int r;
    if ((r = sys_getkinfo(&kinfo)) != OK) {
    	report("warning: couldn't get copy of kernel info struct", r);
    	return;
    }
    if ((r = sys_getmachine(&machine)) != OK) {
    	report("warning: couldn't get copy of kernel machine struct", r);
    	return;
    }

    printf("Dump of kinfo and machine structures.\n\n");
    printf("Machine structure:\n");
    printf("- pc_at:      %3d\n", machine.pc_at); 
    printf("- ps_mca:     %3d\n", machine.ps_mca); 
    printf("- processor:  %3d\n", machine.processor); 
    printf("- protected:  %3d\n", machine.protected); 
    printf("- vdu_ega:    %3d\n", machine.vdu_ega); 
    printf("- vdu_vga:    %3d\n\n", machine.vdu_vga); 
    printf("Kernel info structure:\n");
    printf("- code_base:  %5u\n", kinfo.code_base); 
    printf("- code_size:  %5u\n", kinfo.code_size); 
    printf("- data_base:  %5u\n", kinfo.data_base); 
    printf("- data_size:  %5u\n", kinfo.data_size); 
    printf("- proc_addr:  %5u\n", kinfo.proc_addr); 
    printf("- kmem_base:  %5u\n", kinfo.kmem_base); 
    printf("- kmem_size:  %5u\n", kinfo.kmem_size); 
    printf("- bootdev_base:  %5u\n", kinfo.bootdev_base); 
    printf("- bootdev_size:  %5u\n", kinfo.bootdev_size); 
    printf("- params_base:   %5u\n", kinfo.params_base); 
    printf("- params_size:   %5u\n", kinfo.params_size); 
    printf("- notify_pending:%8u\n", kinfo.nr_ntf_pending); 
    printf("- lock_notify: %6u\n", kinfo.lock_notify); 
    printf("- lock_send:   %6u\n", kinfo.lock_send); 
    printf("- nr_procs:     %3u\n", kinfo.nr_procs); 
    printf("- nr_tasks:     %3u\n", kinfo.nr_tasks); 
    printf("- version:      %.6s\n", kinfo.version); 
    printf("\n");
}

/*===========================================================================*
 *				memchunks_dmp				     *
 *===========================================================================*/
PRIVATE void memchunks_dmp()
{
    int i,r;
    struct memory mem[NR_MEMS];
    if ((r = sys_getmemchunks(mem)) != OK) {
    	report("warning: couldn't get copy of mem chunks", r);
    	return;
    }
	
    printf("Memory chunks:\n");
    for (i=0; i<NR_MEMS; i++) {
      printf("chunk %d: base %u, size %u\n", i, mem[i].base, mem[i].size);
    }
    printf("\n");
}




/*===========================================================================*
 *				sendmask_dmp    				     *
 *===========================================================================*/
PRIVATE void sendmask_dmp()
{
  register struct proc *rp;
  static struct proc *oldrp = BEG_PROC_ADDR;
  int r, i,j, n = 0;

  /* First obtain a fresh copy of the current process table. */
  if ((r = sys_getproctab(proc)) != OK) {
      report("warning: couldn't get copy of process table", r);
      return;
  }

  printf("\n\n");
  printf("Sendmask dump for process table. User processes (*) don't have [].");
  printf("\n");
  printf("The rows of bits indicate to which processes each process may send.");
  printf("\n\n");

  printf("              ");
  for (j=proc_nr(BEG_PROC_ADDR); j< INIT_PROC_NR+1; j++) {
     printf("%3d", j);
  }
  printf("  *\n");

  for (rp = oldrp; rp < END_PROC_ADDR; rp++) {
        if (isemptyp(rp)) continue;
        if (++n > 20) break;

    	printf("%8s ", rp->p_name);
    	j = proc_nr(rp);
	switch(rp->p_type) {
	    case P_IDLE:	printf("/%3d/ ", proc_nr(rp));  break;
	    case P_TASK:	printf("[%3d] ", proc_nr(rp));  break;
	    case P_SYSTEM:	printf("<%3d> ", proc_nr(rp));  break;
	    case P_DRIVER:	printf("{%3d} ", proc_nr(rp));  break;
	    case P_SERVER:	printf("(%3d) ", proc_nr(rp));  break;
	    default: 		printf(" %3d  ", proc_nr(rp));
	}

    	for (j=proc_nr(BEG_PROC_ADDR); j<INIT_PROC_NR+2; j++) {
    	    if (isallowed(rp->p_sendmask, j))	printf(" 1 ");
    	    else 				printf(" 0 ");
    	}
        printf("\n");
  }
  if (rp == END_PROC_ADDR) { printf("\n"); rp = BEG_PROC_ADDR; }
  else printf("--more--\r");
  oldrp = rp;
}



/*===========================================================================*
 *				proctab_dmp    				     *
 *===========================================================================*/
#if (CHIP == INTEL)
PRIVATE void proctab_dmp()
{
/* Proc table dump */

  register struct proc *rp;
  static struct proc *oldrp = BEG_PROC_ADDR;
  int r, n = 0;
  phys_clicks text, data, size;

  /* First obtain a fresh copy of the current process table. */
  if ((r = sys_getproctab(proc)) != OK) {
      report("warning: couldn't get copy of process table", r);
      return;
  }

  printf("\n-pid- -pri- --pc-- --sp-- -user- -sys- -text- -data- -size- -flags- -command-\n");

  for (rp = oldrp; rp < END_PROC_ADDR; rp++) {
	if (isemptyp(rp)) continue;
	if (++n > 23) break;
	text = rp->p_memmap[T].mem_phys;
	data = rp->p_memmap[D].mem_phys;
	size = rp->p_memmap[T].mem_len
		+ ((rp->p_memmap[S].mem_phys + rp->p_memmap[S].mem_len) - data);
	switch(rp->p_type) {
	    case P_IDLE:	printf("/%3d/ ", proc_nr(rp));  break;
	    case P_TASK:	printf("[%3d] ", proc_nr(rp));  break;
	    case P_SYSTEM:	printf("<%3d> ", proc_nr(rp));  break;
	    case P_DRIVER:	printf("{%3d} ", proc_nr(rp));  break;
	    case P_SERVER:	printf("(%3d) ", proc_nr(rp));  break;
	    default: 		printf(" %3d  ", proc_nr(rp));
	}
	printf("%3u %7lx%7lx %6lu%6lu%6uK%6uK%6uK %3x",
	       rp->p_priority,
	       (unsigned long) rp->p_reg.pc,
	       (unsigned long) rp->p_reg.sp,
	       rp->user_time, rp->sys_time,
	       click_to_round_k(text), click_to_round_k(data),
	       click_to_round_k(size),
	       rp->p_flags);
	if (rp->p_flags & RECEIVING) {
		printf(" %-7.7s", proc_name(rp->p_getfrom));
	} else
	if (rp->p_flags & SENDING) {
		printf(" S:%-5.5s", proc_name(rp->p_sendto));
	} else
	if (rp->p_flags == 0) {
		printf("        ");
	}
	printf(" %s\n", rp->p_name);
  }
  if (rp == END_PROC_ADDR) rp = BEG_PROC_ADDR; else printf("--more--\r");
  oldrp = rp;
}
#endif				/* (CHIP == INTEL) */

/*===========================================================================*
 *				memmap_dmp    				     *
 *===========================================================================*/
PRIVATE void memmap_dmp()
{
  register struct proc *rp;
  static struct proc *oldrp = cproc_addr(HARDWARE);
  int r, n = 0;
  phys_clicks size;

  /* First obtain a fresh copy of the current process table. */
  if ((r = sys_getproctab(proc)) != OK) {
      report("warning: couldn't get copy of process table", r);
      return;
  }

  printf("\n--proc name-  -----text-----  -----data-----  ----stack-----  -size-\n");
  for (rp = oldrp; rp < END_PROC_ADDR; rp++) {
	if (isemptyp(rp)) continue;
	if (++n > 23) break;
	size = rp->p_memmap[T].mem_len
		+ ((rp->p_memmap[S].mem_phys + rp->p_memmap[S].mem_len)
						- rp->p_memmap[D].mem_phys);
	printf("%3d %-7.7s  %4x %4x %4x  %4x %4x %4x  %4x %4x %4x  %5uK\n",
	       proc_nr(rp),
	       rp->p_name,
	       rp->p_memmap[T].mem_vir, rp->p_memmap[T].mem_phys, rp->p_memmap[T].mem_len,
	       rp->p_memmap[D].mem_vir, rp->p_memmap[D].mem_phys, rp->p_memmap[D].mem_len,
	       rp->p_memmap[S].mem_vir, rp->p_memmap[S].mem_phys, rp->p_memmap[S].mem_len,
	       click_to_round_k(size));
  }
  if (rp == END_PROC_ADDR) rp = cproc_addr(HARDWARE); 
  else printf("--more--\r");
  oldrp = rp;
}

/*===========================================================================*
 *				proc_name    				     *
 *===========================================================================*/
PRIVATE char *proc_name(proc_nr)
int proc_nr;
{
  if (proc_nr == ANY) return "ANY";
  return cproc_addr(proc_nr)->p_name;
}


