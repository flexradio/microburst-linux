/*
 *  ============================================================================
 *
 *  Copyright (c) 2008-2012, Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef  ATOMIC_LINUX_H
#define  ATOMIC_LINUX_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/atomic.h>

/*! @brief Typedef for atomic variable */
typedef atomic_t Atomic;

/*!
 *  @brief  Reads the variable atomically.
 */
#define Atomic_read(var)            atomic_read ((atomic_t *)var)

/*!
 *  @brief  Sets the variable atomically.
 */
#define Atomic_set(var,val)         atomic_set ((atomic_t *)var, val)

/*!
 *  @brief  Increments the variable atomically.
 */
#define Atomic_inc_return(var)      atomic_inc_return ((atomic_t *)var)

/*!
 *  @brief  Decrements the variable atomically.
 */
#define Atomic_dec_return(var)      atomic_dec_return ((atomic_t *)var)

/*!
 * @brief Function to compare a mask and set if not equal
 *
 * @params v    Pointer to atomic variable
 * @params mask Mask to compare with
 * @params val  Value to be set if mask does not match.
 */
static inline void Atomic_cmpmask_and_set(Atomic * v, UInt32 mask, UInt32 val)
{
    Int32         ret;
    unsigned long flags;
    atomic_t *    atm = (atomic_t *)v;

    raw_local_irq_save(flags);
    ret = atm->counter;
    if (likely(((ret & mask) != mask)))
        atm->counter = val;
    raw_local_irq_restore(flags);
}

/*!
 * @brief Function to compare a mask and then check current value less than
 *        provided value.
 *
 * @params v    Pointer to atomic variable
 * @params mask Mask to compare with
 * @params val  Value to be set if mask does not match.
 */
static inline Bool Atomic_cmpmask_and_lt(Atomic * v, UInt32 mask, UInt32 val)
{
    Bool       ret = TRUE;
    atomic_t * atm = (atomic_t *)v;
    Int32      cur;
    unsigned long flags;

    raw_local_irq_save(flags);
    cur = atm->counter;
    /* Compare mask, if matches then compare val */
    if (likely(((cur & mask) == mask))) {
        if (likely(cur >= val)) {
            ret = FALSE;
        }
    }
    raw_local_irq_restore(flags);

    /*! @retval TRUE  if mask matches and current value is less than given
     *  value */
    /*! @retval FALSE either mask doesnot matches or current value is not less
     *  than given value */
    return ret;
}

/*!
 * @brief Function to compare a mask and then check current value greater than
 *        provided value.
 *
 * @params v    Pointer to atomic variable
 * @params mask Mask to compare with
 * @params val  Value to be set if mask does not match.
 */
static inline Bool Atomic_cmpmask_and_gt(Atomic * v, UInt32 mask, UInt32 val)
{
    Bool       ret = FALSE;
    atomic_t * atm = (atomic_t *)v;
    Int32      cur;
    unsigned long flags;

    raw_local_irq_save(flags);
    cur = atm->counter;
    /* Compare mask, if matches then compare val */
    if (likely(((cur & mask) == mask))) {
        if (likely(cur > val)) {
            ret = TRUE;
        }
    }
    raw_local_irq_restore(flags);

    /*! @retval TRUE  if mask matches and current value is less than given
     *  value */
    /*! @retval FALSE either mask doesnot matches or current value is not
     *  greater than given value */
    return ret;
}

#endif /* if !defined(ATOMIC_LINUX_H) */
