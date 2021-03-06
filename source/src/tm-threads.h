/* Copyright (C) 2007-2011 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Victor Julien <victor@inliniac.net>
 * \author Anoop Saldanha <anoopsaldanha@gmail.com>
 */

#ifndef __TM_THREADS_H__
#define __TM_THREADS_H__

#include "tmqh-packetpool.h"
#include "tm-threads-common.h"
#include "tm-modules.h"

#define TM_QUEUE_NAME_MAX 16
#define TM_THREAD_NAME_MAX 16

typedef TmEcode (*TmSlotFunc)(ThreadVars *, Packet *, void *, PacketQueue *,
                              PacketQueue *);

typedef struct TmSlot_ {
    /* the TV holding this slot */
    ThreadVars *tv;

    /* function pointers */
    SC_ATOMIC_DECLARE(TmSlotFunc, SlotFunc);

    TmEcode (*PktAcqLoop)(ThreadVars *, void *, void *);

    TmEcode (*SlotThreadInit)(ThreadVars *, void *, void **);
    void (*SlotThreadExitPrintStats)(ThreadVars *, void *);
    TmEcode (*SlotThreadDeinit)(ThreadVars *, void *);

    /* data storage */
    void *slot_initdata;
    SC_ATOMIC_DECLARE(void *, slot_data);


    /*
    还有各模块在处理过程中可能会新生成数据包（如隧道数据包、重组数据包），
    这些数据包存储在与每个slot绑定的slot_pre_pq或slot_post_pq队列中
    slot_pre_pq vs. slot_post_pq：
    某个slot在处理某个母数据包时新产生的子数据包，若放入slot_pre_pq中，
    则这个数据包将在本个slot处理完母数据包后，在后续slot处理母数据包之前，
    先将这些子数据包放到后续的slot去处理；而如果是放如slot_post_pq，
    则需要等到母数据包被所有slot都处理完后，在下一个数据包处理之前，
    再去集中处理，如上面所述。
    */

    /* queue filled by the SlotFunc with packets that will
     * be processed futher _before_ the current packet.
     * The locks in the queue are NOT used */
     
    PacketQueue slot_pre_pq;   

    /* queue filled by the SlotFunc with packets that will
     * be processed futher _after_ the current packet. The
     * locks in the queue are NOT used */
     
    PacketQueue slot_post_pq; 

    /* store the thread module id */
    int tm_id;

    /* slot id, only used my TmVarSlot to know what the first slot is */
    int id;

    /* linked list, only used when you have multiple slots(used by TmVarSlot) */
    struct TmSlot_ *slot_next;
} TmSlot;

extern ThreadVars *tv_root[TVT_MAX];

extern SCMutex tv_root_lock;

void TmSlotSetFuncAppend(ThreadVars *, TmModule *, void *);
void TmSlotSetFuncAppendDelayed(ThreadVars *, TmModule *, void *, int delayed);
TmSlot *TmSlotGetSlotForTM(int);

ThreadVars *TmThreadCreate(char *, char *, char *, char *, char *, char *,
                           void *(fn_p)(void *), int);
ThreadVars *TmThreadCreatePacketHandler(char *, char *, char *, char *, char *,
                                        char *);
ThreadVars *TmThreadCreateMgmtThread(char *name, void *(fn_p)(void *), int);
ThreadVars *TmThreadCreateCmdThread(char *name, void *(fn_p)(void *), int);
TmEcode TmThreadSpawn(ThreadVars *);
void TmThreadSetFlags(ThreadVars *, uint8_t);
void TmThreadSetAOF(ThreadVars *, uint8_t);
void TmThreadKillThread(ThreadVars *);
void TmThreadKillThreadsFamily(int family);
void TmThreadKillThreads(void);
void TmThreadClearThreadsFamily(int family);
void TmThreadAppend(ThreadVars *, int);
void TmThreadRemove(ThreadVars *, int);

