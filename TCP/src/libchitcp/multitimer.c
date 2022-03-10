/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  An API for managing multiple timers
 */

/*
 *  Copyright (c) 2013-2019, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  - Neither the name of The University of Chicago nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "chitcp/utils.h"
#include "chitcp/multitimer.h"
#include "chitcp/log.h"

/*
 * delete_timer - Delete an timer from the list of active timer
 *
 * id: id of timer to be deleted
 *
 * mt: head of list to remove timer from
 *
 * Returns: nothing
 */
void delete_timer(multi_timer_t *mt, uint16_t id)
{
    /* no active timer */
    if (mt->active_timers == NULL)
    {
        return;
    }
    active_timer_t *elt;
    DL_FOREACH(mt->active_timers, elt)
    {
        if (elt->timer->id == id)
        {
            break;
        }
    }
    DL_DELETE(mt->active_timers, elt);
    free(elt);
    return;
}

/* Normalize Timespec to let the tv_nsec of timespec less than SECOND
 *
 * ts: timespec
 *
 * Returns: nothing
 */
void timespec_normalize(struct timespec *ts)
{
    ts->tv_sec += ((ts->tv_nsec) / SECOND);
    ts->tv_nsec = ((ts->tv_nsec) % SECOND);
}

/* helper function to create multitimer thread
 *
 * args: multitimer arguments
 *
 * Returns: nothing
 */
void *mt_handler_func(void *args)
{
    multi_timer_t *mt = (multi_timer_t *)args;
    int rc;
    struct timespec ts, temp;

    pthread_mutex_lock(&mt->mt_lock);
    mt->active = true;

    clock_gettime(CLOCK_REALTIME, &ts);
    timespec_normalize(&ts);

    while (mt->active)
    {
        rc = pthread_cond_timedwait(&mt->mt_cond, &mt->mt_lock, &ts);

        clock_gettime(CLOCK_REALTIME, &ts);

        if (rc == ETIMEDOUT)
        {
            clock_gettime(CLOCK_REALTIME, &ts);
            while (mt->active_timers &&
                   timespec_subtract(&temp, &mt->active_timers->timeout_spec, &ts))
            {
                mt->active_timers->timer->num_timeouts += 1;
                mt->active_timers->timer->active = false;
                mt->active_timers->timer->callback_fn(mt, mt->active_timers->timer,
                                                      mt->active_timers->timer->callback_args);

                delete_timer(mt, mt->active_timers->timer->id);
            }
        }
        if (mt->active_timers != NULL)
        {
            ts.tv_sec = mt->active_timers->timeout_spec.tv_sec;
            ts.tv_nsec = mt->active_timers->timeout_spec.tv_nsec;

            /* Normalize Timespec */
            if (ts.tv_nsec > SECOND)
            {
                clock_gettime(CLOCK_REALTIME, &ts);
                timespec_normalize(&ts);
            }
        }
        else
        {
            clock_gettime(CLOCK_REALTIME, &ts);
            timespec_normalize(&ts);
        }
    }
    pthread_mutex_unlock(&mt->mt_lock);
    pthread_exit(NULL);
}

/* See multitimer.h */
int timespec_subtract(struct timespec *result, struct timespec *x, struct timespec *y)
{
    struct timespec tmp;
    tmp.tv_sec = y->tv_sec;
    tmp.tv_nsec = y->tv_nsec;

    /* Perform the carry for the later subtraction by updating tmp. */
    if (x->tv_nsec < tmp.tv_nsec)
    {
        uint64_t sec = (tmp.tv_nsec - x->tv_nsec) / SECOND + 1;
        tmp.tv_nsec -= SECOND * sec;
        tmp.tv_sec += sec;
    }
    if (x->tv_nsec - tmp.tv_nsec > SECOND)
    {
        uint64_t sec = (x->tv_nsec - tmp.tv_nsec) / SECOND;
        tmp.tv_nsec += SECOND * sec;
        tmp.tv_sec -= sec;
    }

    /* Compute the time remaining to wait.
     * tv_nsec is certainly positive. */
    result->tv_sec = x->tv_sec - tmp.tv_sec;
    result->tv_nsec = x->tv_nsec - tmp.tv_nsec;

    /* Return 1 if result is negative. */
    return x->tv_sec < tmp.tv_sec;
}

/* See multitimer.h */
int mt_init(multi_timer_t *mt, uint16_t num_timers)
{
    mt->timers = (single_timer_t **)calloc(1, sizeof(single_timer_t *) * num_timers);
    if (mt->timers == NULL)
    {
        return CHITCP_EINIT;
    }

    mt->num_timers = num_timers;

    for (int id = 0; id < num_timers; id++)
    {
        mt->timers[id] = (single_timer_t *)calloc(1, sizeof(single_timer_t));
        if (mt->timers[id] == NULL)
        {
            return CHITCP_EINIT;
        }

        mt->timers[id]->active = false;
        mt->timers[id]->id = id;
        mt->timers[id]->num_timeouts = 0;
        mt->timers[id]->callback_fn = NULL;
        mt->timers[id]->callback_args = NULL;
    }

    pthread_mutex_init(&mt->mt_lock, NULL);
    pthread_cond_init(&mt->mt_cond, NULL);
    mt->active_timers = NULL;

    if (pthread_create(&mt->mt_thread, NULL, mt_handler_func, mt) != 0)
    {
        mt_free(mt);
        return CHITCP_ETHREAD;
    }

    return CHITCP_OK;
}

