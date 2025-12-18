#pragma once

#if SKL_INTERNAL

#define NAMED_TIMED_BLOCK_(name, number, ...) TimedBlock timedBlock_##number = TimedBlock(__COUNTER__, __FILE__, __LINE__, #name, ## __VA_ARGS__)
#define NAMED_TIMED_BLOCK(name, ...) NAMED_TIMED_BLOCK_(name, __LINE__, ## __VA_ARGS__)

#define TIMED_BLOCK_(number, ...) NAMED_TIMED_BLOCK_(__FUNCTION__, number, ## __VA_ARGS__)
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ## __VA_ARGS__)

#else

#define NAMED_TIMED_BLOCK_(name, number, ...) 
#define NAMED_TIMED_BLOCK(name, ...) 

#define TIMED_BLOCK_(number, ...) 
#define TIMED_BLOCK(...) 

#endif

struct DebugRecord
{
    const char *fileName;
    const char *blockName;

    u32 lineNumber;
    u32 reserved;

    // NOTE(marvin): Top 32-bits hit count, bottom 32-bits cycle count.
    u64 hitCount_cycleCount;
};

extern DebugRecord debugRecordArray[];

struct TimedBlock
{
    DebugRecord *debugRecord;
    u64 startCycleCount;
    u32 hitCount;


    #if SKL_INTERNAL
    TimedBlock(u32 index, const char *fileName, u32 lineNumber,
               const char *blockName, u32 hitCount0 = 1)
    {
        debugRecord = debugRecordArray + index;
        debugRecord->fileName = fileName;
        debugRecord->lineNumber = lineNumber;
        debugRecord->blockName = blockName;
        startCycleCount = ReadCPUTimer();
        hitCount = hitCount0;
    }

    ~TimedBlock()
    {
        u64 cycleCountDelta = ReadCPUTimer() - startCycleCount;
        u64 hitCountDelta = (u64)hitCount;
        u64 delta = cycleCountDelta | (hitCountDelta << 32);
        AtomicAddU64(&debugRecord->hitCount_cycleCount, delta);
    }
    #endif
};
