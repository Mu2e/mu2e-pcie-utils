// This file (mu2e_init.h) was created by Ron Rechenmacher <ron@fnal.gov> on
// Feb  5, 2014. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

#ifndef MU2E_EVENT_H
#define MU2E_EVENT_H

#include <linux/interrupt.h> /* request_irq */
#include <linux/wait.h>      // wait_queue_head_t
extern wait_queue_head_t get_info_wait_queue;

irqreturn_t DmaInterrupt(int irq, void* dev_id);

int  mu2e_sched_poll(int dtc);
int  mu2e_force_poll(int dtc);
int  mu2e_event_up(int dtc);
void mu2e_event_down(int dtc);

#endif  // MU2E_EVENT_H
