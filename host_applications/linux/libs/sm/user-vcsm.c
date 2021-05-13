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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>


#include <vmcs_sm_ioctl.h>
#include "vc_sm_cma_ioctl.h"
#include "dma-buf.h"
#include "user-vcsm.h"
#include "interface/vcos/vcos.h"

typedef struct
{
   VCSM_CACHE_TYPE_T cur;  /* Current pattern. */
   VCSM_CACHE_TYPE_T new;  /* New pattern. */
   VCSM_CACHE_TYPE_T res;  /* End result. */

} VCSM_CACHE_MUTEX_LKUP_T;

// /dev/vcsm-cma is the new device that is using CMA and importing it to the VPU.
#define VCSM_CMA_DEVICE_NAME  "/dev/vcsm-cma"
// /dev/vcsm is the older driver that maps gpu_mem into the ARM.
#define VCSM_DEVICE_NAME      "/dev/vcsm"
#define VCSM_INVALID_HANDLE   (-1)

static VCOS_LOG_CAT_T usrvcsm_log_category;
#define VCOS_LOG_CATEGORY (&usrvcsm_log_category)
static int using_vc_sm_cma = 0;
static int vcsm_handle = VCSM_INVALID_HANDLE;
static int vcsm_refcount;
static unsigned int vcsm_page_size = 0;

static VCOS_ONCE_T vcsm_once = VCOS_ONCE_INIT;
static VCOS_MUTEX_T vcsm_mutex;

#define VCSM_PAYLOAD_ELEM_MAX 512

typedef struct VCSM_PAYLOAD_ELEM_T
{
   unsigned int handle;    // User handle
   int fd;                 // vcsm-cma / dmabuf fd (= user handle-1)
   uint32_t vc_handle;     // VPU reloc heap handle
   uint8_t *mem;           // mmap'ed address
   unsigned int size;      // size of mmap
   uint32_t dma_addr;      // VPU address for the buffer
   int in_use;
} VCSM_PAYLOAD_ELEM_T;

typedef struct VCSM_PAYLOAD_LIST_T
{
   VCSM_PAYLOAD_ELEM_T list[VCSM_PAYLOAD_ELEM_MAX];
   VCOS_MUTEX_T lock;
   struct VCSM_PAYLOAD_LIST_T *next;
} VCSM_PAYLOAD_LIST_T;

static VCSM_PAYLOAD_LIST_T vcsm_payload_list;

static void vcsm_payload_list_init(void)
{
   vcos_mutex_create(&vcsm_payload_list.lock, "vcsm_payload_list");
}

static VCSM_PAYLOAD_ELEM_T *vcsm_payload_list_get()
{
   VCSM_PAYLOAD_ELEM_T *elem = 0;
   unsigned int i;

   vcos_mutex_lock(&vcsm_payload_list.lock);
   for (i = 0; i < VCSM_PAYLOAD_ELEM_MAX; i++)
   {
      if (vcsm_payload_list.list[i].in_use)
         continue;
      elem = &vcsm_payload_list.list[i];
      elem->in_use = 1;
      break;
   }
   vcos_mutex_unlock(&vcsm_payload_list.lock);

   return elem;
}

static void vcsm_payload_list_release(VCSM_PAYLOAD_ELEM_T *elem)
{
   vcos_mutex_lock(&vcsm_payload_list.lock);
   elem->handle = elem->vc_handle = elem->fd = 0;
   elem->mem = NULL;
   elem->in_use = 0;
   vcos_mutex_unlock(&vcsm_payload_list.lock);
}

static VCSM_PAYLOAD_ELEM_T *vcsm_payload_list_find_mem(void *mem)
{
   VCSM_PAYLOAD_ELEM_T *elem = 0;
   unsigned int i;

   vcos_mutex_lock(&vcsm_payload_list.lock);
   for (i = 0; i < VCSM_PAYLOAD_ELEM_MAX; i++)
   {
      if (!vcsm_payload_list.list[i].in_use)
         continue;
      if (vcsm_payload_list.list[i].mem != mem)
         continue;
      elem = &vcsm_payload_list.list[i];
      break;
   }
   vcos_mutex_unlock(&vcsm_payload_list.lock);

   return elem;
}

static VCSM_PAYLOAD_ELEM_T *vcsm_payload_list_find_handle(unsigned int handle)
{
   VCSM_PAYLOAD_ELEM_T *elem = 0;
   unsigned int i;

   vcos_mutex_lock(&vcsm_payload_list.lock);
   for (i = 0; i < VCSM_PAYLOAD_ELEM_MAX; i++)
   {
      if (!vcsm_payload_list.list[i].in_use)
         continue;
      if (vcsm_payload_list.list[i].handle != handle)
         continue;
      elem = &vcsm_payload_list.list[i];
      break;
   }
   vcos_mutex_unlock(&vcsm_payload_list.lock);

   return elem;
}

/*static VCSM_PAYLOAD_ELEM_T *vcsm_payload_list_find_vc_handle(uint32_t vc_handle)
{
   VCSM_PAYLOAD_ELEM_T *elem = 0;
   unsigned int i;

   vcos_mutex_lock(&vcsm_payload_list.lock);
   for (i = 0; i < VCSM_PAYLOAD_ELEM_MAX; i++)
   {
      if (!vcsm_payload_list.list[i].in_use)
         continue;
      if (vcsm_payload_list.list[i].vc_handle != vc_handle)
         continue;
      elem = &vcsm_payload_list.list[i];
      break;
   }
   vcos_mutex_unlock(&vcsm_payload_list.lock);

   return elem;
}*/


/* Cache [(current, new) -> outcome] mapping table, ignoring identity.
**
** Note: Videocore cache mode cannot be udpated 'lock' time.
*/
static VCSM_CACHE_MUTEX_LKUP_T vcsm_cache_mutex_table[] =
{
     /* ------ CURRENT ------- *//* ---------- NEW --------- *//* --------- RESULT --------- */
   { VCSM_CACHE_TYPE_NONE,       VCSM_CACHE_TYPE_HOST,         VCSM_CACHE_TYPE_HOST },
   { VCSM_CACHE_TYPE_NONE,       VCSM_CACHE_TYPE_VC,           VCSM_CACHE_TYPE_NONE },
   { VCSM_CACHE_TYPE_NONE,       VCSM_CACHE_TYPE_HOST_AND_VC,  VCSM_CACHE_TYPE_HOST },

   { VCSM_CACHE_TYPE_HOST,       VCSM_CACHE_TYPE_NONE,         VCSM_CACHE_TYPE_NONE },
   { VCSM_CACHE_TYPE_HOST,       VCSM_CACHE_TYPE_VC,           VCSM_CACHE_TYPE_HOST },
   { VCSM_CACHE_TYPE_HOST,       VCSM_CACHE_TYPE_HOST_AND_VC,  VCSM_CACHE_TYPE_HOST },

   { VCSM_CACHE_TYPE_VC,         VCSM_CACHE_TYPE_NONE,         VCSM_CACHE_TYPE_NONE },
   { VCSM_CACHE_TYPE_VC,         VCSM_CACHE_TYPE_HOST,         VCSM_CACHE_TYPE_HOST_AND_VC },
   { VCSM_CACHE_TYPE_VC,         VCSM_CACHE_TYPE_HOST_AND_VC,  VCSM_CACHE_TYPE_HOST_AND_VC },

   { VCSM_CACHE_TYPE_HOST_AND_VC, VCSM_CACHE_TYPE_NONE,        VCSM_CACHE_TYPE_VC },
   { VCSM_CACHE_TYPE_HOST_AND_VC, VCSM_CACHE_TYPE_HOST,        VCSM_CACHE_TYPE_HOST_AND_VC },
   { VCSM_CACHE_TYPE_HOST_AND_VC, VCSM_CACHE_TYPE_VC,          VCSM_CACHE_TYPE_VC },

   /* Used for lookup termination. */
   { VCSM_CACHE_TYPE_NONE,        VCSM_CACHE_TYPE_NONE,        VCSM_CACHE_TYPE_NONE },
};

static VCSM_CACHE_TYPE_T vcsm_cache_table_lookup( VCSM_CACHE_TYPE_T current,
                                                  VCSM_CACHE_TYPE_T new )
{
   VCSM_CACHE_MUTEX_LKUP_T *p_map = vcsm_cache_mutex_table;   

   while ( !( (p_map->cur == VCSM_CACHE_TYPE_NONE) &&
              (p_map->new == VCSM_CACHE_TYPE_NONE) ) )
   {
      if ( (p_map->cur == current) && (p_map->new == new) )
      {
         return p_map->res;
      }

      p_map++;
   };

   vcos_log_error( "[%s]: [%d]: no mapping found for current %d - new %d",
                   __func__,
                   getpid(),
                   current,
                   new );
   return current;
}