/* See multitimer.h */
int mt_free(multi_timer_t *mt)
{
    while (1)
    {
        if (mt->active)
        {
            mt->active = false;
            pthread_mutex_lock(&mt->mt_lock);
            pthread_cond_signal(&mt->mt_cond);
            pthread_mutex_unlock(&mt->mt_lock);

            /* Free single timers, active timers, and lastly multitimer */
            active_timer_t *at;
            DL_FOREACH(mt->active_timers, at)
            {
                DL_DELETE(mt->active_timers, at);
                free(at);
            }
            for (int i = 0; i < mt->num_timers; i++)
            {
                free(mt->timers[i]);
            }
            free(mt->timers);

            pthread_join(mt->mt_thread, NULL);
            pthread_mutex_destroy(&mt->mt_lock);
            pthread_cond_destroy(&mt->mt_cond);

            return CHITCP_OK;
        }
    }
}

/* See multitimer.h */
int mt_get_timer_by_id(multi_timer_t *mt, uint16_t id, single_timer_t **timer)
{
    if (id >= mt->num_timers || id < 0)
    {
        // invalid id
        return CHITCP_EINVAL;
    }

    *timer = mt->timers[id];

    return CHITCP_OK;
}
int timeoutcmp(active_timer_t *a, active_timer_t *b)
{
    struct timespec first = a->timeout_spec;
    struct timespec second = b->timeout_spec;
    if (first.tv_sec > second.tv_sec)
    {
        return 1;
    }
    else if (first.tv_sec < second.tv_sec)
    {
        return -1;
    }
    else
    {
        if (first.tv_nsec > second.tv_nsec)
        {
            return 1;
        }
        else if (first.tv_nsec < second.tv_nsec)
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }
}

/* See multitimer.h */
int mt_set_timer(multi_timer_t *mt, uint16_t id, uint64_t timeout, mt_callback_func callback_fn, void *callback_args)
{
    struct timespec temp;
    if (id >= mt->num_timers || id < 0)
    {
        // invalid id
        return CHITCP_EINVAL;
    }
    active_timer_t *elt;
    DL_FOREACH(mt->active_timers, elt)
    {
        if (elt->timer->id == id)
        {
            return CHITCP_EINVAL;
        }
    }
    mt->timers[id]->active = true;
    mt->timers[id]->callback_fn = callback_fn;
    mt->timers[id]->callback_args = callback_args;

    active_timer_t *temp_timer = (active_timer_t *)calloc(1, sizeof(active_timer_t));
    temp_timer->timer = mt->timers[id];

    /* update the timeout spec of the timer */
    clock_gettime(CLOCK_REALTIME, &temp_timer->timeout_spec);
    temp_timer->timeout_spec.tv_nsec += timeout;
    timespec_normalize(&temp_timer->timeout_spec);

    DL_APPEND(mt->active_timers, temp_timer);
    DL_SORT(mt->active_timers, timeoutcmp);
    pthread_mutex_lock(&mt->mt_lock);
    pthread_cond_signal(&mt->mt_cond);
    pthread_mutex_unlock(&mt->mt_lock);

    return CHITCP_OK;
}

/* See multitimer.h */
int mt_cancel_timer(multi_timer_t *mt, uint16_t id)
{
    if (id >= mt->num_timers || id < 0)
    {
        // invalid id
        return CHITCP_EINVAL;
    }
    while (1)
    {
        if (mt->active)
        {
            if (!mt->timers[id]->active)
            {
                return CHITCP_EINVAL;
            }
            else
            {
                mt->timers[id]->active = false;
                delete_timer(mt, id);
                pthread_mutex_lock(&mt->mt_lock);
                pthread_cond_signal(&mt->mt_cond);
                pthread_mutex_unlock(&mt->mt_lock);

                return CHITCP_OK;
            }
        }
    }
}

/* See multitimer.h */
int mt_set_timer_name(multi_timer_t *mt, uint16_t id, const char *name)
{
    if (id >= mt->num_timers || id < 0)
    {
        // invalid id
        return CHITCP_EINVAL;
    }
    strncpy(mt->timers[id]->name, name, MAX_TIMER_NAME_LEN);
    return CHITCP_OK;
}

/* mt_chilog_single_timer - Prints a single timer using chilog
 *
 * level: chilog log level
 *
 * timer: Timer
 *
 * Returns: Always returns CHITCP_OK
 */
int mt_chilog_single_timer(loglevel_t level, single_timer_t *timer)
{
    struct timespec now, diff;
    clock_gettime(CLOCK_REALTIME, &now);

    if (timer->active)
    {
        /* Compute the appropriate value for "diff" here; it should contain
         * the time remaining until the timer times out.
         * Note: The timespec_subtract function can come in handy here*/
        diff.tv_sec = 0;
        diff.tv_nsec = 0;
        chilog(level, "%i %s %lis %lins", timer->id, timer->name, diff.tv_sec, diff.tv_nsec);
    }
    else
        chilog(level, "%i %s", timer->id, timer->name);

    return CHITCP_OK;
}

/* See multitimer.h */
int mt_chilog(loglevel_t level, multi_timer_t *mt, bool active_only)
{
    /* Your code here */

    return CHITCP_OK;
}
