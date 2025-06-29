#pragma once
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <cstdint>
#include <list>
#include <unordered_map>

namespace MyCoroutine
{
    constexpr int INVALID_BATCH_ID = -1; //无效的batchId
    constexpr int INVALID_ROUTINE_ID = -1; //无效的协程id
    constexpr int MAX_COROUTINE_SIZE = 102400; //最多创建102400个协程
    constexpr int MAX_BATCH_RUN_SIZE = 51200; //最多创建51200个批量执行
    constexpr int CANARY_SIZE = 512; //canary内存的大小，单位为字节
    constexpr uint8_t CANARY_PADDING = 0x88; //canary填充的内容
    /*
    1.协程的状态转移
    idle->readly
    ready->run
    run->suspend
    suspend->run
    run->idle
    2.批量执行的状态
    idle->ready
    ready->run
    run->idle
    */
    enum State{
        Idle = 1, //空闲
        Ready = 2, //就绪
        Run = 3, //运行
        Suspend = 4, //挂起
    };
    enum ResumeResult{
        NotRunnable = 1, //没有可运行的携程
        Success = 2, //成功唤起一个挂起的携程
    };
    enum StackCheckResult{
        Normal = 0, //正常
        OverFlow = 1, //栈顶溢出
        UderFlow = 2, //栈底溢出
    };
    typedef void (*Entry)(void *arg); //入口函数
    typedef struct LocalData{
        //协程本地变量的数据
        void *data;
        Entry freeEntry; //用于释放本地协程变量的内存。
    } LocalData;
    typedef struct Coroutine{
        //协程结构体。
        State state; //当前优先级
        uint32_t priority; //协程优先级，值越小，优先级越高
        void *arg;  //协程入口函数参数
        Entry entry; //协程入口函数
        ucontext_t ctx; //协程执行上下文
        uint8_t *stack; //每个协程独占协程栈，动态分配
        std::unordered_map<void*, LocalData> local; //协程本地变量的存储
        int relateBatchId; //关联的batchId
        bool isInsertBatch; //当前在协程是否插入了batchRun的卡点
    } Coroutine;
    typedef struct Batch{
        //协程执行结构体
        State state; //协程执行的状态
        int relateId; //关联协程id
        std::unordered_map<int, bool> cid2finish; //每个关联协程的运行状态
    } Batch;
    typedef struct Schedule{
        //协程调度器
        ucontext_t main; //用于保存主协程的上下文
        int32_t runningCorouineId; //运行中的从协程的id
        int32_t coroutineCnt; //协程数
        int32_t activityCnt; //处于非空闲状态的协程数
        bool isMasterCoroutine; //当前协程是否为主协程
        Coroutine *coroutines[MAX_COROUTINE_SIZE]; //从协程数组池
        Batch *batchs[MAX_BATCH_RUN_SIZE]; //批量执行数组池
        int stackSize; //协程栈的大小，单位为字节
        std::list<int> batchFinishList; //完成的批量执行的关联协程id
        bool stackCheck; //检测协程栈空间是否溢出
    } Schedule;
    
    int CoroutineCreate(Schedule &schedule, Entry entry, void *arg, uint32_t priority = 0, int relateBatchId = INVALID_BATCH_ID);
    //创建协程
    
    bool CoroutineCanCreate(Schedule& schedule);
    //判断是否能创建协程
    
    void CoroutineYield(Schedule &schedule);
    //让出执行权，只能在从协程里面调用
    
    int CoroutineResume(Schedule &schedule);
    //恢复从协程的调用，只能在主协程中调用
    
    int CoroutineResumeById(Schedule& schedule, int id);
    //恢复指定从携程的调用，只能在主协程中调用

    int CoroutineResumeInBatch(Schedule& schedule, int id);
    //恢复从协程batch中的协程调用，只能在主协程中调用

    int CoroutineResumeBatchFinish(Schedule &schedule);
    //恢复被插入batch卡点的从协程的调用，只能在主协程调用

    bool CoroutineIsInBatch(Schedule& schedule);
    //判断当前从协程是否在bathch中

    void CoroutineLocalSet(Schedule& schedule, void *key, LocalData localdata);
    //设置协程本地变量

    bool CoroutineLocalGet(Schedule& schedule, void *key, LocalData &localdata);
    //获取协程本地变量
    
    int CoroutineStackCheck(Schedule& schedule, int id);
    //协程栈使用检测

    int BatchInit(Schedule &schedule, int batchId);
    //初始化一个批量执行的上下文

    void BatchAdd(Schedule &schedule, int batchId, Entry entry, void *arg, uint32_t priority = 0);
    //在批量执行上下文添加要执行的任务

    void BatchRun(Schedule &schedule, int batchId);
    //执行批量操作
    
    int ScheduleInit(Schedule &schedule, int coroutineCnt, int stackSize = 8 * 1024);
    //初始化协程调度结构体
    
    bool ScheduleRunning(Schedule &schedule);
    //判断是否还有协程还在运行
    
    void ScheduleClean(Schedule &schedule);
    //释放调度器
    
    bool ScheduleTryReleaseMemory(Schedule &schedule);
    //调度器尝试释放内存
    
    int ScheduleGetRunCid(Schedule &schedule);
    //获取当前运行的从协程id
    
    void ScheduleDisableStackCheck(Schedule &schedule);
    //关闭协程栈检查
}