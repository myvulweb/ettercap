/*
    ettercap -- top half and dispatching module

    Copyright (C) ALoR & NaGA

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

    $Header: /home/drizzt/dev/sources/ettercap.cvs/ettercap_ng/src/ec_dispatcher.c,v 1.20 2003/07/16 20:45:30 alor Exp $
*/

#include <ec.h>
#include <ec_threads.h>
#include <ec_hook.h>
#include <ec_stats.h>


/* this is the PO queue from bottom to top half */
struct po_queue_entry {
   struct packet_object *po;
   SIMPLEQ_ENTRY(po_queue_entry) next;
};

static SIMPLEQ_HEAD(, po_queue_entry) po_queue = SIMPLEQ_HEAD_INITIALIZER(po_queue);

/* global mutex on interface */

static pthread_mutex_t po_mutex = PTHREAD_MUTEX_INITIALIZER;
#define PO_QUEUE_LOCK     do{ pthread_mutex_lock(&po_mutex); }while(0)
#define PO_QUEUE_UNLOCK   do{ pthread_mutex_unlock(&po_mutex); }while(0)

/* proto */

void top_half_queue_add(struct packet_object *po);
EC_THREAD_FUNC(top_half);

/*******************************************/

/*
 * top half function
 * it is the dispatcher for the various methods
 * which need to process packet objects 
 * created by the bottom_half (capture).
 * it read the queue created by top_half_queue_add()
 * and deliver the po to all the registered functions
 */

EC_THREAD_FUNC(top_half)
{
   struct po_queue_entry *e;
   u_int pck_len;
   
   DEBUG_MSG("top_half activated !");
  
   /* initialize the thread */
   ec_thread_init();

   LOOP { 
     
      CANCELLATION_POINT();
      
      /* the queue is updated by other thread, lock it */
      PO_QUEUE_LOCK;
      
      /* get the first element */
      e = SIMPLEQ_FIRST(&po_queue);

      /* the queue is empty, nothing to do... */
      if (e == NULL) {
         PO_QUEUE_UNLOCK;
         
         usleep(1);
         continue;
      }
  
      /* start the counter for the TopHalf */
      stats_half_start(&GBL_STATS->th);
       
      /* remove the packet form the queue */
      SIMPLEQ_REMOVE_HEAD(&po_queue, e, next);
     
      /* update the queue stats */
      stats_queue_del();
      
      /* 
       * we have extracted the element, unlock the queue 
       *
       * the bottom half MUST be very fast and it cannot
       * wait on the top half lock.
       */
      PO_QUEUE_UNLOCK;
      
      /* 
       * check if it is the last packet of a file...
       * and exit if we are in console or demonize mode
       */
      if (GBL_UI->type == UI_CONSOLE || GBL_UI->type == UI_DAEMONIZE) {
         if (e->po->flags & PO_EOF) {
            DEBUG_MSG("End of dump file...");
            USER_MSG("\nEnd of dump file...\n");
            clean_exit(0);
         }
      }
      
      /* HOOK_POINT: DISPATCHER */
      hook_point(HOOK_DISPATCHER, e->po);

      /* save the len before the free() */
      pck_len = e->po->len;
      
      /* destroy the duplicate packet object */
      packet_destroy_object(e->po);
      SAFE_FREE(e);
      
      /* start the counter for the TopHalf */
      stats_half_end(&GBL_STATS->th, pck_len);
      
   } 
}

/* 
 * add a packet to the top half queue.
 * this fuction is called by the bottom half thread
 */

void top_half_queue_add(struct packet_object *po)
{
   struct po_queue_entry *e;

   e = calloc(1, sizeof(struct po_queue_entry));
   ON_ERROR(e, NULL, "can't allocate memory");
   
   e->po = packet_dup(po);
   
   PO_QUEUE_LOCK;
   
   /* add the message to the queue */
   SIMPLEQ_INSERT_TAIL(&po_queue, e, next);
   
   /* update the stats */
   stats_queue_add();
   
   PO_QUEUE_UNLOCK;
}


/* EOF */

// vim:ts=3:expandtab

