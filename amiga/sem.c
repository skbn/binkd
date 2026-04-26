/*
 *  Amiga semaphores
 */

#include <exec/exec.h>
#include <proto/exec.h>
#include <string.h>
#include <sem.h>

extern void Log(int lev, char *s, ...);

int _InitSem(void *vpSem)
{
    memset(vpSem, 0, sizeof(struct SignalSemaphore));
    InitSemaphore((struct SignalSemaphore *)vpSem);
    return 0;
}

int _CleanSem(void *vpSem)
{
    return 0;
}

int _LockSem(void *vpSem)
{
    ObtainSemaphore((struct SignalSemaphore *)vpSem);
    return 0;
}

int _ReleaseSem(void *vpSem)
{
    ReleaseSemaphore((struct SignalSemaphore *)vpSem);
    return 0;
}

int _InitEventSem(EVENTSEM *sem)
{
    if (!sem)
        return -1;

    sem->waiter = NULL;

    sem->sigbit = AllocSignal(-1);

    if (sem->sigbit == (ULONG)-1)
        return -1;

    return 0;
}

int _CleanEventSem(EVENTSEM *sem)
{
    if (!sem)
        return -1;

    if (sem->sigbit != (ULONG)-1)
    {
        FreeSignal((LONG)sem->sigbit);
        sem->sigbit = (ULONG)-1;
    }

    sem->waiter = NULL;
    return 0;
}

int _PostSem(EVENTSEM *sem)
{
    if (!sem)
        return -1;

    if (sem->waiter && sem->sigbit != (ULONG)-1)
    {
        Signal((struct Task *)sem->waiter, 1UL << sem->sigbit);
    }

    return 0;
}

int _WaitSem(EVENTSEM *sem, int sec)
{
    ULONG mask;
    struct Task *me;

    if (!sem || sem->sigbit == (ULONG)-1)
        return -1;

    me = FindTask(NULL);
    sem->waiter = me;

    /* Wait on SIGBREAKF_CTRL_C to avoid hanging on race */
    mask = Wait((1UL << sem->sigbit) | SIGBREAKF_CTRL_C);

    sem->waiter = NULL;

    /* Return timeout/break indication if only CTRL_C fired */
    if (!(mask & (1UL << sem->sigbit)) && (mask & SIGBREAKF_CTRL_C))
        return -1;

    return 0;
}
