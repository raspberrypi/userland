/*
Copyright (c) 2016 Raspberry Pi (Trading) Ltd.
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

#ifndef DTOVERLAY_H
#define DTOVERLAY_H

#define BE4(x) ((x)>>24)&0xff, ((x)>>16)&0xff, ((x)>>8)&0xff, ((x)>>0)&0xff

#define NON_FATAL(err) (((err) < 0) ? -(err) : (err))
#define IS_FATAL(err) ((err) < 0)
#define ONLY_FATAL(err) (IS_FATAL(err) ? (err) : 0)

#define DTOVERLAY_PADDING(size) (-(size))

typedef enum
{
   DTOVERLAY_ERROR,
   DTOVERLAY_DEBUG
} dtoverlay_logging_type_t;

typedef struct
{
   const char *param;
   int len;
   const char *b;
} DTOVERLAY_PARAM_T;

typedef struct
{
   void *fdt;
   int fdt_is_malloced;
   int min_phandle;
   int max_phandle;
   void *trailer;
   int trailer_len;
   int trailer_is_malloced;
} DTBLOB_T;


typedef void DTOVERLAY_LOGGING_FUNC(dtoverlay_logging_type_t type,
                                    const char *fmt, va_list args);

/* Return values: -ve = fatal error, positive = non-fatal error */
int dtoverlay_create_node(DTBLOB_T *dtb, const char *node_name, int path_len);

int dtoverlay_delete_node(DTBLOB_T *dtb, const char *node_name, int path_len);

int dtoverlay_fixup_overlay(DTBLOB_T *base_dtb, DTBLOB_T *overlay_dtb);

int dtoverlay_merge_overlay(DTBLOB_T *base_dtb, DTBLOB_T *overlay_dtb);

int dtoverlay_merge_params(DTBLOB_T *dtb, const DTOVERLAY_PARAM_T *params,
                           unsigned int num_params);

const char *dtoverlay_find_override(DTBLOB_T *dtb, const char *override_name,
                                    int *data_len);

int dtoverlay_apply_override(DTBLOB_T *dtb, const char *override_name,
                             const char *override_data, int data_len,
                             const char *override_value);

int dtoverlay_extract_override(const char *override_name,
                               const char **data_ptr, int *len_ptr,
                               const char **namep, int *namelenp, int *offp,
                               int *sizep);

int dtoverlay_apply_integer_override(DTBLOB_T *dtb, int phandle,
                                     const char *prop_name, int name_len,
                                     int override_off, int override_size,
                                     uint64_t override_val);

int dtoverlay_apply_string_override(DTBLOB_T *dtb, int phandle,
                                    const char *prop_name, int name_len,
                                    const char *override_str);

int dtoverlay_set_synonym(DTBLOB_T *dtb, const char *dst, const char *src);

int dtoverlay_dup_property(DTBLOB_T *dtb, const char *node_name,
                           const char *dst, const char *src);

DTBLOB_T *dtoverlay_create_dtb(void *fdt, int max_size);

DTBLOB_T *dtoverlay_load_dtb_from_fp(FILE *fp, int max_size);

DTBLOB_T *dtoverlay_load_dtb(const char *filename, int max_size);

DTBLOB_T *dtoverlay_import_fdt(void *fdt, int max_size);

int dtoverlay_save_dtb(const DTBLOB_T *dtb, const char *filename);

int dtoverlay_extend_dtb(DTBLOB_T *dtb, int new_size);

int dtoverlay_dtb_totalsize(DTBLOB_T *dtb);

void dtoverlay_pack_dtb(DTBLOB_T *dtb);

void dtoverlay_free_dtb(DTBLOB_T *dtb);

static inline void *dtoverlay_dtb_trailer(DTBLOB_T *dtb)
{
    return dtb->trailer;
}

static inline int dtoverlay_dtb_trailer_len(DTBLOB_T *dtb)
{
    return dtb->trailer_len;
}

static inline void dtoverlay_dtb_set_trailer(DTBLOB_T *dtb,
                                             void *trailer,
                                             int trailer_len)
{
    dtb->trailer = trailer;
    dtb->trailer_len = trailer_len;
    dtb->trailer_is_malloced = 0;
}

int dtoverlay_find_matching_node(DTBLOB_T *dt, const char **node_names, int pos);

const void *dtoverlay_get_property(DTBLOB_T *dt, int pos, const char *prop_name, int *prop_len);

const char *dtoverlay_get_alias(DTBLOB_T *dt, const char *alias_name);

void dtoverlay_set_logging_func(DTOVERLAY_LOGGING_FUNC *func);

void dtoverlay_enable_debug(int enable);

#endif
