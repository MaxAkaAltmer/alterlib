/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2020 Maxim L. Grishin  (altmer@arts-union.ru)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*****************************************************************************/

#include "athread.h"

#if defined(linux) || defined(__APPLE__)

#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <signal.h>

struct internalSleepStream
{
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;

    volatile bool exit_flag;
    volatile bool complete_flag;

    //пользовательские переменные
    void *data;
    volatile bool loop_flag;
    ADelegate<int,void*> proc; //делегат или...
    int (*fun)(void*); //простая функция
};

void* sleepStreamThread (void *p)
{
    internalSleepStream *hand = (internalSleepStream*)p;

    pthread_mutex_lock( &hand->mutex );
    hand->complete_flag=true;

    while (true)
    {
        pthread_cond_wait( &hand->condition, &hand->mutex );

        if(hand->exit_flag)break;

        int rv=0;
        do
        {
            if(hand->fun) rv=(*hand->fun)(hand->data);
            else rv=hand->proc(hand->data);

            if(hand->loop_flag && rv>=0)
            {
                aSleep(rv);
            }
        }while(hand->loop_flag && rv>=0);

        hand->complete_flag=true;
    }

    pthread_mutex_unlock(&hand->mutex);
    return NULL;
}

AThread::AThread(ADelegate<int, void *> proc)
{
    internalSleepStream *hand = new internalSleepStream;
    hand->complete_flag=false;
    hand->exit_flag=false;
    hand->loop_flag=false;

    pthread_cond_init(&hand->condition,NULL);
    pthread_mutex_init(&hand->mutex,NULL);

    hand->fun=NULL;
    hand->proc = proc;
    pthread_create(&hand->thread,NULL,sleepStreamThread,hand);

    internal=hand;
}

AThread::AThread(int (*fun)(void*))
{
    internalSleepStream *hand = new internalSleepStream;
    hand->complete_flag=false;
    hand->exit_flag=false;
    hand->loop_flag=false;

    pthread_cond_init(&hand->condition,NULL);
    pthread_mutex_init(&hand->mutex,NULL);

    hand->fun=fun;
    pthread_create(&hand->thread,NULL,sleepStreamThread,hand);

    internal=hand;
}

void AThread::terminate()
{
    internalSleepStream *hand = (internalSleepStream*)internal;

    hand->loop_flag=false;
    hand->exit_flag=true;
    pthread_kill(hand->thread, SIGKILL);
}

AThread::~AThread()
{
    internalSleepStream *hand = (internalSleepStream*)internal;

    if(!hand->exit_flag)
    {
        hand->loop_flag=false;
        bool sucsess=false;
        while(!sucsess)
        {
            pthread_mutex_lock( &hand->mutex );
            if(hand->complete_flag)
            {
                sucsess=true;
                hand->exit_flag=true;
            }
            pthread_mutex_unlock(&hand->mutex);
            if(!sucsess)sched_yield();
        }
        pthread_cond_signal(&hand->condition);
        sched_yield();

        pthread_join( hand->thread, NULL);
    }

    pthread_mutex_destroy(&hand->mutex);

    delete hand;
}

bool AThread::run(void *data, bool loop)
{
    internalSleepStream *hand = (internalSleepStream*)internal;

    if(hand->exit_flag)return false;
    hand->loop_flag=false;

    bool sucsess=false;
    while(!sucsess)
    {
        pthread_mutex_lock( &hand->mutex );
        if(hand->complete_flag)
        {
            sucsess=true;
            hand->loop_flag=loop;
            hand->complete_flag=false;
            hand->data=data;
        }
        pthread_mutex_unlock(&hand->mutex);
        if(!sucsess)sched_yield();
    }
    pthread_cond_signal(&hand->condition);

    return true;
}

bool AThread::wait(int count, int us)
{
    internalSleepStream *hand = (internalSleepStream*)internal;

    if(hand->exit_flag)return true;
    hand->loop_flag=false;

    bool success=false;
    while(!success && (count<0 || count>0))
    {
        if(count>0)count--;
        if(hand->complete_flag)
        {
            success=true;
        }
        if(!success)
        {
            if(!us)sched_yield();
            else usleep(us);
        }
    }

    return success;
}

