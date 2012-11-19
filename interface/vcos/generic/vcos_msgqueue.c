/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_msgqueue.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

typedef struct VCOS_ENDPOINT_WAITER_T
{
   const char *name;
   VCOS_SEMAPHORE_T sem;
   struct VCOS_ENDPOINT_WAITER_T *next;
} VCOS_ENDPOINT_WAITER_T;

static VCOS_MUTEX_T lock;
static VCOS_MSG_ENDPOINT_T *endpoints;
static VCOS_ENDPOINT_WAITER_T *waiters;
static VCOS_TLS_KEY_T tls_key;

static VCOS_STATUS_T vcos_msgq_create(VCOS_MSGQUEUE_T *q)
{
   VCOS_STATUS_T st;

   memset(q, 0, sizeof(*q));

   st = vcos_semaphore_create(&q->sem,"vcos:msgqueue",0);
   if (st == VCOS_SUCCESS)
      st = vcos_mutex_create(&q->lock,"vcos:msgqueue");
   return st;
}

static void vcos_msgq_delete(VCOS_MSGQUEUE_T *q)
{
   vcos_semaphore_delete(&q->sem);
   vcos_mutex_delete(&q->lock);
}

/* append a message to a message queue */
static void msgq_append(VCOS_MSGQUEUE_T *q, VCOS_MSG_T *msg)
{
   vcos_mutex_lock(&q->lock);
   if (q->head == NULL)
   {
      q->head = q->tail = msg;
   }
   else
   {
      q->tail->next = msg;
      q->tail = msg;
   }
   vcos_mutex_unlock(&q->lock);
}

/* initialise this library */

VCOS_STATUS_T vcos_msgq_init(void)
{
   VCOS_STATUS_T st = vcos_mutex_create(&lock,"msgq");
   if (st != VCOS_SUCCESS)
      goto fail_mtx;

   st = vcos_tls_create(&tls_key);
   if (st != VCOS_SUCCESS)
      goto fail_tls;

   endpoints = NULL;
   return st;

fail_tls:
   vcos_mutex_delete(&lock);
fail_mtx:
   return st;
}

void vcos_msgq_deinit(void)
{
   vcos_mutex_delete(&lock);
   vcos_tls_delete(tls_key);
}

/* find a message queue, optionally blocking until it is created */
static VCOS_MSGQUEUE_T *vcos_msgq_find_helper(const char *name, int wait)
{
   VCOS_MSG_ENDPOINT_T *ep;
   VCOS_ENDPOINT_WAITER_T waiter;
   do
   {
      vcos_mutex_lock(&lock);
      for (ep = endpoints; ep != NULL; ep = ep->next)
      {
         if (strcmp(ep->name, name) == 0)
         {
            /* found it - return to caller */
            vcos_mutex_unlock(&lock);
            return &ep->primary;
         }
      }

      /* if we get here, we did not find it */
      if (!wait)
      {
         vcos_mutex_unlock(&lock);
         return NULL;
      }
      else
      {
         VCOS_STATUS_T st;
         waiter.name = name;
         st = vcos_semaphore_create(&waiter.sem,"vcos:waiter",0);
         if (st != VCOS_SUCCESS)
         {
            vcos_assert(0);
            vcos_mutex_unlock(&lock);
            return NULL;
         }

         /* Push new waiter onto head of global waiters list */
         /* coverity[use] Suppress ATOMICITY warning - 'waiters' might have
          * changed since the last iteration of the loop, which is okay */
         waiter.next = waiters;
         waiters = &waiter;

         /* we're now on the list, so can safely go to sleep */
         vcos_mutex_unlock(&lock);

         /* coverity[lock] This is a semaphore, not a mutex */
         vcos_semaphore_wait(&waiter.sem);
         /* It should now be on the list, but it could in theory be deleted
          * between waking up and going to look for it. So may have to wait
          * again.
          * So, go round again....
          */
         vcos_semaphore_delete(&waiter.sem);
         continue;
      }
   } while (1);
}

VCOS_MSGQUEUE_T *vcos_msgq_find(const char *name)
{
   return vcos_msgq_find_helper(name,0);
}
  

VCOS_MSGQUEUE_T *vcos_msgq_wait(const char *name)
{
   return vcos_msgq_find_helper(name,1);
}

static _VCOS_INLINE
void vcos_msg_send_helper(VCOS_MSGQUEUE_T *src,
                           VCOS_MSGQUEUE_T *dest,
                           uint32_t code,
                           VCOS_MSG_T *msg)
{
   vcos_assert(msg);
   vcos_assert(dest);

   msg->code = code;
   msg->dst = dest;
   msg->src = src;
   msg->next = NULL;
   msg->src_thread = vcos_thread_current();

   msgq_append(dest, msg);
   vcos_semaphore_post(&dest->sem);
}

/* wait on a queue for a message */
static _VCOS_INLINE
VCOS_MSG_T *vcos_msg_wait_helper(VCOS_MSGQUEUE_T *queue)
{
   VCOS_MSG_T *msg;
   vcos_semaphore_wait(&queue->sem);
   vcos_mutex_lock(&queue->lock);

   msg = queue->head;
   vcos_assert(msg);    /* should always be a message here! */

   queue->head = msg->next;
   if (queue->head == NULL)
      queue->tail = NULL;

   vcos_mutex_unlock(&queue->lock);
   return msg;
}

