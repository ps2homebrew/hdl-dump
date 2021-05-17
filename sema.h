/*
 * sema.h
 * $Id: sema.h,v 1.1 2006/09/01 17:37:58 bobi Exp $
 *
 * Copyright 2004 Bobi B., w1zard0f07@yahoo.com
 *
 * This file is part of hdl_dump.
 *
 * hdl_dump is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * hdl_dump is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hdl_dump; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if !defined(_SEMA_H)
#define _SEMA_H

/* quick & dirty POSIX semaphores for Win32 */
#if defined(_BUILD_WIN32)
#include <windows.h>

typedef struct sema_type
{
    HANDLE h;
} sem_t;

static int
sem_init(sem_t *sem,
         int pshared,
         unsigned int value)
{
    sem->h = CreateSemaphore(NULL, value, 1, NULL);
    return (sem->h != NULL ? 0 : -1);
}

static int
sem_wait(sem_t *sem)
{
    return (WaitForSingleObject(sem->h, INFINITE) == WAIT_OBJECT_0 ? 0 : -1);
}

#if 0 /* warning: `sem_trywait' defined but not used */
static int
sem_trywait (sem_t *sem)
{
  return (WaitForSingleObject (sem->h, 0) == WAIT_OBJECT_0 ? 0 : -1);
}
#endif

static int
sem_post(sem_t *sem)
{
    return (ReleaseSemaphore(sem->h, 1, NULL) != 0 ? 0 : -1);
}

static int
sem_destroy(sem_t *sem)
{
    return (CloseHandle(sem->h) ? 0 : -1);
}

#elif defined(__MACH__)
#include <pthread.h>

typedef struct sema_type
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    unsigned int counter;
} sem_t;

static int
sem_init(sem_t *sem,
         int pshared,
         unsigned int value)
{
    if (pthread_mutex_init(&sem->mutex, NULL) != 0)
        return -1;
    if (pthread_cond_init(&sem->cond, NULL) != 0) {
        (void)pthread_mutex_destroy(&sem->mutex);
        return -1;
    }
    sem->counter = value;
    return 0;
}

static int
sem_wait(sem_t *sem)
{
    if (pthread_mutex_lock(&sem->mutex))
        return -1;
    while (sem->counter == 0)
        (void)pthread_cond_wait(&sem->cond, &sem->mutex);
    --sem->counter;
    (void)pthread_mutex_unlock(&sem->mutex);
    return 0;
}

static int
sem_post(sem_t *sem)
{
    if (pthread_mutex_lock(&sem->mutex))
        return -1;
    ++sem->counter;
    (void)pthread_mutex_unlock(&sem->mutex);
    if (pthread_cond_signal(&sem->cond))
        return -1;
    return 0;
}

static int
sem_destroy(sem_t *sem)
{
    (void)pthread_mutex_destroy(&sem->mutex);
    (void)pthread_cond_destroy(&sem->cond);
}

#endif

#endif /* _SEMA_H defined? */