bool AThread::isOff()
{
    internalSleepStream *hand = (internalSleepStream*)internal;
    return hand->complete_flag || hand->exit_flag;
}

void aSleep(int us)
{
    if(!us)sched_yield();
    else usleep(us);
}

struct internalSemaphore
{
    pthread_mutex_t mutex;
};

ASemaphore::ASemaphore()
{
    internalSemaphore *hand = new internalSemaphore;
    pthread_mutex_init(&hand->mutex,NULL);

    internal=hand;
}

ASemaphore::~ASemaphore()
{
    internalSemaphore *hand = (internalSemaphore*)internal;
    pthread_mutex_destroy(&hand->mutex);
    delete hand;
}

void ASemaphore::lock()
{
    internalSemaphore *hand = (internalSemaphore*)internal;
    pthread_mutex_lock( &hand->mutex );
}

void ASemaphore::unlock()
{
    internalSemaphore *hand = (internalSemaphore*)internal;
    pthread_mutex_unlock(&hand->mutex);
}

#else

#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <WinBase.h>

typedef WINBOOL WINAPI (*SleepConditionVariableCS_proc) (PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds);
static SleepConditionVariableCS_proc pSleepConditionVariableCS=NULL;
typedef VOID WINAPI (*InitializeConditionVariable_proc) (PCONDITION_VARIABLE ConditionVariable);
static InitializeConditionVariable_proc pInitializeConditionVariable=NULL;
typedef VOID WINAPI (*WakeAllConditionVariable_proc) (PCONDITION_VARIABLE ConditionVariable);
static WakeAllConditionVariable_proc pWakeAllConditionVariable=NULL;

static bool existsConditionVars()
{
    HMODULE hand=LoadLibraryA("kernel32.dll");
    pSleepConditionVariableCS=(SleepConditionVariableCS_proc)GetProcAddress(hand,"SleepConditionVariableCS");
    pInitializeConditionVariable=(InitializeConditionVariable_proc)GetProcAddress(hand,"InitializeConditionVariable");
    pWakeAllConditionVariable=(WakeAllConditionVariable_proc)GetProcAddress(hand,"WakeAllConditionVariable");

    if(pSleepConditionVariableCS && pInitializeConditionVariable
            && pWakeAllConditionVariable)
    {
        return true;
    }
    return false;
}

static bool conditionVariableExists=existsConditionVars();

struct internalSleepStream
{
    DWORD id;
    HANDLE hph;

    //медленная синхронизация для ХР
    HANDLE go;

    //быстрая синхронизация начиная с Vista
    CONDITION_VARIABLE cv;
    CRITICAL_SECTION lock;

    volatile bool loop_flag;
    volatile bool exit_flag;
    volatile bool complete_flag;

    //пользовательские переменные
    void *data;
    ADelegate<int,void*> proc; //делегат или...
    int (*fun)(void*); //простая функция
};


static DWORD WINAPI sleepStreamThread (PVOID p)
{
    internalSleepStream *hand = (internalSleepStream*)p;

    EnterCriticalSection (&hand->lock);
    hand->complete_flag=true;
    if(!conditionVariableExists)
        LeaveCriticalSection (&hand->lock);

    while (true)
    {
        if(!conditionVariableExists)
        {
            WaitForSingleObject(hand->go, INFINITE);
            ResetEvent(hand->go);
            EnterCriticalSection (&hand->lock);
        }
        else
        {
            pSleepConditionVariableCS (&hand->cv, &hand->lock, INFINITE);
        }
        if(hand->exit_flag)break;

        int rv=0;
        do
        {
            if(hand->fun) rv=(*hand->fun)(hand->data);
            else rv=hand->proc(hand->data);

            if(hand->loop_flag && rv>=0)
            {
                aSleep(rv);
            }
        }while(hand->loop_flag && rv>=0);

        hand->complete_flag=true;
        if(!conditionVariableExists)
            LeaveCriticalSection (&hand->lock);

    }
    if(conditionVariableExists)
        LeaveCriticalSection (&hand->lock);

    return 0;
}

void AThread::terminate()
{
    internalSleepStream *hand = (internalSleepStream*)internal;

    hand->loop_flag=false;
    hand->exit_flag=true;
    TerminateThread(&hand->hph,0);
}