/* peek on a queue for a message */
static _VCOS_INLINE
VCOS_MSG_T *vcos_msg_peek_helper(VCOS_MSGQUEUE_T *queue)
{
   VCOS_MSG_T *msg;
   vcos_mutex_lock(&queue->lock);

   msg = queue->head;

   /* if there's a message, remove it from the queue */
   if (msg)
   {
      queue->head = msg->next;
      if (queue->head == NULL)
         queue->tail = NULL;
   }

   vcos_mutex_unlock(&queue->lock);
   return msg;
}

void vcos_msg_send(VCOS_MSGQUEUE_T *dest, uint32_t code, VCOS_MSG_T *msg)
{
   vcos_msg_send_helper(NULL, dest, code, msg);
}

/* send on to the target queue, then wait on our secondary queue for the reply */
void vcos_msg_sendwait(VCOS_MSGQUEUE_T *dest, uint32_t code, VCOS_MSG_T *msg)
{
   VCOS_MSG_ENDPOINT_T *self = (VCOS_MSG_ENDPOINT_T *)vcos_tls_get(tls_key);
   VCOS_MSG_T *reply;
   vcos_assert(self);
   vcos_msg_send_helper(&self->secondary, dest, code, msg);
   reply = vcos_msg_wait_helper(&self->secondary);

   /* If this fires, then the caller is sending and waiting for multiple things at the same time,
    * somehow. In that case you need a state machine.
    */
   vcos_assert(reply == msg);

   (void)reply;
}

/** Wait for a message
  */
VCOS_MSG_T *vcos_msg_wait(void)
{
   VCOS_MSG_ENDPOINT_T *self = (VCOS_MSG_ENDPOINT_T *)vcos_tls_get(tls_key);
   vcos_assert(self);
   return vcos_msg_wait_helper(&self->primary);
}

/** Peek for a message
  */
VCOS_MSG_T *vcos_msg_peek(void)
{
   VCOS_MSG_ENDPOINT_T *self = (VCOS_MSG_ENDPOINT_T *)vcos_tls_get(tls_key);
   vcos_assert(self);   
   return vcos_msg_peek_helper(&self->primary);
}

/* wait for a specific message, on the secondary interface
 * FIXME: does this need to do the nasty waking-up-and-walking-the-queue trick?
 */
VCOS_MSG_T * vcos_msg_wait_specific(VCOS_MSGQUEUE_T *queue, VCOS_MSG_T *msg)
{
   VCOS_MSG_ENDPOINT_T *self = (VCOS_MSG_ENDPOINT_T *)vcos_tls_get(tls_key);
   VCOS_MSG_T *reply;
   (void)queue;
   vcos_assert(self);
   reply = vcos_msg_wait_helper(&self->secondary);
   vcos_assert(reply == msg); /* if this fires, someone is playing fast and loose with sendwait */
   (void)msg;
   return reply;
}

/** Send a reply to a message
  */
void vcos_msg_reply(VCOS_MSG_T *msg)
{
   VCOS_MSGQUEUE_T *src = msg->src;
   VCOS_MSGQUEUE_T *dst = msg->dst;
   vcos_msg_send_helper(dst, src, msg->code | VCOS_MSG_REPLY_BIT, msg);
}


/** Create an endpoint. Each thread should need no more than one of these - if you 
  * find yourself needing a second one, you've done something wrong.
  */
VCOS_STATUS_T vcos_msgq_endpoint_create(VCOS_MSG_ENDPOINT_T *ep, const char *name)
{
   VCOS_STATUS_T st;
   if (strlen(name) > sizeof(ep->name)-1)
      return VCOS_EINVAL;
   vcos_snprintf(ep->name, sizeof(ep->name), "%s", name);

   st = vcos_msgq_create(&ep->primary);
   if (st == VCOS_SUCCESS)
   {
      st = vcos_msgq_create(&ep->secondary);
   }

   if (st != VCOS_SUCCESS)
   {
      vcos_msgq_delete(&ep->primary);
   }
   else
   {
      VCOS_ENDPOINT_WAITER_T **pwaiter;
      vcos_mutex_lock(&lock);
      ep->next = endpoints;
      endpoints = ep;
      vcos_tls_set(tls_key,ep);

      /* is anyone waiting for this endpoint to come into existence? */
      for (pwaiter = &waiters; *pwaiter != NULL;)
      {
         VCOS_ENDPOINT_WAITER_T *waiter = *pwaiter;
         if (vcos_strcasecmp(waiter->name, name) == 0)
         {
            *pwaiter = waiter->next;
            vcos_semaphore_post(&waiter->sem);
         }
         else
            pwaiter = &(*pwaiter)->next;
      }
      vcos_mutex_unlock(&lock);
   }
   return st;
}

/** Destroy an endpoint.
  */
void  vcos_msgq_endpoint_delete(VCOS_MSG_ENDPOINT_T *ep)
{
   VCOS_MSG_ENDPOINT_T **pep;
   vcos_tls_set(tls_key, NULL);

   vcos_mutex_lock(&lock);

   pep = &endpoints;

   while ((*pep) != ep)
   {
      pep = & (*pep)->next;
   }
   *pep = ep->next;

   vcos_msgq_delete(&ep->primary);
   vcos_msgq_delete(&ep->secondary);

   vcos_mutex_unlock(&lock);
}



