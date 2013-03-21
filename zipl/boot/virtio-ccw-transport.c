/*
 * VirtIO CCW transport implementation
 *
 * Copyright (c) 2010 Alexander Graf <agraf@suse.de>
 * Copyright (c) 2013 SUSE LINUX Products GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "virtio-transport.h"

/***********************************************
 *             Virtio functions                *
 ***********************************************/

static void virtio_init(void)
{
}

/***********************************************
 *                 SCLP fake                   *
 ***********************************************/


/*
 * Subroutine to set up the SCLP interface.
 *
 * Parameters:
 *   R2  = 0 to activate, non-zero to deactivate
 *
 * Returns:
 *   R2  = 0 on success, non-zero on failure
 */
unsigned long _sclp_setup(unsigned long deactivate)
{
    if (deactivate)
        return 0;

    early_puts("_sclp_setup\n");
    virtio_init();
    return 0;
}