AThread::AThread(ADelegate<int, void *> proc)
{
    internalSleepStream *hand = new internalSleepStream;
    hand->complete_flag=false;
    hand->exit_flag=false;
    hand->loop_flag=false;

    //XP совместимость
    if(!conditionVariableExists)
        hand->go=CreateEvent(0,1,0,0);
    else
        pInitializeConditionVariable (&hand->cv);

    InitializeCriticalSection (&hand->lock);

    hand->fun=NULL;
    hand->proc = proc;
    hand->hph = CreateThread (NULL, 0, sleepStreamThread, hand, 0, &hand->id);

    internal = hand;
}

AThread::AThread(int (*fun)(void*))
{
    internalSleepStream *hand = new internalSleepStream;
    hand->complete_flag=false;
    hand->exit_flag=false;
    hand->loop_flag=false;

    //XP совместимость
    if(!conditionVariableExists)
        hand->go=CreateEvent(0,1,0,0);
    else
        pInitializeConditionVariable (&hand->cv);

    InitializeCriticalSection (&hand->lock);

    hand->fun = fun;
    hand->hph = CreateThread (NULL, 0, sleepStreamThread, hand, 0, &hand->id);

    internal = hand;
}

AThread::~AThread()
{
    internalSleepStream *hand = (internalSleepStream*)internal;

    if(!hand->exit_flag)
    {
        hand->loop_flag=false;

        bool sucsess=false;
        while(!sucsess)
        {
            EnterCriticalSection (&hand->lock);
            if(hand->complete_flag)
            {
                sucsess=true;
                hand->exit_flag=true;
            }
            LeaveCriticalSection (&hand->lock);
            if(!sucsess)Sleep(0);
        }
        if(!conditionVariableExists)
            SetEvent(hand->go);
        else
            pWakeAllConditionVariable (&hand->cv);

        Sleep(0);
        WaitForSingleObject (hand->hph, INFINITE);
    }

    CloseHandle(hand->hph);
    DeleteCriticalSection(&hand->lock);
    if(!conditionVariableExists)
        CloseHandle(hand->go);

    delete hand;
}

bool AThread::run(void *data, bool loop)
{
    internalSleepStream *hand = (internalSleepStream*)internal;

    if(hand->exit_flag)return false;
    hand->loop_flag=false;

    bool sucsess=false;
    while(!sucsess)
    {
        EnterCriticalSection (&hand->lock);
        if(hand->complete_flag)
        {
            sucsess=true;
            hand->loop_flag=loop;
            hand->complete_flag=false;
            hand->data = data;
        }
        LeaveCriticalSection (&hand->lock);
        if(!sucsess)Sleep(0);
    }
    if(!conditionVariableExists)
        SetEvent(hand->go);
    else
        pWakeAllConditionVariable (&hand->cv);

    return true;
}

bool AThread::wait(int count, int us)
{
    internalSleepStream *hand = (internalSleepStream*)internal;

    if(hand->exit_flag)return true;
    hand->loop_flag=false;

    bool success=false;
    while(!success && (count<0 || count>0))
    {
        if(count>0)count--;
        if(hand->complete_flag)
        {
            success=true;
        }
        if(!success)aSleep(us);
    }

    return success;
}

bool AThread::isOff()
{
    internalSleepStream *hand = (internalSleepStream*)internal;
    return hand->complete_flag || hand->exit_flag;
}

void aSleep(int us)
{
    Sleep(us/1000);
}

struct internalSemaphore
{
    CRITICAL_SECTION lock;
};

ASemaphore::ASemaphore()
{
    internalSemaphore *hand = new internalSemaphore;
    InitializeCriticalSection (&hand->lock);
    internal=hand;
}

ASemaphore::~ASemaphore()
{
    internalSemaphore *hand = (internalSemaphore*)internal;
    DeleteCriticalSection(&hand->lock);
    delete hand;
}

void ASemaphore::lock()
{
    internalSemaphore *hand = (internalSemaphore*)internal;
    EnterCriticalSection (&hand->lock);
}

void ASemaphore::unlock()
{
    internalSemaphore *hand = (internalSemaphore*)internal;
    LeaveCriticalSection (&hand->lock);
}

#endif