/* A one off vcsm initialization routine
*/
static void vcsm_init_once(void)
{
   vcos_mutex_create(&vcsm_mutex, VCOS_FUNCTION);
   vcos_log_set_level(&usrvcsm_log_category, VCOS_LOG_ERROR);
   usrvcsm_log_category.flags.want_prefix = 0;
   vcos_log_register( "usrvcsm", &usrvcsm_log_category );
   vcsm_payload_list_init();
}


/* Initialize the vcsm processing.
**
** Must be called once before attempting to do anything else.
**
** Returns 0 on success, -1 on error.
*/
int vcsm_init_ex( int want_export, int fd )
{
   int result  = VCSM_INVALID_HANDLE;
   vcos_once(&vcsm_once, vcsm_init_once);

   /* Only open the VCSM device once per process.
   */
   vcos_mutex_lock( &vcsm_mutex );
   if ( vcsm_refcount != 0 )
   {
      // Note that in this case the external fd is ignored.

      if (want_export && !using_vc_sm_cma)
      {
         vcos_log_trace( "[%s]: fail as already open and export not available",
                        __func__);
         vcos_mutex_unlock( &vcsm_mutex );
         return -1;
      }
      goto out; /* VCSM already opened. Nothing to do. */
   }

   if (fd != -1)
   {
      vcsm_handle = dup(fd);

      // FIXME: Sanity check which device that the fd actually relates to.
      // For now we have to guess based on whether export is requested.
      // (the main use case is from Chromium which will be requesting export).
      if (want_export)
         using_vc_sm_cma = 1;

      goto out;
   }

   if (want_export)
   {
      vcsm_handle = open( VCSM_CMA_DEVICE_NAME, O_RDWR, 0 );

      if (vcsm_handle >= 0)
      {
         using_vc_sm_cma = 1;
         vcos_log_trace( "[%s]: Using vc-sm-cma, handle %d",
                        __func__, vcsm_handle);
      }
   }

   if (vcsm_handle < 0)
   {
      vcos_log_trace( "[%s]: NOT using vc-sm-cma as handle was %d",
                      __func__, vcsm_handle);
      vcsm_handle = open( VCSM_DEVICE_NAME, O_RDWR, 0 );
   }

   if (vcsm_handle < 0 && !want_export)
   {
      // vcsm failed and not tried vcsm-cma yet.
      vcsm_handle = open( VCSM_CMA_DEVICE_NAME, O_RDWR, 0 );

      if (vcsm_handle >= 0)
      {
         using_vc_sm_cma = 1;
         vcos_log_trace( "[%s]: Using vc-sm-cma, handle %d",
                        __func__, vcsm_handle);
      }
   }

out:
   if ( vcsm_handle >= 0 )
   {
      vcsm_page_size = getpagesize();

      result = 0;
      vcsm_refcount++;

      vcos_log_trace( "[%s]: [%d]: %d (align: %u) - ref-cnt %u",
                      __func__,
                      getpid(),
                      vcsm_handle,
                      vcsm_page_size,
                      vcsm_refcount );
   }
   vcos_mutex_unlock( &vcsm_mutex );
   return result;
}

/* Initialize the vcsm processing.
**
** Must be called once before attempting to do anything else.
**
** Returns 0 on success, -1 on error.
*/
int vcsm_init( void )
{
  return vcsm_init_ex(0, -1);
}

/* Terminates the vcsm processing.
**
** Must be called vcsm services are no longer needed, it will
** take care of removing any allocation under the current process
** control if deemed necessary.
*/
void vcsm_exit( void )
{
   vcos_mutex_lock( &vcsm_mutex );

   if ( vcsm_refcount == 0 )
   {
      goto out; /* Shouldn't really happen. */
   }

   if ( --vcsm_refcount != 0 )
   {
      vcos_log_trace( "[%s]: [%d]: %d - ref-cnt: %u",
                      __func__,
                      getpid(),
                      vcsm_handle,
                      vcsm_refcount );

      goto out; /* We're done. */
   }

   close( vcsm_handle );
   vcsm_handle = VCSM_INVALID_HANDLE;

out:
   vcos_mutex_unlock( &vcsm_mutex );
}


/* Allocates a cached block of memory of size 'size' via the vcsm memory
** allocator, the type of caching requested is passed as argument of the
** function call.
**
** Returns:        0 on error
**                 a non-zero opaque handle on success.
**
** On success, the user must invoke vcsm_lock with the returned opaque
** handle to gain access to the memory associated with the opaque handle.
** When finished using the memory, the user calls vcsm_unlock_xx (see those
** function definition for more details on the one that can be used).
** 
** A well behaved application should make every attempt to lock/unlock
** only for the duration it needs to access the memory data associated with
** the opaque handle.
*/
unsigned int vcsm_malloc_cache( unsigned int size, VCSM_CACHE_TYPE_T cache, const char *name )
{
   unsigned int size_aligned = size;
   void *usr_ptr = NULL;
   int rc;
   unsigned int handle = VCSM_INVALID_HANDLE;

   if ( (size == 0) || (vcsm_handle == VCSM_INVALID_HANDLE) ) 
   {
      vcos_log_error( "[%s]: [%d] [%s]: NULL size or invalid device!",
                      __func__,
                      getpid(),
                      name );
      return 0;
   }

   /* Ask for page aligned.
   */
   size_aligned = (size + vcsm_page_size - 1) & ~(vcsm_page_size - 1);

   if (using_vc_sm_cma)
   {
      struct vc_sm_cma_ioctl_alloc alloc;
      VCSM_PAYLOAD_ELEM_T *payload;

      memset( &alloc, 0, sizeof(alloc));

      alloc.size   = size_aligned;
      alloc.num    = 1;
      alloc.cached = (enum vmcs_sm_cache_e) cache;   /* Convenient one to one mapping. */
      alloc.handle = 0;
      if ( name != NULL )
      {
         memcpy ( alloc.name, name, 32 );
      }
      rc = ioctl( vcsm_handle,
                  VC_SM_CMA_IOCTL_MEM_ALLOC,
                  &alloc );

      if ( rc < 0 || alloc.handle < 0 )
      {
         vcos_log_error( "[%s]: [%d] [%s]: ioctl mem-alloc FAILED [%d] (hdl: %x)",
                         __func__,
                         getpid(),
                         alloc.name,
                         rc,
                         alloc.handle );
         return 0;
      }

      vcos_log_trace( "[%s]: [%d] [%s]: ioctl mem-alloc %d (hdl: %x)",
                      __func__,
                      getpid(),
                      alloc.name,
                      rc,
                      alloc.handle );

      /* Map the buffer into user space.
      */
      usr_ptr = mmap( 0,
                      alloc.size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      alloc.handle,
                      0 );

      if ( usr_ptr == MAP_FAILED )
      {
         vcos_log_error( "[%s]: [%d]: mmap FAILED (hdl: %x)",
                      __func__,
                      getpid(),
                      alloc.handle );
         vcsm_free( alloc.handle );
         return 0;
      }

      // vc-sm-cma now hands out file handles (signed int), whilst libvcsm is
      // handling unsigned int handles. Already checked the handle >=0, so
      // add one to make it a usable handle.
      handle = alloc.handle + 1;

      vcos_log_trace( "[%s]: mmap to %p",
                      __func__,
                      usr_ptr
                      );

      payload = vcsm_payload_list_get();
      if (!payload)
      {
         vcos_log_error("[%s]: max number of allocations reached: %d", __func__, VCSM_PAYLOAD_ELEM_MAX);
         munmap(usr_ptr, alloc.size);
         vcsm_free(alloc.handle);
         return 0;
      }
      payload->handle = handle;
      payload->fd = alloc.handle;
      payload->vc_handle = alloc.vc_handle;
      payload->mem = usr_ptr;
      payload->size = size_aligned;
      if (alloc.dma_addr & 0xFFFFFFFF00000000ULL)
      {
         vcos_log_error("[%s]: dma address returned > 32bit 0x%llx", __func__, alloc.dma_addr);
         payload->dma_addr = 0;
      }
      else
         payload->dma_addr = (uint32_t)alloc.dma_addr;
   }
   else
   {
      struct vmcs_sm_ioctl_alloc alloc;

      memset( &alloc, 0, sizeof(alloc));
      /* Allocate the buffer on videocore via the VCSM (Videocore Shared Memory)
      ** interface.
      */
      alloc.size   = size_aligned;
      alloc.num    = 1;
      alloc.cached = (enum vmcs_sm_cache_e) cache;   /* Convenient one to one mapping. */
      alloc.handle = 0;
      if ( name != NULL )
      {
         memcpy ( alloc.name, name, 32 );
      }
      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MEM_ALLOC,
                  &alloc );

      if ( rc < 0 || alloc.handle == 0 )
      {
         vcos_log_error( "[%s]: [%d] [%s]: ioctl mem-alloc FAILED [%d] (hdl: %x)",
                         __func__,
                         getpid(),
                         alloc.name,
                         rc,
                         alloc.handle );
         return 0;
      }

      vcos_log_trace( "[%s]: [%d] [%s]: ioctl mem-alloc %d (hdl: %x)",
                      __func__,
                      getpid(),
                      alloc.name,
                      rc,
                      alloc.handle );

      /* Map the buffer into user space.
      */
      usr_ptr = mmap( 0,
                      alloc.size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      vcsm_handle,
                      alloc.handle );

      if ( usr_ptr == NULL )
      {
         vcos_log_error( "[%s]: [%d]: mmap FAILED (hdl: %x)",
                      __func__,
                      getpid(),
                      alloc.handle );
         vcsm_free( alloc.handle );
         return 0;
      }

      handle = alloc.handle;
   }

   return handle;
}


