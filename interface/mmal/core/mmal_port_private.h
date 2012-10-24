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

#ifndef MMAL_PORT_PRIVATE_H
#define MMAL_PORT_PRIVATE_H

/** Definition of a port. */
typedef struct MMAL_PORT_PRIVATE_T
{
   /** Pointer to the private data of the core */
   struct MMAL_PORT_PRIVATE_CORE_T *core;
   /** Pointer to the private data of the module in use */
   struct MMAL_PORT_MODULE_T *module;

   MMAL_STATUS_T (*pf_set_format)(MMAL_PORT_T *port);
   MMAL_STATUS_T (*pf_enable)(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T);
   MMAL_STATUS_T (*pf_disable)(MMAL_PORT_T *port);
   MMAL_STATUS_T (*pf_send)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *);
   MMAL_STATUS_T (*pf_flush)(MMAL_PORT_T *port);
   MMAL_STATUS_T (*pf_parameter_set)(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param);
   MMAL_STATUS_T (*pf_parameter_get)(MMAL_PORT_T *port, MMAL_PARAMETER_HEADER_T *param);
   MMAL_STATUS_T (*pf_connect)(MMAL_PORT_T *port, MMAL_PORT_T *other_port);

   uint8_t *(*pf_payload_alloc)(MMAL_PORT_T *port, uint32_t payload_size);
   void     (*pf_payload_free)(MMAL_PORT_T *port, uint8_t *payload);

} MMAL_PORT_PRIVATE_T;

/** Callback called by components when a \ref MMAL_BUFFER_HEADER_T needs to be sent back to the
 * user */
void mmal_port_buffer_header_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

/** Callback called by components when an event \ref MMAL_BUFFER_HEADER_T needs to be sent to the
 * user. Events differ from ordinary buffer headers because they originate from the component
 * and do not return data from the client to the component. */
void mmal_port_event_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

/** Allocate a port structure */
MMAL_PORT_T *mmal_port_alloc(MMAL_COMPONENT_T *, MMAL_PORT_TYPE_T type, unsigned int extra_size);
/** Free a port structure */
void mmal_port_free(MMAL_PORT_T *port);
/** Allocate an array of ports */
MMAL_PORT_T **mmal_ports_alloc(MMAL_COMPONENT_T *, unsigned int ports_num, MMAL_PORT_TYPE_T type,
                               unsigned int extra_size);
/** Free an array of ports */
void mmal_ports_free(MMAL_PORT_T **ports, unsigned int ports_num);

/** Acquire a reference on a port */
void mmal_port_acquire(MMAL_PORT_T *port);

/** Release a reference on a port */
MMAL_STATUS_T mmal_port_release(MMAL_PORT_T *port);

#endif /* MMAL_PORT_PRIVATE_H */
