/*
 * Copyright © 2013 Siarhei Siamashka <siarhei.siamashka@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "cpuinfo.h"
#include "cpu_backend.h"

#ifdef __GNUC__
#define always_inline inline __attribute__((always_inline))
#else
#define always_inline inline
#endif

static void
twopass_blt_8bpp(int        width,
                 int        height,
                 uint8_t   *dst_bytes,
                 uintptr_t  dst_stride,
                 uint8_t   *src_bytes,
                 uintptr_t  src_stride)
                 
{
    if (src_bytes < dst_bytes + width &&
        src_bytes + src_stride * height > dst_bytes)
    {
        src_bytes += src_stride * height - src_stride;
        dst_bytes += dst_stride * height - dst_stride;
        dst_stride = -dst_stride;
        src_stride = -src_stride;
        if (src_bytes + width > dst_bytes)
        {
            while (--height >= 0)
            {
                memmove(dst_bytes, src_bytes, width);
                dst_bytes += dst_stride;
                src_bytes += src_stride;
            }
            return;
        }
    }
    while (--height >= 0)
    {
        memmove(dst_bytes, src_bytes, width);
        dst_bytes += dst_stride;
        src_bytes += src_stride;
    }
}

static always_inline int
overlapped_blt(void     *self,
               uint32_t *src_bits,
               uint32_t *dst_bits,
               int       src_stride,
               int       dst_stride,
               int       src_bpp,
               int       dst_bpp,
               int       src_x,
               int       src_y,
               int       dst_x,
               int       dst_y,
               int       width,
               int       height)
{
    uint8_t *dst_bytes = (uint8_t *)dst_bits;
    uint8_t *src_bytes = (uint8_t *)src_bits;
    cpu_backend_t *ctx = (cpu_backend_t *)self;
    int bpp = src_bpp >> 3;
    if (src_bpp != dst_bpp || src_bpp & 7 || src_stride < 0 || dst_stride < 0) {
        return 0;
    }

    twopass_blt_8bpp((uintptr_t) width * bpp,
                     height,
                     dst_bytes + (uintptr_t) dst_y * dst_stride * 4 +
                                 (uintptr_t) dst_x * bpp,
                     (uintptr_t) dst_stride * 4,
                     src_bytes + (uintptr_t) src_y * src_stride * 4 +
                                 (uintptr_t) src_x * bpp,
                     (uintptr_t) src_stride * 4);
    return 1;
}

/* A generic implementation */
static int
overlapped_blt_noop(void     *self,
                    uint32_t *src_bits,
                    uint32_t *dst_bits,
                    int       src_stride,
                    int       dst_stride,
                    int       src_bpp,
                    int       dst_bpp,
                    int       src_x,
                    int       src_y,
                    int       dst_x,
                    int       dst_y,
                    int       width,
                    int       height)
{
    return overlapped_blt(self, src_bits, dst_bits, src_stride, dst_stride,
                          src_bpp, dst_bpp, src_x, src_y, dst_x, dst_y,
                          width, height);
}

cpu_backend_t *cpu_backend_init(uint8_t *uncached_buffer,
                                size_t   uncached_buffer_size)
{
    cpu_backend_t *ctx = calloc(sizeof(cpu_backend_t), 1);
    if (!ctx)
        return NULL;

    ctx->uncached_area_begin = uncached_buffer;
    ctx->uncached_area_end   = uncached_buffer + uncached_buffer_size;

    ctx->blt2d.self = ctx;
    ctx->blt2d.overlapped_blt = overlapped_blt_noop;
    ctx->cpuinfo = cpuinfo_init();

    return ctx;
}

void cpu_backend_close(cpu_backend_t *ctx)
{
    if (ctx->cpuinfo)
        cpuinfo_close(ctx->cpuinfo);

    free(ctx);
}