/* Allocates a non-cached block of memory of size 'size' via the vcsm memory
** allocator.
**
** Returns:        0 on error
**                 a non-zero opaque handle on success.
**
** On success, the user must invoke vcsm_lock with the returned opaque
** handle to gain access to the memory associated with the opaque handle.
** When finished using the memory, the user calls vcsm_unlock_xx (see those
** function definition for more details on the one that can be used).
** 
** A well behaved application should make every attempt to lock/unlock
** only for the duration it needs to access the memory data associated with
** the opaque handle.
*/
unsigned int vcsm_malloc( unsigned int size, const char *name )
{
   return vcsm_malloc_cache( size, VCSM_CACHE_TYPE_NONE, name );
}

/* Shares an allocated block of memory.
**
** Returns:        0 on error
**                 a non-zero opaque handle on success.
**
** On success, the user must invoke vcsm_lock with the returned opaque
** handle to gain access to the memory associated with the opaque handle.
** When finished using the memory, the user calls vcsm_unlock_xx (see those
** function definition for more details on the one that can be used).
**
** A well behaved application should make every attempt to lock/unlock
** only for the duration it needs to access the memory data associated with
** the opaque handle.
*/
unsigned int vcsm_malloc_share( unsigned int handle )
{
   struct vmcs_sm_ioctl_alloc_share alloc;
   int rc;

   if ( vcsm_handle == VCSM_INVALID_HANDLE )
   {
      vcos_log_error( "[%s]: [%d]: NULL size or invalid device!",
                      __func__,
                      getpid() );
      return 0;
   }

   if (using_vc_sm_cma)
   {
      return 0;
   }

   memset( &alloc, 0, sizeof(alloc) );

   /* Share the buffer on videocore via the VCSM (Videocore Shared Memory)
   ** interface.
   */
   alloc.handle = handle;
   rc = ioctl( vcsm_handle,
               VMCS_SM_IOCTL_MEM_ALLOC_SHARE,
               &alloc );

   if ( rc < 0 || alloc.handle == 0 )
   {
      vcos_log_error( "[%s]: [%d]: ioctl mem-share FAILED [%d] (hdl: %x->%x)",
                      __func__,
                      getpid(),
                      rc,
                      handle,
                      alloc.handle );
      goto error;
   }

   vcos_log_trace( "[%s]: [%d]: ioctl mem-share %d (hdl: %x->%x)",
                   __func__,
                   getpid(),
                   rc,
                   handle,
                   alloc.handle );

   return alloc.handle;

 error:
   if ( alloc.handle )
   {
      vcsm_free( alloc.handle );
   }
   return 0;
}

/* Frees a block of memory that was successfully allocated by
** a prior call the vcms_alloc.
**
** The handle should be considered invalid upon return from this
** call.
**
** Whether any memory is actually freed up or not as the result of
** this call will depends on many factors, if all goes well it will
** be freed.  If something goes wrong, the memory will likely end up
** being freed up as part of the vcsm_exit process.  In the end the
** memory is guaranteed to be freed one way or another.
*/
void vcsm_free( unsigned int handle )
{
   int rc;
   void *usr_ptr = NULL;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (handle == 0) ) 
   {
      vcos_log_error( "[%s]: [%d]: invalid device or handle!",
                      __func__,
                      getpid() );

      goto out;
   }

   if (using_vc_sm_cma)
   {
      VCSM_PAYLOAD_ELEM_T *elem;

      elem = vcsm_payload_list_find_handle(handle);

      if (!elem)
      {
         vcos_log_trace( "[%s]: handle %u not tracked, or not mapped. elem %p\n",
                      __func__, handle, elem);
         goto out;
      }

      rc = munmap( elem->mem, elem->size );

      vcos_log_trace( "[%s]: ioctl unmap fd: %d, addr %p, size %u. rc %d",
                      __func__,
                      elem->fd,
                      elem->mem,
                      elem->size,
                      rc
                        );

      close(elem->fd);

      vcsm_payload_list_release(elem);
   }
   else
   {
      struct vmcs_sm_ioctl_free alloc_free;
      struct vmcs_sm_ioctl_size sz;
      struct vmcs_sm_ioctl_map map;

      memset( &sz, 0, sizeof(sz) );
      memset( &alloc_free, 0, sizeof(alloc_free) );
      memset( &map, 0, sizeof(map) );

      /* Verify what we want is valid.
      */
      sz.handle = handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_SIZE_USR_HDL,
                  &sz );

      vcos_log_trace( "[%s]: [%d]: ioctl size-usr-hdl %d (hdl: %x) - size %u",
                      __func__,
                      getpid(),
                      rc,
                      sz.handle,
                      sz.size );

      /* We will not be able to free up the resource!
      **
      ** However, the driver will take care of it eventually once the device is
      ** closed (or dies), so this is not such a dramatic event...
      */
      if ( (rc < 0) || (sz.size == 0) )
      {
         goto out;
      }

      /* Un-map the buffer from user space, using the last known mapped
      ** address valid.
      */
      usr_ptr = (void *) vcsm_usr_address( sz.handle );
      if ( usr_ptr != NULL )
      {
         munmap( usr_ptr, sz.size );

         vcos_log_trace( "[%s]: [%d]: ioctl unmap hdl: %x",
                         __func__,
                         getpid(),
                         sz.handle );
      }
      else
      {
         vcos_log_trace( "[%s]: [%d]: freeing unmapped area (hdl: %x)",
                         __func__,
                         getpid(),
                         map.handle );
      }

      /* Free the allocated buffer all the way through videocore.
      */
      alloc_free.handle    = sz.handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MEM_FREE,
                  &alloc_free );

      vcos_log_trace( "[%s]: [%d]: ioctl mem-free %d (hdl: %x)",
                      __func__,
                      getpid(),
                      rc,
                      alloc_free.handle );
   }

out:
   return;
}


/* Queries the status of the the vcsm.
**
** Triggers dump of various kind of information, see the
** different variants specified in VCSM_STATUS_T.
**
** Pid is optional.
*/
void vcsm_status( VCSM_STATUS_T status, int pid )
{
   struct vmcs_sm_ioctl_walk walk;

   if ( vcsm_handle == VCSM_INVALID_HANDLE ) 
   {
      vcos_log_error( "[%s]: [%d]: invalid device!",
                      __func__,
                      getpid() );

      return;
   }

   if (using_vc_sm_cma)
   {
      return;
   }

   memset( &walk, 0, sizeof(walk) );

   /* Allow user to specify the pid of interest if desired, otherwise
   ** assume the current one.
   */
   walk.pid  = (pid == VCSM_INVALID_HANDLE) ? getpid() : pid;

   switch ( status )
   {
      case VCSM_STATUS_VC_WALK_ALLOC:
      {
         ioctl( vcsm_handle,
                VMCS_SM_IOCTL_VC_WALK_ALLOC,
                NULL );
      }
      break;

      case VCSM_STATUS_HOST_WALK_MAP:
      {
         ioctl( vcsm_handle,
                VMCS_SM_IOCTL_HOST_WALK_MAP,
                NULL );
      }
      break;

      case VCSM_STATUS_HOST_WALK_PID_MAP:
      {
         ioctl( vcsm_handle,
                VMCS_SM_IOCTL_HOST_WALK_PID_MAP,
                &walk );
      }
      break;

      case VCSM_STATUS_HOST_WALK_PID_ALLOC:
      {
         ioctl( vcsm_handle,
                VMCS_SM_IOCTL_HOST_WALK_PID_ALLOC,
                &walk );
      }
      break;

      case VCSM_STATUS_NONE:
      default:
         vcos_log_error( "[%s]: [%d]: invalid argument %d",
                         __func__,
                         getpid(),
                         status );
      break;
   }
}