TmEcode TmThreadSetCPUAffinity(ThreadVars *, uint16_t);
TmEcode TmThreadSetThreadPriority(ThreadVars *, int);
TmEcode TmThreadSetCPU(ThreadVars *, uint8_t);
TmEcode TmThreadSetupOptions(ThreadVars *);
void TmThreadSetPrio(ThreadVars *);
int TmThreadGetNbThreads(uint8_t type);

void TmThreadInitMC(ThreadVars *);
void TmThreadTestThreadUnPaused(ThreadVars *);
void TmThreadContinue(ThreadVars *);
void TmThreadContinueThreads(void);
void TmThreadPause(ThreadVars *);
void TmThreadPauseThreads(void);
void TmThreadCheckThreadState(void);
TmEcode TmThreadWaitOnThreadInit(void);
ThreadVars *TmThreadsGetCallingThread(void);

void TmThreadActivateDummySlot(void);
void TmThreadDeActivateDummySlot(void);

int TmThreadsCheckFlag(ThreadVars *, uint16_t);
void TmThreadsSetFlag(ThreadVars *, uint16_t);
void TmThreadsUnsetFlag(ThreadVars *, uint16_t);
void TmThreadWaitForFlag(ThreadVars *, uint16_t);

TmEcode TmThreadsSlotVarRun (ThreadVars *tv, Packet *p, TmSlot *slot);

ThreadVars *TmThreadsGetTVContainingSlot(TmSlot *);
void TmThreadDisableThreadsWithTMS(uint8_t tm_flags);
TmSlot *TmThreadGetFirstTmSlotForPartialPattern(const char *);

/**
 *  \brief Process the rest of the functions (if any) and queue.
 */
static inline TmEcode TmThreadsSlotProcessPkt(ThreadVars *tv, TmSlot *s, Packet *p) // lgx_mark 进一步的数据包处理
{
    TmEcode r = TM_ECODE_OK;

    if (s == NULL) {
        tv->tmqh_out(tv, p);
        return r;
    }

    if (TmThreadsSlotVarRun(tv, p, s) == TM_ECODE_FAILED) //lgx_mark 将数据包依次传入后续的各个slot进行处理。
    {
        TmqhOutputPacketpool(tv, p);
        TmSlot *slot = s;
        while (slot != NULL) {
            SCMutexLock(&slot->slot_post_pq.mutex_q);
            TmqhReleasePacketsToPacketPool(&slot->slot_post_pq);
            SCMutexUnlock(&slot->slot_post_pq.mutex_q);

            slot = slot->slot_next;
        }
        TmThreadsSetFlag(tv, THV_FAILED);
        r = TM_ECODE_FAILED;

    }
    else
    {
        tv->tmqh_out(tv, p);

        /* post process pq */
        TmSlot *slot = s;
        while (slot != NULL) {
            if (slot->slot_post_pq.top != NULL)
            {
                while (1)
                {
                    SCMutexLock(&slot->slot_post_pq.mutex_q);
                    Packet *extra_p = PacketDequeue(&slot->slot_post_pq);
                    SCMutexUnlock(&slot->slot_post_pq.mutex_q);

                    if (extra_p == NULL)
                        break;

                    if (slot->slot_next != NULL)
                    {
                        r = TmThreadsSlotVarRun(tv, extra_p, slot->slot_next);
                        if (r == TM_ECODE_FAILED)
                        {
                            SCMutexLock(&slot->slot_post_pq.mutex_q);
                            TmqhReleasePacketsToPacketPool(&slot->slot_post_pq);
                            SCMutexUnlock(&slot->slot_post_pq.mutex_q);

                            TmqhOutputPacketpool(tv, extra_p);
                            TmThreadsSetFlag(tv, THV_FAILED);
                            break;
                        }
                    }
                    tv->tmqh_out(tv, extra_p);
                }
            } /* if (slot->slot_post_pq.top != NULL) */
            slot = slot->slot_next;
        } /* while (slot != NULL) */
    }

    return r;
}

#endif /* __TM_THREADS_H__ */
