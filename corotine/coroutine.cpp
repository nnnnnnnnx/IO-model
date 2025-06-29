#include "coroutine.h"
#include "percentile.hpp"
#include <iostream>
#include <assert.h>

 namespace MyCoroutine{
    static bool isBatchDone(Schedule &schedule, int batchId){
        assert(batchId >= 0 && batchId < MAX_BATCH_RUN_SIZE);
        assert(schedule.batchs[batchId]->state == Run);
        for (const auto &kv : schedule.batchs[batchId]->cid2finish){
            if(not kv.second) return false;
            //有一个关联协程没执行完，就返回false
        }
        return true;
    }

    static void CoroutineRun(Schedule *schedule){
        schedule->isMasterCoroutine = false;
        int id = schedule->runningCorouineId;
        assert(id >= 0 && id < schedule->coroutineCnt);
        Coroutine *routine = schedule->coroutines[id];
        routine->entry(routine->arg); //执行entry函数
        //entry函数执行完成之后，将协程状态变更为Idle，并标记runningCorouineId为无效id
        routine->state = Idle;
        //如果有关联的batch，则更新batch信息，设置batch关联的写成已经执行完
        if (routine->relateBatchId != INVALID_BATCH_ID){
            Batch *batch = schedule->batchs[routine->relateBatchId];
            batch->cid2finish[id] = true;
            //batch都更新完了，更新batchFinishList
            if (isBatchDone(*schedule, routine->relateBatchId)){
                schedule->batchFinishList.push_back(batch->relateId);
            }
            routine->relateBatchId = INVALID_BATCH_ID;
        }
        schedule->activityCnt--;
        schedule->runningCorouineId = INVALID_ROUTINE_ID;
        if (schedule->stackCheck){
            assert(Normal == CoroutineStackCheck(*schedule, id));
        }
        //等到这个函数执行完，调用栈会回到主协程中，执行routine->ctx.uc_link
        //指向的上下文中的下一条指令
    }

    static void CoroutineInit(Schedule &schedule, Coroutine *routine, Entry entry, void *arg,
        uint32_t priority, int relateBatchId)
    {
        routine->arg = arg;
        routine->entry = entry;
        routine->state = Ready;
        routine->priority = priority;
        routine->relateBatchId = relateBatchId;
        routine->isInsertBatch = false;
        if (nullptr == routine->stack){
            routine->stack = new uint8_t[schedule.stackSize];
            //填充栈顶canary内容
            memset(routine->stack, CANARY_PADDING, CANARY_SIZE);
            //填充栈底canary内容
            memset(routine->stack + schedule.stackSize - CANARY_SIZE, CANARY_PADDING, CANARY_SIZE);
        }
        getcontext(&(routine->ctx));
        routine->ctx.uc_stack.ss_flags = 0;
        routine->ctx.uc_stack.ss_sp = routine->stack + CANARY_SIZE;
        routine->ctx.uc_stack.ss_size = schedule.stackSize - 2 * CANARY_SIZE;
        routine->ctx.uc_link = &(schedule.main);
        /*
        设置routine->ctx上下文执行的函数和对应的参数
        这里没有直接使用entry和arg的设置，而是多了一层CoroutineRun函数的调用
        这是为了在CoroutineRun中的entry函数执行完成之后，将从协程的状态更新为Idle，
        并更新当前处于运行中的协程id变为无效id
        这样这些逻辑就可以对上层函数调用透明了
        */
       makecontext(&(routine->ctx), (void(*)(void))(CoroutineRun), 1, &schedule);
    }

    int CoroutineCreate(Schedule &schedule, Entry entry, void *arg, uint32_t priority, int relateBatchId)    
    {
        int id = 0;
        for (id = 0; id < schedule.coroutineCnt; id++)
        {
            if(schedule.coroutines[id]->state == Idle) break;
        }
        if (id >= schedule.coroutineCnt){
            return INVALID_ROUTINE_ID;
        }
        schedule.activityCnt++;
        Coroutine *routine = schedule.coroutines[id];
        CoroutineInit(schedule, routine, entry, arg, priority, relateBatchId);
        return id;
    }

    bool CoroutineCanCreate(Schedule& schedule)
    {
        int id = 0;
        for (id = 0; id < schedule.coroutineCnt; id++){
            if(schedule.coroutines[id]->state == Idle) return true;
        }
        return false;
    }

    void CoroutineYield(Schedule &schedule)
    {
        assert(not schedule.isMasterCoroutine);
        int id = schedule.runningCorouineId;
        assert(id >= 0 && id < schedule.coroutineCnt);
        Coroutine* routine = schedule.coroutines[schedule.runningCorouineId];
        //更新当前的从协程状态为挂起
        routine->state = Suspend;
        //当前的从协程让出执行权，并把当前的从协程的执行上下文保存在
        //routine->ctx中，执行权回到住协程，只有当从协程被主协程resume
        //swapcontext才会返回
        swapcontext(&routine->ctx, &(schedule.main));
        schedule.isMasterCoroutine = false;
    }

    int CoroutineResume(Schedule &schedule)
    {
        assert(schedule.isMasterCoroutine);
        bool isInsertBatch = true;
        uint32_t priority = UINT32_MAX;
        int coroutineId = INVALID_ROUTINE_ID;
        //从高优先级挂起的协程开始运行，并考虑是否插入了Batch卡点
        for (int i = 0; i < schedule.coroutineCnt; i++)
        {
            if(schedule.coroutines[i]->state == Idle || schedule.coroutines[i]->state == Run)
            {
                continue;
            }
            //执行到这，schedule.coroutines[i]为suspend或ready
            if (not schedule.coroutines[i]->isInsertBatch && isInsertBatch){
                coroutineId = i;
                //没有batch卡点的协程优先级更高
                priority = schedule.coroutines[i]->priority;
                isInsertBatch = false;
            } else if(schedule.coroutines[i]->isInsertBatch && isInsertBatch)
            {
                //插入了batch卡点的协程优先级更低，所以这里不更新isInsertBatch priority和coroutineId
            }else{
                //都没有插入batch卡点，或者都插入了batch卡点
                if(schedule.coroutines[i]->priority < priority){
                    coroutineId = i;
                    priority = schedule.coroutines[i]->priority;
                }
            }
        }
        if(coroutineId == INVALID_ROUTINE_ID) return NotRunnable;
        Coroutine *routine = schedule.coroutines[coroutineId];
        //对于插入batch卡点的协会才能够，需要再次校验batch是否执行完
        if(isInsertBatch){
            //卡点关联的协程必须全部执行完
            assert(isBatchDone(schedule, routine->relateBatchId));
        }
        routine->state = Run;
        schedule.runningCorouineId = coroutineId;
        //从主协程切换到协程编号为id的写成中，执行，并把当前执行上下文保存到main中
        //只有当从协程执行完成或者从协程主动yield，swapcontext才会返回
        swapcontext(&schedule.main, &routine->ctx);
        schedule.isMasterCoroutine = true;
        return Success;
    }

    int CoroutineResumeById(Schedule& schedule, int id)
    {
        assert(schedule.isMasterCoroutine);
        assert(id >= 0 && id < schedule.coroutineCnt);
        Coroutine *routine = schedule.coroutines[id];
        //只有挂起状态或者是就绪状态才可以唤醒
        if(routine->state != Suspend && routine->state != Ready)
            return NotRunnable;
        //插入卡点的协程，需要batch执行完才可以唤醒
        if(routine->isInsertBatch && not isBatchDone(schedule, routine->relateBatchId))
            return NotRunnable;
        routine->state = Run;
        schedule.runningCorouineId = id;
        //从主协程切换到协程编号为id的写成中，执行，并把当前执行上下文保存到main中
        //只有当从协程执行完成或者从协程主动yield，swapcontext才会返回
        swapcontext(&schedule.main, &routine->ctx);
        schedule.isMasterCoroutine = true;
        return Success;
    }

    int CoroutineResumeInBatch(Schedule& schedule, int id){
        assert(schedule.isMasterCoroutine);
        assert(id >= 0 && id < schedule.coroutineCnt);
        Coroutine *routine = schedule.coroutines[id];
        //如果没有被插入batch卡点，则没有需要唤醒的batch协程
        if (not routine->isInsertBatch) return NotRunnable;
        int batchId = routine->relateBatchId;
        auto iter = schedule.batchs[batchId]->cid2finish.begin();
        //恢复batchid关联所有从协程
        while(iter != schedule.batchs[batchId]->cid2finish.end())
        {
            assert(iter->second == false);
            assert(CoroutineResumeById(schedule, iter->first) == Success);
            iter++;
        }
        return Success;
    }

    int CoroutineResumeBatchFinish(Schedule &schedule)
    {
        assert(schedule.isMasterCoroutine);
        if (schedule.batchFinishList.size() <= 0) return NotRunnable;
        while(not schedule.batchFinishList.empty()){
            int cid = schedule.batchFinishList.front();
            schedule.batchFinishList.pop_front();
            assert(CoroutineResumeById(schedule, cid) == Success);
        }   
        return Success;
    }

    bool CoroutineIsInBatch(Schedule &schedule){
        assert(not schedule.isMasterCoroutine);
        int cid = schedule.runningCorouineId;
        return schedule.coroutines[cid]->relateBatchId != INVALID_BATCH_ID;        
    }

    void CoroutineLocalSet(Schedule& schedule, void *key, LocalData localdata)
    {
        assert(not schedule.isMasterCoroutine);
        //只有协程中能调用
        int cid = schedule.runningCorouineId;
        auto iter = schedule.coroutines[cid]->local.find(key);
        if (iter != schedule.coroutines[cid]->local.end()){
            iter->second.freeEntry(iter->second.data);
            //如果之前有值，释放空间
        }
        schedule.coroutines[cid]->local[key] = localdata;
    }

    bool CoroutineLocalGet(Schedule& schedule, void *key, LocalData &localdata)
    {
        assert(not schedule.isMasterCoroutine);
        //只有协程中能调用
        int cid = schedule.runningCorouineId;
        auto iter = schedule.coroutines[cid]->local.find(key);
        if (iter == schedule.coroutines[cid]->local.end())
        {
            //如果不存在，则判断是否有关联batch
            int relateBatchId = schedule.coroutines[cid]->relateBatchId;
            if(relateBatchId == INVALID_BATCH_ID) return false;
            //从被插入batch卡点的协程中查找，进而实现部分协程间变量的共享
            Batch *batch = schedule.batchs[relateBatchId];
            iter = schedule.coroutines[batch->relateId]->local.find(key);
            if (iter == schedule.coroutines[batch->relateId]->local.end())
                return false;
            localdata = iter->second;
            return true;
        }
        localdata = iter->second;
        return true;
    }

    int CoroutineStackCheck(Schedule& schedule, int id)
    {
        assert(id >= 0 && id < schedule.coroutineCnt);
        Coroutine *routine = schedule.coroutines[id];
        assert(routine->stack);
        //栈的生长方向，从高地址到低地址
        for (int i = 0; i < CANARY_SIZE; i++){
            if(routine->stack[i] != CANARY_PADDING){
                return OverFlow;
            }
            if(routine->stack[schedule.stackSize - 1 - i] != CANARY_PADDING){
                return UderFlow;
            }
        }
        return Normal;
    }

    int BatchInit(Schedule &schedule, int batchId)
    {
        assert(not schedule.isMasterCoroutine);
        for (int i = 0; i < MAX_BATCH_RUN_SIZE; i++)
        {
            if(schedule.batchs[i]->state == Idle)
            {
                schedule.batchs[i]->state = Ready;
                schedule.batchs[i]->relateId = schedule.runningCorouineId;
                schedule.coroutines[schedule.runningCorouineId]->relateBatchId = i;
                schedule.coroutines[schedule.runningCorouineId]->isInsertBatch = true;
                return i;
            }
        }
        return INVALID_BATCH_ID;
    }

    void BatchAdd(Schedule &schedule, int batchId, Entry entry, void *arg, uint32_t priority)
    {
        assert(not schedule.isMasterCoroutine);
        assert(batchId >= 0 && batchId < MAX_BATCH_RUN_SIZE);
        assert(schedule.batchs[batchId]->state == Ready);
        assert(schedule.batchs[batchId]->relateId == schedule.runningCorouineId);
        int id = CoroutineCreate(schedule, entry, arg, priority, batchId);
        assert(id != INVALID_ROUTINE_ID);
        schedule.batchs[batchId]->cid2finish[id] = false; //新增要执行的协程还没执行完
    }

    void BatchRun(Schedule &schedule, int batchId)
    {
        assert(not schedule.isMasterCoroutine);
        assert(batchId >= 0 && batchId < MAX_BATCH_RUN_SIZE);
        assert(schedule.batchs[batchId]->relateId == schedule.runningCorouineId);
        schedule.batchs[batchId]->state = Run;
        //只是一个卡点，等batch中所有的协程都执行完了，主协程在恢复协程的执行
        CoroutineYield(schedule);
        schedule.batchs[batchId]->state = Idle;
        schedule.batchs[batchId]->cid2finish.clear();
        schedule.coroutines[schedule.runningCorouineId]->relateBatchId = INVALID_BATCH_ID;
        schedule.coroutines[schedule.runningCorouineId]->isInsertBatch = false;
    }

    int ScheduleInit(Schedule &schedule, int coroutineCnt, int stackSize)
    {
        assert(coroutineCnt > 0 && coroutineCnt <= MAX_COROUTINE_SIZE);
        stackSize += (CANARY_SIZE * 2);//添加canary需要的额外内存
        schedule.activityCnt = 0;
        schedule.stackCheck = true;
        schedule.stackSize = stackSize;
        schedule.isMasterCoroutine = true;
        schedule.coroutineCnt = coroutineCnt;
        schedule.runningCorouineId = INVALID_ROUTINE_ID;
        for (int i = 0; i < coroutineCnt; i++)
        {
            schedule.coroutines[i] = new Coroutine;
            schedule.coroutines[i]->state = Idle;
            schedule.coroutines[i]->stack = nullptr;
        }
        for (int i = 0; i < MAX_BATCH_RUN_SIZE; i++)
        {
            schedule.batchs[i] = new Batch;
            schedule.batchs[i]->state = Idle;
        }
        return 0;
    }

    bool ScheduleRunning(Schedule &schedule)
    {
        assert(schedule.isMasterCoroutine);
        if(schedule.runningCorouineId != INVALID_ROUTINE_ID) return true;
        for (int i = 0; i < schedule.coroutineCnt; i++)
        {
            if(schedule.coroutines[i]->state != Idle) return true;
        }
        return false;
    }

    void ScheduleClean(Schedule &schedule)
    {
        assert(schedule.isMasterCoroutine);
        for (int i = 0; i < schedule.coroutineCnt; i++)
        {
            delete[] schedule.coroutines[i]->stack;
            for (auto &item : schedule.coroutines[i]->local){
                item.second.freeEntry(item.second.data);
                //释放协程本地变量的内存空间
            }
            delete schedule.coroutines[i];
        }
        for (int i = 0; i < MAX_BATCH_RUN_SIZE; i++){
            delete schedule.batchs[i];
        }
    }

    bool ScheduleTryReleaseMemory(Schedule &schedule)
    {
        static Percentile pct;
        pct.Stat("activityCnt", schedule.activityCnt);
        double pctValue;
        //保持pct99水平即可
        if (not pct.GetPercentile("activityCnt", 0.99, pctValue))
            return false;
        int32_t releaseCnt = 0;
        //扣除活动的协程，计算剩余需要保留的栈空间内存的协程数
        int32_t remainStackCnt = (int32_t)pctValue - schedule.activityCnt;
        for (int i = 0; i < schedule.coroutineCnt; i++)
        {
            if (schedule.coroutines[i]->state != Idle) continue;
            if (nullptr == schedule.coroutines[i]->stack) continue;
            if(remainStackCnt <= 0)
            {
                //没有保留名额了
               delete[] schedule.coroutines[i]->stack;
               //释放状态为Idle的协程栈内存
               schedule.coroutines[i]->stack = nullptr;
               for (auto &item : schedule.coroutines[i]->local)
               {
                    item.second.freeEntry(item.second.data);
                    //释放协程本地变量的内存空间
               }
               schedule.coroutines[i]->local.clear();
               releaseCnt++;
               if(releaseCnt >= 25) break; //最多释放25个协程栈的空间，以避免耗时时长
            }else{
                remainStackCnt--; //将保留名额减1
            }
        }
        return true;
    }

    int ScheduleGetRunCid(Schedule &schedule)
    {
        return schedule.runningCorouineId;
    }
    
    void ScheduleDisableStackCheck(Schedule &schedule)
    {
        schedule.stackCheck = false;
    }

}