/* Retrieves a videocore opaque handle from a mapped user address
** pointer.  The videocore handle will correspond to the actual
** memory mapped in videocore.
**
** Returns:        0 on error
**                 a non-zero opaque handle on success.
**
** Note: the videocore opaque handle is distinct from the user
**       opaque handle (allocated via vcsm_malloc) and it is only
**       significant for such application which knows what to do
**       with it, for the others it is just a number with little
**       use since nothing can be done with it (in particular
**       for safety reason it cannot be used to map anything).
*/
unsigned int vcsm_vc_hdl_from_ptr( void *usr_ptr )
{
   int rc;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (usr_ptr == NULL) )
   {
      vcos_log_error( "[%s]: [%d]: invalid device!",
                      __func__,
                      getpid() );

      return 0;
   }

   if (using_vc_sm_cma)
   {
      VCSM_PAYLOAD_ELEM_T *elem;

      elem = vcsm_payload_list_find_mem(usr_ptr);

      if (!elem)
      {
         vcos_log_trace( "[%s]: addr %p not tracked, or not mapped. elem %p\n",
                      __func__, usr_ptr, elem);
         return 0;
      }
      return elem->vc_handle;
   }
   else
   {
      struct vmcs_sm_ioctl_map map;

      memset( &map, 0, sizeof(map) );

      map.pid  = getpid();
      map.addr = (uintptr_t) usr_ptr;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MAP_VC_HDL_FR_ADDR,
                  &map );

      if ( rc < 0 )
      {
         vcos_log_error( "[%s]: [%d]: ioctl mapped-usr-hdl FAILED [%d] (pid: %d, addr: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.pid,
                         map.addr );

         return 0;
      }
      else
      {
         vcos_log_trace( "[%s]: [%d]: ioctl mapped-usr-hdl %d (hdl: %x, addr: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.handle,
                         map.addr );

         return map.handle;
      }
   }
}


/* Retrieves a videocore opaque handle from a opaque handle
** pointer.  The videocore handle will correspond to the actual
** memory mapped in videocore.
**
** Returns:        0 on error
**                 a non-zero opaque handle on success.
**
** Note: the videocore opaque handle is distinct from the user
**       opaque handle (allocated via vcsm_malloc) and it is only
**       significant for such application which knows what to do
**       with it, for the others it is just a number with little
**       use since nothing can be done with it (in particular
**       for safety reason it cannot be used to map anything).
*/
unsigned int vcsm_vc_hdl_from_hdl( unsigned int handle )
{
   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (handle == 0) )
   {
      vcos_log_error( "[%s]: [%d]: invalid device or handle!",
                      __func__,
                      getpid() );

      return 0;
   }

   if (using_vc_sm_cma)
   {
      VCSM_PAYLOAD_ELEM_T *elem;

      elem = vcsm_payload_list_find_handle(handle);

      if (!elem)
      {
         vcos_log_trace( "[%s]: handle %u not tracked, or not mapped. \n",
                      __func__, handle);
         return 0;
      }
      return elem->vc_handle;
   }
   else
   {
      int rc;
      struct vmcs_sm_ioctl_map map;

      memset( &map, 0, sizeof(map) );

      map.pid    = getpid();
      map.handle = handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MAP_VC_HDL_FR_HDL,
                  &map );

      if ( rc < 0 )
      {
         vcos_log_error( "[%s]: [%d]: ioctl mapped-usr-hdl FAILED [%d] (pid: %d, hdl: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.pid,
                         map.handle );

         return 0;
      }
      else
      {
         vcos_log_trace( "[%s]: [%d]: ioctl mapped-usr-hdl %d (hdl: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.handle );

         return map.handle;
      }
   }
}


/* Retrieves a videocore (bus) address from a opaque handle
** pointer.
**
** Returns:        0 on error
**                 a non-zero videocore address on success.
*/
unsigned int vcsm_vc_addr_from_hdl( unsigned int handle )
{
   int rc;
   struct vmcs_sm_ioctl_map map;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (handle == 0) )
   {
      vcos_log_error( "[%s]: [%d]: invalid device or handle!",
                      __func__,
                      getpid() );

      return 0;
   }

   if (using_vc_sm_cma)
   {
      // The API is broken here if we're looking for 64bit support.
      // Need to return a dma_addr_t instead, and the value is platform
      // dependent.
      // Admittedly VideoCore is only 32-bit, so there could be an
      // implementation returning a VPU bus address which would fit in an
      // unsigned int. TODO.
      VCSM_PAYLOAD_ELEM_T *elem;

      elem = vcsm_payload_list_find_handle(handle);

      if (!elem)
      {
         vcos_log_trace( "[%s]: handle %u not tracked, or not mapped. \n",
                      __func__, handle);
         return 0;
      }
      return elem->dma_addr;
   }
   else
   {
      memset( &map, 0, sizeof(map) );

      map.pid    = getpid();
      map.handle = handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MAP_VC_ADDR_FR_HDL,
                  &map );

      if ( rc < 0 )
      {
         vcos_log_error( "[%s]: [%d]: ioctl mapped-usr-hdl FAILED [%d] (pid: %d, hdl: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.pid,
                         map.handle );

         return 0;
      }
      else
      {
         vcos_log_trace( "[%s]: [%d]: ioctl mapped-usr-hdl %d (hdl: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.handle );

         return map.addr;
      }
   }
}


/* Retrieves a mapped user address from an opaque user
** handle.
**
** Returns:        0 on error
**                 a non-zero address on success.
**
** On success, the address corresponds to the pointer
** which can access the data allocated via the vcsm_malloc
** call.
*/
void *vcsm_usr_address( unsigned int handle )
{
   int rc;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (handle == 0) ) 
   {
      vcos_log_error( "[%s]: [%d]: invalid device or handle!",
                      __func__,
                      getpid() );

      return NULL;
   }

   if (using_vc_sm_cma)
   {
      //No need to lock the buffer, but need to retrieve the user address
      VCSM_PAYLOAD_ELEM_T *elem;

      elem = vcsm_payload_list_find_handle(handle);

      if (!elem || !elem->mem)
      {
         vcos_log_trace( "[%s]: handle %u not tracked, or not mapped. elem %p\n",
                      __func__, handle, elem);
         return NULL;
      }

      return elem->mem;
   }
   else
   {
      struct vmcs_sm_ioctl_map map;
      memset( &map, 0, sizeof(map) );

      map.pid    = getpid();
      map.handle = handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MAP_USR_ADDRESS,
                  &map );

      if ( rc < 0 )
      {
         vcos_log_error( "[%s]: [%d]: ioctl mapped-usr-address FAILED [%d] (pid: %d, addr: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.pid,
                         map.addr );

         return NULL;
      }
      else
      {
         vcos_log_trace( "[%s]: [%d]: ioctl mapped-usr-address %d (hdl: %x, addr: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.handle,
                         map.addr );

         return (void*)(uintptr_t)map.addr;
      }
   }
}


