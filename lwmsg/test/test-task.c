/*
 * Copyright (c) Likewise Software.  All rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the license, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.  You should have received a copy
 * of the GNU Lesser General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * LESSER GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */

/*
 * Module Name:
 *
 *        test-task.c
 *
 * Abstract:
 *
 *        Task manager unit tests
 *
 * Authors: Brian Koropoff (bkoropoff@likewisesoftware.com)
 *
 */

#define LWMSG_SPEC_META

#include <config.h>
#include <lwmsg/lwmsg.h>
#include <moonunit/moonunit.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "util-private.h"
#include "test-private.h"

LWMsgTaskManager* manager = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t event = PTHREAD_COND_INITIALIZER;

MU_FIXTURE_SETUP(task)
{
    MU_TRY(lwmsg_task_acquire_manager(&manager));
}

MU_FIXTURE_TEARDOWN(task)
{
    lwmsg_task_release_manager(manager);
}

static
void
basic_work_item(
    void* data
    )
{
    LWMsgBool volatile *value = data;

    pthread_mutex_lock(&lock);
    *value = LWMSG_TRUE;
    pthread_cond_signal(&event);
    pthread_mutex_unlock(&lock);
}

MU_TEST(task, basic_work_item)
{
    LWMsgBool volatile value = LWMSG_FALSE;

    MU_TRY(lwmsg_task_dispatch_work_item(manager, basic_work_item, (void*) &value));

    pthread_mutex_lock(&lock);
    while (!value)
    {
        pthread_cond_wait(&event, &lock);
    }
    pthread_mutex_unlock(&lock);
}

static const int target = 5;

static
LWMsgStatus
timer_task(
    void* data,
    LWMsgTaskTrigger trigger,
    LWMsgTaskTrigger* next_trigger,
    LWMsgTime* next_time
    )
{
    int volatile * value = data;

    if (trigger & LWMSG_TASK_TRIGGER_INIT)
    {
        MU_VERBOSE("Waking up for the first time");
    }
    else
    {
        MU_VERBOSE("Waking up on timer");
    }

    if (*value < target)
    {
        (*value)++;
        *next_trigger = LWMSG_TASK_TRIGGER_TIME;
        next_time->seconds = 0;
        next_time->microseconds = 10000;
    }
    else
    {
        *next_trigger = 0;
    }

    return LWMSG_STATUS_SUCCESS;
}

MU_TEST(task, timer_task)
{
    int value = 0;
    LWMsgTask* task = NULL;

    MU_TRY(lwmsg_task_new(manager, NULL, timer_task, (void*) &value, &task));

    lwmsg_task_wake(task);
    lwmsg_task_wait(task);
    lwmsg_task_release(task);

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, value, target);
}