/* Retrieves a user opaque handle from a mapped user address
** pointer.
**
** Returns:        0 on error
**                 a non-zero opaque handle on success.
*/
unsigned int vcsm_usr_handle( void *usr_ptr )
{
   int rc;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (usr_ptr == NULL) ) 
   {
      vcos_log_error( "[%s]: [%d]: invalid device or null usr-ptr!",
                      __func__,
                      getpid() );

      return 0;
   }

   if (using_vc_sm_cma)
   {
      //No need to lock the buffer, but need to retrieve the user address
      VCSM_PAYLOAD_ELEM_T *elem;

      elem = vcsm_payload_list_find_mem(usr_ptr);

      if (!elem || !elem->mem)
      {
         vcos_log_trace( "[%s]: usr_ptr %p not tracked, or not mapped. elem %p\n",
                      __func__, usr_ptr, elem);
         return 0;
      }

      return elem->handle;
   }
   else
   {
      struct vmcs_sm_ioctl_map map;

      memset( &map, 0, sizeof(map) );

      map.pid = getpid();
      map.addr = (uintptr_t) usr_ptr;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MAP_USR_HDL,
                  &map );

      if ( rc < 0 )
      {
         vcos_log_error( "[%s]: [%d]: ioctl mapped-usr-hdl FAILED [%d] (pid: %d, addr: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.pid,
                         map.addr );

         return 0;
      }
      else
      {
         vcos_log_trace( "[%s]: [%d]: ioctl mapped-usr-hdl %d (hdl: %x, addr: %x)",
                         __func__,
                         getpid(),
                         rc,
                         map.handle,
                         map.addr );

         return map.handle;
      }
   }
}


/* Locks the memory associated with this opaque handle.
**
** Returns:        NULL on error
**                 a valid pointer on success.
**
** A user MUST lock the handle received from vcsm_malloc
** in order to be able to use the memory associated with it.
**
** On success, the pointer returned is only valid within
** the lock content (ie until a corresponding vcsm_unlock_xx
** is invoked).
*/
void *vcsm_lock( unsigned int handle )
{
   int rc;
   void *usr_ptr = NULL;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (handle == 0) ) 
   {
      vcos_log_error( "[%s]: [%d]: invalid device or invalid handle!",
                      __func__,
                      getpid() );

      goto out;
   }

   if (using_vc_sm_cma)
   {
      //No need to lock the buffer, but need to retrieve the user address
      VCSM_PAYLOAD_ELEM_T *elem;

      elem = vcsm_payload_list_find_handle(handle);

      if (!elem || !elem->mem)
      {
         vcos_log_trace( "[%s]: handle %u not tracked, or not mapped. elem %p\n",
                      __func__, handle, elem);
         goto out;
      }

      usr_ptr = elem->mem;

      {
         struct dma_buf_sync sync;

         //Now sync the buffer
         sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
         rc = ioctl( elem->fd,
                     DMA_BUF_IOCTL_SYNC,
                     &sync );
         if ( rc < 0 )
         {
            vcos_log_trace( "[%s]: [%d]: ioctl DMA_BUF_IOCTL_SYNC failed, rc %d",
                      __func__,
                      getpid(),
                      rc);
         }
      }
      vcos_log_trace( "[%s]: [%d]: ioctl size-usr-hdl %d (hdl: %x) - addr %p",
                      __func__,
                      getpid(),
                      rc,
                      handle,
                      usr_ptr);
   }
   else
   {
      struct vmcs_sm_ioctl_lock_unlock lock_unlock;
      struct vmcs_sm_ioctl_size sz;
      struct vmcs_sm_ioctl_map map;
      struct vmcs_sm_ioctl_cache cache;

      memset( &sz, 0, sizeof(sz) );
      memset( &lock_unlock, 0, sizeof(lock_unlock) );
      memset( &map, 0, sizeof(map) );
      memset( &cache, 0, sizeof(cache) );

      /* Verify what we want is valid. */
      sz.handle = handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_SIZE_USR_HDL,
                  &sz );

      vcos_log_trace( "[%s]: [%d]: ioctl size-usr-hdl %d (hdl: %x) - size %u",
                      __func__,
                      getpid(),
                      rc,
                      sz.handle,
                      sz.size );

      /* We will not be able to lock the resource! */
      if ( (rc < 0) || (sz.size == 0) )
      {
         goto out;
      }

      /* Lock the allocated buffer all the way through videocore. */
      lock_unlock.handle    = sz.handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MEM_LOCK,
                  &lock_unlock );

      vcos_log_trace( "[%s]: [%d]: ioctl mem-lock %d (hdl: %x)",
                      __func__,
                      getpid(),
                      rc,
                      lock_unlock.handle );

      /* We will not be able to lock the resource!
      */
      if ( rc < 0 )
      {
         goto out;
      }

      usr_ptr = (void *) (uintptr_t)lock_unlock.addr;

      /* If applicable, invalidate the cache now.
      */
      if ( usr_ptr && sz.size )
      {
         cache.handle = sz.handle;
         cache.addr   = (uintptr_t) usr_ptr;
         cache.size   = sz.size;

         rc = ioctl( vcsm_handle,
                     VMCS_SM_IOCTL_MEM_INVALID,
                     &cache );

         vcos_log_trace( "[%s]: [%d]: ioctl invalidate (cache) %d (hdl: %x, addr: %x, size: %u)",
                         __func__,
                         getpid(),
                         rc,
                         cache.handle,
                         cache.addr,
                         cache.size );

         if ( rc < 0 )
         {
            vcos_log_error( "[%s]: [%d]: invalidate failed (rc: %d) - [%x;%x] - size: %u (hdl: %x) - cache incoherency",
                            __func__,
                            getpid(),
                            rc,
                            (unsigned int) cache.addr,
                            (unsigned int) (cache.addr + cache.size),
                            (unsigned int) (cache.addr + cache.size) - (unsigned int) cache.addr,
                            cache.handle );
         }
      }
   }

out:
   return usr_ptr;
}


/* Locks the memory associated with this opaque handle.  The lock
** also gives a chance to update the *host* cache behavior of the
** allocated buffer if so desired.  The *videocore* cache behavior
** of the allocated buffer cannot be changed by this call and such
** attempt will be ignored.
**
** The system will attempt to honour the cache_update mode request,
** the cache_result mode will provide the final answer on which cache
** mode is really in use.  Failing to change the cache mode will not
** result in a failure to lock the buffer as it is an application
** decision to choose what to do if (cache_result != cache_update)
**
** The value returned in cache_result can only be considered valid if
** the returned pointer is non NULL.  The cache_result pointer may be
** NULL if the application does not care about the actual outcome of
** its action with regards to the cache behavior change.
**
** Returns:        NULL on error
**                 a valid pointer on success.
**
** A user MUST lock the handle received from vcsm_malloc
** in order to be able to use the memory associated with it.
**
** On success, the pointer returned is only valid within
** the lock content (ie until a corresponding vcsm_unlock_xx
** is invoked).
*/
void *vcsm_lock_cache( unsigned int handle,
                       VCSM_CACHE_TYPE_T cache_update,
                       VCSM_CACHE_TYPE_T *cache_result )
{
   int rc;
   struct vmcs_sm_ioctl_lock_cache lock_cache;
   struct vmcs_sm_ioctl_chk chk;
   struct vmcs_sm_ioctl_map map;
   struct vmcs_sm_ioctl_cache cache;
   struct vmcs_sm_ioctl_size sz;
   void *usr_ptr = NULL;
   VCSM_CACHE_TYPE_T new_cache;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (handle == 0) ) 
   {
      vcos_log_error( "[%s]: [%d]: invalid device or invalid handle!",
                      __func__,
                      getpid() );

      goto out;
   }

   if (using_vc_sm_cma)
   {
      //FIXME: IMPLEMENT THIS
      vcos_log_error("[%s]: IMPLEMENT ME", __func__);
      return NULL;
   }
   else
   {
      memset( &chk, 0, sizeof(chk) );
      memset( &sz, 0, sizeof(sz) );
      memset( &lock_cache, 0, sizeof(lock_cache) );
      memset( &map, 0, sizeof(map) );
      memset( &cache, 0, sizeof(cache) );

      /* Verify what we want is valid.
      */
      chk.handle = handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_CHK_USR_HDL,
                  &chk );

      vcos_log_trace( "[%s]: [%d]: ioctl chk-usr-hdl %d (hdl: %x, addr: %x, sz: %u, cache: %d)",
                      __func__,
                      getpid(),
                      rc,
                      chk.handle,
                      chk.addr,
                      chk.size,
                      chk.cache );

      /* We will not be able to lock the resource!
      */
      if ( rc < 0 )
      {
         goto out;
      }

      /* Validate cache requirements.
      */
      if ( cache_update != (VCSM_CACHE_TYPE_T)chk.cache )
      {
         new_cache = vcsm_cache_table_lookup( (VCSM_CACHE_TYPE_T) chk.cache,
                                              cache_update );
         vcos_log_trace( "[%s]: [%d]: cache lookup hdl: %x: [cur %d ; req %d] -> new %d ",
                         __func__,
                         getpid(),
                         chk.handle,
                         (VCSM_CACHE_TYPE_T)chk.cache,
                         cache_update,
                         new_cache );

         if ( (enum vmcs_sm_cache_e)new_cache == chk.cache )
         {
            /* Effectively no change.
            */
            if ( cache_result != NULL )
            {
               *cache_result = new_cache;
            }
            goto lock_default;
         }
      }
      else
      {
         if ( cache_result != NULL )
         {
            *cache_result = (VCSM_CACHE_TYPE_T)chk.cache;
         }
         goto lock_default;
      }

      /* At this point we know we want to lock the buffer and apply a cache
      ** behavior change.  Start by cleaning out whatever is already setup.
      */
      if ( chk.addr && chk.size )
      {
         munmap( (void *)(uintptr_t)chk.addr, chk.size );

         vcos_log_trace( "[%s]: [%d]: ioctl unmap hdl: %x",
                         __func__,
                         getpid(),
                         chk.handle );
      }

      /* Lock and apply cache behavior change to the allocated buffer all the
      ** way through videocore.
      */
      lock_cache.handle    = chk.handle;
      lock_cache.cached    = (enum vmcs_sm_cache_e) new_cache;   /* Convenient one to one mapping. */

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MEM_LOCK_CACHE,
                  &lock_cache );

      vcos_log_trace( "[%s]: [%d]: ioctl mem-lock-cache %d (hdl: %x)",
                      __func__,
                      getpid(),
                      rc,
                      lock_cache.handle );

      /* We will not be able to lock the resource!
      */
      if ( rc < 0 )
      {
         goto out;
      }

      /* It is possible that this size was zero if the resource was
      ** already un-mapped when we queried it, in such case we need
      ** to figure out the size now to allow mapping to work.
      */
      if ( chk.size == 0 )
      {
         sz.handle = chk.handle;

         rc = ioctl( vcsm_handle,
                     VMCS_SM_IOCTL_SIZE_USR_HDL,
                     &sz );

         vcos_log_trace( "[%s]: [%d]: ioctl size-usr-hdl %d (hdl: %x) - size %u",
                         __func__,
                         getpid(),
                         rc,
                         sz.handle,
                         sz.size );

         /* We will not be able to map again the resource!
         */
         if ( (rc < 0) || (sz.size == 0) )
         {
            goto out;
         }
      }

      /* Map the locked buffer into user space.
      */
      usr_ptr = mmap( 0,
                      (chk.size != 0) ? chk.size : sz.size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      vcsm_handle,
                      chk.handle );

      if ( usr_ptr == NULL )
      {
         vcos_log_error( "[%s]: [%d]: mmap FAILED (hdl: %x)",
                         __func__,
                         getpid(),
                         chk.handle );
      }

      /* If applicable, invalidate the cache now.
      */
      cache.size   = (chk.size != 0) ? chk.size : sz.size;
      if ( usr_ptr && cache.size )
      {
         cache.handle = chk.handle;
         cache.addr   = (uintptr_t) usr_ptr;

         rc = ioctl( vcsm_handle,
                     VMCS_SM_IOCTL_MEM_INVALID,
                     &cache );

         vcos_log_trace( "[%s]: [%d]: ioctl invalidate (cache) %d (hdl: %x, addr: %x, size: %u)",
                         __func__,
                         getpid(),
                         rc,
                         cache.handle,
                         cache.addr,
                         cache.size );

         if ( rc < 0 )
         {
            vcos_log_error( "[%s]: [%d]: invalidate failed (rc: %d) - [%x;%x] - size: %u (hdl: %x) - cache incoherency",
                            __func__,
                            getpid(),
                            rc,
                            (unsigned int) cache.addr,
                            (unsigned int) (cache.addr + cache.size),
                            (unsigned int) (cache.addr + cache.size) - (unsigned int) cache.addr,
                            cache.handle );
         }
      }

      /* Update the caller with the information it expects to see.
      */
      if ( cache_result != NULL )
      {
         *cache_result = new_cache;
      }

      /* Done.
      */
      goto out;
   }


lock_default:
   usr_ptr = vcsm_lock ( handle );

out:
   return usr_ptr;
}


/* Unlocks the memory associated with this user mapped address.
** Apply special processing that would override the otherwise
** default behavior.
**
** If 'cache_no_flush' is specified:
**    Do not flush cache as the result of the unlock (if cache
**    flush was otherwise applicable in this case).
**
** Returns:        0 on success
**                 -errno on error.
**
** After unlocking a mapped address, the user should no longer
** attempt to reference it.
*/
int vcsm_unlock_ptr_sp( void *usr_ptr, int cache_no_flush )
{
   int rc;
   struct vmcs_sm_ioctl_lock_unlock lock_unlock;
   struct vmcs_sm_ioctl_map map;
   struct vmcs_sm_ioctl_cache cache;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (usr_ptr == NULL) ) 
   {
      vcos_log_error( "[%s]: [%d]: invalid device or invalid user-ptr!",
                      __func__,
                      getpid() );

      rc = -EIO;
      goto out;
   }

   if (using_vc_sm_cma)
   {
      struct dma_buf_sync sync;
      VCSM_PAYLOAD_ELEM_T *elem;

      elem = vcsm_payload_list_find_mem(usr_ptr);

      if (!elem)
      {
         vcos_log_trace( "[%s]: addr %p not tracked, or not mapped. elem %p\n",
                      __func__, usr_ptr, elem);
         rc = -EINVAL;
         goto out;
      }

      if (!cache_no_flush)
      {
         sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
         rc = ioctl( elem->fd,
                     DMA_BUF_IOCTL_SYNC,
                     &sync );
         if ( rc < 0 )
         {
            vcos_log_trace( "[%s]: [%d]: ioctl DMA_BUF_IOCTL_SYNC failed, rc %d",
                      __func__,
                      getpid(),
                      rc);
         }
      }
      else
         rc = 0;
   }
   else
   {
      memset( &map, 0, sizeof(map) );
      memset( &lock_unlock, 0, sizeof(lock_unlock) );
      memset( &cache, 0, sizeof(cache) );

      /* Retrieve the handle of the memory we want to unlock.
      */
      map.pid = getpid();
      map.addr = (uintptr_t) usr_ptr;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MAP_USR_HDL,
                  &map );

      vcos_log_trace( "[%s]: [%d]: ioctl mapped-usr-hdl %d (hdl: %x, addr: %x, sz: %u)",
                      __func__,
                      getpid(),
                      rc,
                      map.handle,
                      map.addr,
                      map.size );

      /* We will not be able to flush/unlock the resource!
      */
      if ( rc < 0 )
      {
         goto out;
      }

      /* If applicable, flush the cache now.
      */
      if ( !cache_no_flush && map.addr && map.size )
      {
         cache.handle = map.handle;
         cache.addr   = map.addr;
         cache.size   = map.size;

         rc = ioctl( vcsm_handle,
                     VMCS_SM_IOCTL_MEM_FLUSH,
                     &cache );

         vcos_log_trace( "[%s]: [%d]: ioctl flush (cache) %d (hdl: %x, addr: %x, size: %u)",
                         __func__,
                         getpid(),
                         rc,
                         cache.handle,
                         cache.addr,
                         cache.size );

         if ( rc < 0 )
         {
            vcos_log_error( "[%s]: [%d]: flush failed (rc: %d) - [%x;%x] - size: %u (hdl: %x) - cache incoherency",
                            __func__,
                            getpid(),
                            rc,
                            (unsigned int) cache.addr,
                            (unsigned int) (cache.addr + cache.size),
                            (unsigned int) (cache.addr + cache.size) - (unsigned int) cache.addr,
                            cache.handle );
         }
      }

      /* Unock the allocated buffer all the way through videocore.
      */
      lock_unlock.handle    = map.handle;  /* From above ioctl. */

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MEM_UNLOCK,
                  &lock_unlock );

      vcos_log_trace( "[%s]: [%d]: ioctl mem-unlock %d (hdl: %x)",
                      __func__,
                      getpid(),
                      rc,
                      lock_unlock.handle );
   }

out:
   return rc;
}


/* Unlocks the memory associated with this user mapped address.
**
** Returns:        0 on success
**                 -errno on error.
**
** After unlocking a mapped address, the user should no longer
** attempt to reference it.
*/
int vcsm_unlock_ptr( void *usr_ptr )
{
   return vcsm_unlock_ptr_sp( usr_ptr, 0 );
}


/* Unlocks the memory associated with this user opaque handle.
** Apply special processing that would override the otherwise
** default behavior.
**
** If 'cache_no_flush' is specified:
**    Do not flush cache as the result of the unlock (if cache
**    flush was otherwise applicable in this case).
**
** Returns:        0 on success
**                 -errno on error.
**
** After unlocking an opaque handle, the user should no longer
** attempt to reference the mapped addressed once associated
** with it.
*/
int vcsm_unlock_hdl_sp( unsigned int handle, int cache_no_flush )
{
   int rc;
   struct vmcs_sm_ioctl_lock_unlock lock_unlock;
   struct vmcs_sm_ioctl_chk chk;
   struct vmcs_sm_ioctl_cache cache;
   struct vmcs_sm_ioctl_map map;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (handle == 0) ) 
   {
      vcos_log_error( "[%s]: [%d]: invalid device or invalid handle!",
                      __func__,
                      getpid() );

      rc = -EIO;
      goto out;
   }

   if (using_vc_sm_cma)
   {
      VCSM_PAYLOAD_ELEM_T *elem;
      struct dma_buf_sync sync;

      elem = vcsm_payload_list_find_handle(handle);

      if (!elem)
      {
         vcos_log_trace( "[%s]: handle %u not tracked, or not mapped. elem %p\n",
                      __func__, handle, elem);
         rc = -EINVAL;
         goto out;
      }

      sync.flags = DMA_BUF_SYNC_END;
      if (!cache_no_flush)
      {
         sync.flags |= DMA_BUF_SYNC_RW;
      }

      rc = ioctl( elem->fd, DMA_BUF_IOCTL_SYNC, &sync );
      if ( rc < 0 )
      {
         vcos_log_trace( "[%s]: [%d]: ioctl DMA_BUF_IOCTL_SYNC failed, rc %d",
                   __func__,
                   getpid(),
                   rc);
      }
   }
   else
   {
      memset( &chk, 0, sizeof(chk) );
      memset( &lock_unlock, 0, sizeof(lock_unlock) );
      memset( &cache, 0, sizeof(cache) );
      memset( &map, 0, sizeof(map) );

      /* Retrieve the handle of the memory we want to lock.
      */
      chk.handle = handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_CHK_USR_HDL,
                  &chk );

      vcos_log_trace( "[%s]: [%d]: ioctl chk-usr-hdl %d (hdl: %x, addr: %x, sz: %u) nf %d",
                      __func__,
                      getpid(),
                      rc,
                      chk.handle,
                      chk.addr,
                      chk.size,
                      cache_no_flush);

      /* We will not be able to flush/unlock the resource!
      */
      if ( rc < 0 )
      {
         goto out;
      }

      /* If applicable, flush the cache now.
      */
      if ( !cache_no_flush && chk.addr && chk.size )
      {
         cache.handle = chk.handle;
         cache.addr   = chk.addr;
         cache.size   = chk.size;

         rc = ioctl( vcsm_handle,
                     VMCS_SM_IOCTL_MEM_FLUSH,
                     &cache );

         vcos_log_trace( "[%s]: [%d]: ioctl flush (cache) %d (hdl: %x)",
                         __func__,
                         getpid(),
                         rc,
                         cache.handle );

         if ( rc < 0 )
         {
            vcos_log_error( "[%s]: [%d]: flush failed (rc: %d) - [%x;%x] - size: %u (hdl: %x) - cache incoherency",
                            __func__,
                            getpid(),
                            rc,
                            (unsigned int) cache.addr,
                            (unsigned int) (cache.addr + cache.size),
                            (unsigned int) (cache.addr + cache.size) - (unsigned int) cache.addr,
                            cache.handle );
         }
      }

      /* Unlock the allocated buffer all the way through videocore.
      */
      lock_unlock.handle    = chk.handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MEM_UNLOCK,
                  &lock_unlock );

      vcos_log_trace( "[%s]: [%d]: ioctl mem-unlock %d (hdl: %x)",
                      __func__,
                      getpid(),
                      rc,
                      lock_unlock.handle );
   }

out:
   return rc;
}


/* Unlocks the memory associated with this user opaque handle.
**
** Returns:        0 on success
**                 -errno on error.
**
** After unlocking an opaque handle, the user should no longer
** attempt to reference the mapped addressed once associated
** with it.
*/
int vcsm_unlock_hdl( unsigned int handle )
{
   return vcsm_unlock_hdl_sp( handle, 0 );
}

/* Resizes a block of memory allocated previously by vcsm_alloc.
**
** Returns:        0 on success
**                 -errno on error.
**
** The handle must be unlocked by user prior to attempting any
** resize action.
**
** On error, the original size allocated against the handle
** remains available the same way it would be following a
** successful vcsm_malloc.
*/
int vcsm_resize( unsigned int handle, unsigned int new_size )
{
   int rc;
   struct vmcs_sm_ioctl_size sz;
   struct vmcs_sm_ioctl_resize resize;
   struct vmcs_sm_ioctl_lock_unlock lock_unlock;
   struct vmcs_sm_ioctl_map map;
   unsigned int size_aligned = new_size;
   void *usr_ptr = NULL;

   if ( (vcsm_handle == VCSM_INVALID_HANDLE) || (handle == 0) ) 
   {
      vcos_log_error( "[%s]: [%d]: invalid device or invalid handle!",
                      __func__,
                      getpid() );

      rc = -EIO;
      goto out;
   }

   if (using_vc_sm_cma)
   {
      //Not supported with CMA
      rc = -EFAULT;
   }
   else
   {
      memset( &sz, 0, sizeof(sz) );
      memset( &resize, 0, sizeof(resize) );
      memset( &lock_unlock, 0, sizeof(lock_unlock) );
      memset( &map, 0, sizeof(map) );

      /* Ask for page aligned.
      */
      size_aligned = (new_size + vcsm_page_size - 1) & ~(vcsm_page_size - 1);

      /* Verify what we want is valid.
      */
      sz.handle = handle;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_SIZE_USR_HDL,
                  &sz );

      vcos_log_trace( "[%s]: [%d]: ioctl size-usr-hdl %d (hdl: %x) - size %u",
                      __func__,
                      getpid(),
                      rc,
                      sz.handle,
                      sz.size );

      /* We will not be able to free up the resource!
      **
      ** However, the driver will take care of it eventually once the device is
      ** closed (or dies), so this is not such a dramatic event...
      */
      if ( (rc < 0) || (sz.size == 0) )
      {
         goto out;
      }

      /* We first need to unmap the resource
      */
      usr_ptr = (void *) vcsm_usr_address( sz.handle );
      if ( usr_ptr != NULL )
      {
         munmap( usr_ptr, sz.size );

         vcos_log_trace( "[%s]: [%d]: ioctl unmap hdl: %x",
                         __func__,
                         getpid(),
                         sz.handle );
      }
      else
      {
         vcos_log_trace( "[%s]: [%d]: freeing unmapped area (hdl: %x)",
                         __func__,
                         getpid(),
                         map.handle );
      }

      /* Resize the allocated buffer all the way through videocore.
      */
      resize.handle    = sz.handle;
      resize.new_size  = size_aligned;

      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MEM_RESIZE,
                  &resize );

      vcos_log_trace( "[%s]: [%d]: ioctl resize %d (hdl: %x)",
                      __func__,
                      getpid(),
                      rc,
                      resize.handle );

      /* Although resized, the resource will not be usable.
      */
      if ( rc < 0 )
      {
         goto out;
      }

      /* Remap the resource
      */
      if (  mmap( 0,
                  resize.new_size,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  vcsm_handle,
                  resize.handle ) == NULL )
      {
         vcos_log_error( "[%s]: [%d]: mmap FAILED (hdl: %x)",
                         __func__,
                         getpid(),
                         resize.handle );

         /* At this point, it is not yet a problem that we failed to
         ** map the buffer because it will not be used right away.
         **
         ** Possibly the mapping may work the next time the user tries
         ** to lock the buffer for usage, and if it still fails, it will
         ** be up to the user to deal with it.
         */
      }
   }

out:
   return rc;
}


/* Flush or invalidate the memory associated with this user opaque handle
**
** Returns:        non-zero on error
**
** structure contains a list of flush/invalidate commands
** See header file
*/
int vcsm_clean_invalid( struct vcsm_user_clean_invalid_s *s )
{
   int rc = 0;
   struct vmcs_sm_ioctl_clean_invalid cache;

   if ( vcsm_handle == VCSM_INVALID_HANDLE )
   {
      vcos_log_error( "[%s]: [%d]: invalid device or invalid handle!",
                      __func__,
                      getpid() );

      return -1;
   }

   memcpy( &cache, s, sizeof cache );

   if (using_vc_sm_cma)
   {
      // Deprecated. The API is not compatible with 64-bit support (addr is an
      // unsigned int), therefore please update to vcsm_clean_invalid2.
      rc = -1;
   }
   else
   {
      rc = ioctl( vcsm_handle,
                   VMCS_SM_IOCTL_MEM_CLEAN_INVALID,
                   &cache );
   }

   return rc;
}

/* Flush or invalidate the memory associated with this user opaque handle
**
** Returns:        non-zero on error
**
** structure contains a list of flush/invalidate commands
** See header file
*/
int vcsm_clean_invalid2( struct vcsm_user_clean_invalid2_s *s )
{
   int rc = 0;

   if ( vcsm_handle == VCSM_INVALID_HANDLE )
   {
      vcos_log_error( "[%s]: [%d]: invalid device or invalid handle!",
                      __func__,
                      getpid() );

      return -1;
   }

   if (using_vc_sm_cma)
   {
      struct vc_sm_cma_ioctl_clean_invalid2 *vcsm_s;
      int i;

      vcsm_s = (struct vc_sm_cma_ioctl_clean_invalid2 *)malloc(sizeof(*vcsm_s) + sizeof(struct vc_sm_cma_ioctl_clean_invalid_block)*s->op_count);
      if (!vcsm_s)
         return -1;

      vcsm_s->op_count = s->op_count;
      for (i = 0; i < vcsm_s->op_count; i++)
      {
         vcsm_s->s[i].invalidate_mode = s->s[i].invalidate_mode;
         vcsm_s->s[i].block_count = s->s[i].block_count;
         vcsm_s->s[i].start_address = s->s[i].start_address;
         vcsm_s->s[i].block_size = s->s[i].block_size;
         vcsm_s->s[i].inter_block_stride = s->s[i].inter_block_stride;
      }

      rc = ioctl( vcsm_handle,
                   VC_SM_CMA_IOCTL_MEM_CLEAN_INVALID2,
                   vcsm_s );
      free(vcsm_s);
   }
   else
   {
      rc = ioctl( vcsm_handle,
                   VMCS_SM_IOCTL_MEM_CLEAN_INVALID2,
                   s );
   }

   return rc;
}

/* Imports a dmabuf, and binds it to a VCSM handle and MEM_HANDLE_T
**
** Returns:        0 on error
**                 a non-zero opaque handle on success.
**
** On success, the user must invoke vcsm_lock with the returned opaque
** handle to gain access to the memory associated with the opaque handle.
** When finished using the memory, the user calls vcsm_unlock_xx (see those
** function definition for more details on the one that can be used).
** Use vcsm_release to detach from the dmabuf (VideoCore may still hold
** a reference to the buffer until it has finished with the buffer).
**
*/
unsigned int vcsm_import_dmabuf( int dmabuf, const char *name )
{
   int rc;
   unsigned int handle = 0;

   if ( vcsm_handle == VCSM_INVALID_HANDLE )
   {
      vcos_log_error( "[%s]: [%d]: invalid device or invalid handle!",
                      __func__,
                      getpid() );

      return -1;
   }

   if (using_vc_sm_cma)
   {
      VCSM_PAYLOAD_ELEM_T *payload;
      struct vc_sm_cma_ioctl_import_dmabuf import;
      memset( &import, 0, sizeof(import) );

      /* Map the buffer on videocore via the VCSM (Videocore Shared Memory) interface. */
      import.dmabuf_fd = dmabuf;
      import.cached = VMCS_SM_CACHE_NONE; //Support no caching for now - makes it easier for cache management
      if ( name != NULL )
      {
         memcpy ( import.name, name, 32 );
      }
      rc = ioctl( vcsm_handle,
                  VC_SM_CMA_IOCTL_MEM_IMPORT_DMABUF,
                  &import );

      if ( rc < 0 || import.handle < 0 )
      {
         vcos_log_error( "[%s]: [%d] [%s]: ioctl mem-import-dmabuf FAILED [%d] (hdl: %x)",
                         __func__,
                         getpid(),
                         import.name,
                         rc,
                         import.handle );
      }
      else
      {
         /* Map the buffer into user space.
         */
         vcos_log_trace( "[%s]: mapping fd %d, imported from fd %d\n", __func__, import.handle, dmabuf);
         void *usr_ptr = mmap( 0,
                         import.size,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         import.handle,
                         0 );

         if ( usr_ptr == MAP_FAILED )
         {
            vcos_log_error( "[%s]: [%d]: mmap FAILED (hdl: %x), size %u",
                         __func__,
                         getpid(),
                         import.handle, import.size );
            vcsm_free( import.handle );
            return 0;
         }


         vcos_log_trace( "[%s]: mmap to %p",
                         __func__,
                         usr_ptr
                         );

         //vc-sm-cma now hands out file handles (signed int), whilst libvcsm is 
         //handling unsigned int handles. Already checked the handle >=0, so
         //add one to make it a usable handle.
         handle = import.handle + 1;


        vcos_log_trace( "[%s]: [%d] [%s]: ioctl mem-import-dmabuf %d (dmabuf %d imported as hdl: %x)",
                        __func__,
                        getpid(),
                        import.name,
                        rc,
                        dmabuf,
                        import.handle );

         payload = vcsm_payload_list_get();
         if (!payload)
         {
            vcos_log_error("[%s]: max number of allocations reached: %d", __func__, VCSM_PAYLOAD_ELEM_MAX);
            munmap(usr_ptr, import.size);
            vcsm_free(import.handle);
            return 0;
         }
         payload->handle = handle;
         payload->fd = import.handle;
         payload->vc_handle = import.vc_handle;
         payload->mem = usr_ptr;
         payload->size = import.size;
         if (import.dma_addr & 0xFFFFFFFF00000000ULL)
         {
            vcos_log_error("[%s]: dma address returned > 32bit 0x%llx", __func__, import.dma_addr);
            payload->dma_addr = 0;
         }
         else
            payload->dma_addr = (uint32_t)import.dma_addr;
      }
   }
   else
   {
      struct vmcs_sm_ioctl_import_dmabuf import;
      memset( &import, 0, sizeof(import) );

      /* Map the buffer on videocore via the VCSM (Videocore Shared Memory) interface. */
      import.dmabuf_fd = dmabuf;
      import.cached = VMCS_SM_CACHE_NONE; //Support no caching for now - makes it easier for cache management
      if ( name != NULL )
      {
         memcpy ( import.name, name, 32 );
      }
      rc = ioctl( vcsm_handle,
                  VMCS_SM_IOCTL_MEM_IMPORT_DMABUF,
                  &import );

      if ( rc < 0 || import.handle == 0 )
      {
         vcos_log_error( "[%s]: [%d] [%s]: ioctl mem-import-dmabuf FAILED [%d] (hdl: %x)",
                         __func__,
                         getpid(),
                         import.name,
                         rc,
                         import.handle );
      }
      else
      {
         handle = import.handle;
      }

      vcos_log_trace( "[%s]: [%d] [%s]: ioctl mem-import-dmabuf hdl %d rc %d (vcsm hdl: %x)",
                      __func__,
                      getpid(),
                      import.name,
                      dmabuf,
                      rc,
                      import.handle );
   }

   return handle;
}

/* Exports a vcsm handle as a dmabuf.
**
** Returns:        <0 on error
**                 a file descriptor to the dmabuf on success.
**
** The allocation will persist until the file descriptor is closed,
** even if the vcsm handle is released.
**
*/
int vcsm_export_dmabuf( unsigned int vcsm_handle )
{
   int handle = -1;

   if (using_vc_sm_cma)
   {
      VCSM_PAYLOAD_ELEM_T *elem;

      elem = vcsm_payload_list_find_handle(vcsm_handle);

      if (!elem)
      {
         vcos_log_trace( "[%s]: handle %u not tracked, or not mapped. elem %p\n",
                      __func__, handle, elem);
         return -1;
      }

      //Duplicate the existing fd.
      handle = dup(elem->fd);
   }
   else
   {
      //Not supported on the old vcsm kernel driver.
      handle = -1;
   }
   return handle;
}
