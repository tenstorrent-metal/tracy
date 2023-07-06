#ifndef __TRACYTTDEVICE_HPP__
#define __TRACYTTDEVICE_HPP__

#if !defined TRACY_ENABLE

#define TracyTTContext() nullptr
#define TracyTTDestroy(c)
#define TracyTTContextName(c, x, y)

#define TracyTTNamedZone(c, x, y, z, w)
#define TracyTTNamedZoneC(c, x, y, z, w, v)
#define TracyTTZone(c, x, y)
#define TracyTTZoneC(c, x, y, z)
#define TracyTTZoneTransient(c,x,y,z,w,v)

#define TracyTTNamedZoneS(c, x, y, z, w, v)
#define TracyTTNamedZoneCS(c, x, y, z, w, v, a)
#define TracyTTZoneS(c, x, y, z)
#define TracyTTZoneCS(c, x, y, z, w)
#define TracyTTZoneTransientS(c,x,y,z,w,v)

#define TracyTTNamedZoneSetEvent(x, e)
#define TracyTTZoneSetEvent(e)

#define TracyTTCollect(c)

#define TracySetCpuTime()

namespace tracy
{
    class TTCtxScope {};
}

using TracyTTCtx = void*;

#else

#include <atomic>
#include <cassert>
#include <sstream>
#include <fstream>

#include "Tracy.hpp"
#include "../client/TracyCallstack.hpp"
#include "../client/TracyProfiler.hpp"
#include "../common/TracyAlloc.hpp"
#include "../common/TracyTTDeviceData.hpp"

#define TRACY_TT_TO_STRING_INDIRECT(T) #T
#define TRACY_TT_TO_STRING(T) TRACY_TT_TO_STRING_INDIRECT(T)
#define TRACY_TT_ASSERT(p) if(!(p)) {                                                         \
    TracyMessageL( "TRACY_TT_ASSERT failed on " TracyFile ":" TRACY_TT_TO_STRING(TracyLine) );  \
    assert(false && "TRACY_TT_ASSERT failed");                                                \
}

namespace tracy {

    enum class EventPhase : uint8_t
    {
        Begin,
        End
    };

    inline int64_t m_tcpu = 0;

    static inline void set_cpu_time()
    {
        ZoneScoped;
        m_tcpu = Profiler::GetTime();
    }

    struct EventInfo
    {
        TTDeviceEvent event;
        EventPhase phase;
    };

    class TTCtx
    {
    public:
        enum { QueryCount = 64 * 1024 };

        TTCtx()
            : m_contextId(GetGpuCtxCounter().fetch_add(1, std::memory_order_relaxed))
            , m_head(0)
            , m_tail(0)
        {
            ZoneScopedC(Color::Red4);
        }

        void PopulateTTContext(int64_t tgpu, float period)
        {
            ZoneScopedC(Color::Red);
            int64_t tcpu;
            tcpu = m_tcpu;

            auto item = Profiler::QueueSerial();
            MemWrite(&item->hdr.type, QueueType::GpuNewContext);
            MemWrite(&item->gpuNewContext.cpuTime, tcpu);
            MemWrite(&item->gpuNewContext.gpuTime, tgpu);
            memset(&item->gpuNewContext.thread, 0, sizeof(item->gpuNewContext.thread));
            MemWrite(&item->gpuNewContext.period, period);
            MemWrite(&item->gpuNewContext.type, GpuContextType::tt_device);
            MemWrite(&item->gpuNewContext.context, GetId());
            MemWrite(&item->gpuNewContext.flags, GpuContextCalibration);
#ifdef TRACY_ON_DEMAND
            GetProfiler().DeferItem(*item);
#endif
            Profiler::QueueSerialFinish();
        }

        void Name( const char* name, uint16_t len )
        {
            auto ptr = (char*)tracy_malloc( len );
            memcpy( ptr, name, len );

            auto item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::GpuContextName );
            MemWrite( &item->gpuContextNameFat.context, GetId() );
            MemWrite( &item->gpuContextNameFat.ptr, (uint64_t)ptr );
            MemWrite( &item->gpuContextNameFat.size, len );
#ifdef TRACY_ON_DEMAND
            GetProfiler().DeferItem( *item );
#endif
            Profiler::QueueSerialFinish();
        }

        void Collect(std::map<uint32_t, std::map<TTDeviceEvent, uint64_t ,TTDeviceEvent_cmp>>& device_data)
        {
            ZoneScopedC(Color::Red4);

            if (m_tail == m_head) return;

#ifdef TRACY_ON_DEMAND
            if (!GetProfiler().IsConnected())
            {
                m_head = m_tail = 0;
            }
#endif


            for (; m_tail != m_head; m_tail = (m_tail + 1) % QueryCount)
            {

                ZoneScopedNC("Add Marker", Color::Red4);
                EventInfo eventInfo = GetQuery(m_tail);

                uint64_t threadID = eventInfo.event.get_thread_id();
                TTDeviceEvent event = eventInfo.event;
                uint64_t timeShift = 0;

                if (eventInfo.phase == EventPhase::Begin)
                {
                    if (eventInfo.event.marker == 0)
                    {
                        event.marker = 1;
                    }
                    else if (eventInfo.event.marker == 1)
                    {
                        event.marker = 2;
                    }
                    else
                    {
                        timeShift = -10;
                    }
                }
                else
                {
                    if (eventInfo.event.marker == 0)
                    {
                        event.marker = 4;
                    }
                    else if (eventInfo.event.marker == 1)
                    {
                        event.marker = 3;
                    }
                    else
                    {
                        timeShift = 10;
                    }
                }

                if (device_data.find(event.run_num) != device_data.end() &&
                        device_data[event.run_num].find (event) != device_data[event.run_num].end())
                {
                    uint64_t timestamp = device_data[event.run_num].at(event);

                    auto item = Profiler::QueueSerial();
                    MemWrite(&item->hdr.type, QueueType::GpuTime);
                    MemWrite(&item->gpuTime.gpuTime, (uint64_t)(timestamp) + timeShift);
                    MemWrite(&item->gpuTime.queryId, (uint16_t)m_tail);
                    MemWrite(&item->gpuTime.context, GetId());
                    Profiler::QueueSerialFinish();

//#define TT_DEBUG
#ifdef TT_DEBUG
                    static int counter = 0;
                    counter++;
                    std::cout << counter << ","\
                        << event.marker << ","\
                        << event.risc << ","\
                        << event.core_x << ","\
                        << event.core_y << ","\
                        << timestamp << std::endl;
#endif

                }

            }
        }

        tracy_force_inline uint8_t GetId() const
        {
            return m_contextId;
        }

        tracy_force_inline unsigned int NextQueryId(EventInfo eventInfo)
        {
            const auto id = m_head;
            m_head = (m_head + 1) % QueryCount;
            TRACY_TT_ASSERT(m_head != m_tail);
            m_query[id] = eventInfo;
            return id;
        }

        tracy_force_inline EventInfo& GetQuery(unsigned int id)
        {
            TRACY_TT_ASSERT(id < QueryCount);
            return m_query[id];
        }

    private:

        uint8_t m_contextId;

        EventInfo m_query[QueryCount];
        unsigned int m_head; // index at which a new event should be inserted
        unsigned int m_tail; // oldest event

    };

    class TTCtxScope {
    public:
        uint32_t m_threadID;
        tracy_force_inline TTCtxScope(TTCtx* ctx, const SourceLocationData* srcLoc, bool is_active, int threadID)
#ifdef TRACY_ON_DEMAND
            : m_active(is_active&& GetProfiler().IsConnected())
#else
            : m_active(is_active)
#endif
            , m_ctx(ctx)
            , m_event(TTDeviceEvent ())
            , m_threadID (threadID)
        {
            if (!m_active) return;

            ZoneScoped;
            m_beginQueryId = ctx->NextQueryId(EventInfo{ TTDeviceEvent (), EventPhase::Begin });

            auto item = Profiler::QueueSerial();
            MemWrite(&item->hdr.type, QueueType::GpuZoneBeginSerial);
            MemWrite(&item->gpuZoneBegin.cpuTime, Profiler::GetTime());
            MemWrite(&item->gpuZoneBegin.srcloc, (uint64_t)srcLoc);
            MemWrite(&item->gpuZoneBegin.thread, (uint32_t)m_threadID);
            MemWrite(&item->gpuZoneBegin.queryId, (uint16_t)m_beginQueryId);
            MemWrite(&item->gpuZoneBegin.context, ctx->GetId());
            Profiler::QueueSerialFinish();
        }

        tracy_force_inline TTCtxScope(TTCtx* ctx, const SourceLocationData* srcLoc, int depth, bool is_active, int threadID)
#ifdef TRACY_ON_DEMAND
            : m_active(is_active&& GetProfiler().IsConnected())
#else
            : m_active(is_active)
#endif
            , m_ctx(ctx)
            , m_event(TTDeviceEvent ())
            , m_threadID (threadID)
        {
            if (!m_active) return;

            m_beginQueryId = ctx->NextQueryId(EventInfo{ TTDeviceEvent (), EventPhase::Begin });

            GetProfiler().SendCallstack(depth);

            auto item = Profiler::QueueSerial();
            MemWrite(&item->hdr.type, QueueType::GpuZoneBeginCallstackSerial);
            MemWrite(&item->gpuZoneBegin.cpuTime, Profiler::GetTime());
            MemWrite(&item->gpuZoneBegin.srcloc, (uint64_t)srcLoc);
            MemWrite(&item->gpuZoneBegin.thread, (uint32_t)m_threadID);
            MemWrite(&item->gpuZoneBegin.queryId, (uint16_t)m_beginQueryId);
            MemWrite(&item->gpuZoneBegin.context, ctx->GetId());
            Profiler::QueueSerialFinish();
        }

        tracy_force_inline TTCtxScope(TTCtx* ctx, uint32_t line, const char* source, size_t sourceSz, const char* function, size_t functionSz, const char* name, size_t nameSz, uint32_t color, bool is_active, int threadID)
#ifdef TRACY_ON_DEMAND
            : m_active(is_active && GetProfiler().IsConnected())
#else
            : m_active(is_active)
#endif
            , m_ctx(ctx)
            , m_event(TTDeviceEvent ())
            , m_threadID (threadID)
        {
            if (!m_active) return;

            m_beginQueryId = ctx->NextQueryId(EventInfo{ TTDeviceEvent (), EventPhase::Begin });

            const auto srcloc = Profiler::AllocSourceLocation( line, source, sourceSz, function, functionSz, name, nameSz, color);
            auto item = Profiler::QueueSerial();
            MemWrite(&item->hdr.type, QueueType::GpuZoneBeginAllocSrcLocSerial);
            MemWrite(&item->gpuZoneBegin.cpuTime, Profiler::GetTime());
            MemWrite(&item->gpuZoneBegin.srcloc, srcloc);
            MemWrite(&item->gpuZoneBegin.thread, (uint32_t)m_threadID);
            MemWrite(&item->gpuZoneBegin.queryId, (uint16_t)m_beginQueryId);
            MemWrite(&item->gpuZoneBegin.context, ctx->GetId());
            Profiler::QueueSerialFinish();
        }

        tracy_force_inline TTCtxScope(TTCtx* ctx, uint32_t line, const char* source, size_t sourceSz, const char* function, size_t functionSz, const char* name, size_t nameSz, uint32_t color, int depth, bool is_active, int threadID)
#ifdef TRACY_ON_DEMAND
            : m_active(is_active && GetProfiler().IsConnected())
#else
            : m_active(is_active)
#endif
            , m_ctx(ctx)
            , m_event(TTDeviceEvent ())
            , m_threadID (threadID)
        {
            if (!m_active) return;

            m_beginQueryId = ctx->NextQueryId(EventInfo{ TTDeviceEvent (), EventPhase::Begin });

            const auto srcloc = Profiler::AllocSourceLocation( line, source, sourceSz, function, functionSz, name, nameSz, color);
            auto item = Profiler::QueueSerialCallstack( Callstack( depth ) );
            MemWrite(&item->hdr.type, QueueType::GpuZoneBeginAllocSrcLocCallstackSerial);
            MemWrite(&item->gpuZoneBegin.cpuTime, Profiler::GetTime());
            MemWrite(&item->gpuZoneBegin.srcloc, srcloc);
            MemWrite(&item->gpuZoneBegin.thread, (uint32_t)m_threadID);
            MemWrite(&item->gpuZoneBegin.queryId, (uint16_t)m_beginQueryId);
            MemWrite(&item->gpuZoneBegin.context, ctx->GetId());
            Profiler::QueueSerialFinish();
        }
        tracy_force_inline void SetEvent(TTDeviceEvent event)
        {
            if (!m_active) return;
            m_event = event;
            m_ctx->GetQuery(m_beginQueryId).event = m_event;
        }

        tracy_force_inline ~TTCtxScope()
        {
            if (!m_active) return;
            const auto queryId = m_ctx->NextQueryId(EventInfo{ m_event, EventPhase::End });

            auto item = Profiler::QueueSerial();
            MemWrite(&item->hdr.type, QueueType::GpuZoneEndSerial);
            MemWrite(&item->gpuZoneEnd.cpuTime, Profiler::GetTime());
            MemWrite(&item->gpuZoneEnd.thread, (uint32_t)m_threadID);
            MemWrite(&item->gpuZoneEnd.queryId, (uint16_t)queryId);
            MemWrite(&item->gpuZoneEnd.context, m_ctx->GetId());
            Profiler::QueueSerialFinish();
        }

        const bool m_active;
        TTCtx* m_ctx;
        TTDeviceEvent m_event;
        unsigned int m_beginQueryId;
    };

    static inline TTCtx* CreateTTContext()
    {
        auto ctx = (TTCtx*)tracy_malloc(sizeof(TTCtx));
        new (ctx) TTCtx();
        return ctx;
    }


    static inline void DestroyTTContext(TTCtx* ctx)
    {
        ctx->~TTCtx();
        tracy_free(ctx);
    }

}  // namespace tracy

using TracyTTCtx = tracy::TTCtx*;

#define TracyTTContext() tracy::CreateTTContext();
#define TracyTTDestroy(ctx) tracy::DestroyTTContext(ctx);
#define TracyTTContextName(ctx, name, size) ctx->Name(name, size)
#if defined TRACY_HAS_CALLSTACK && defined TRACY_CALLSTACK
#  define TracyTTNamedZone(ctx, varname, name, active, threadID) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,TracyLine) { name, TracyFunction, TracyFile, (uint32_t)TracyLine, 0 }; tracy::TTCtxScope varname(ctx, &TracyConcat(__tracy_gpu_source_location,TracyLine), TRACY_CALLSTACK, active, threadID );
#  define TracyTTNamedZoneC(ctx, varname, name, color, active, threadID) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,TracyLine) { name, TracyFunction, TracyFile, (uint32_t)TracyLine, color }; tracy::TTCtxScope varname(ctx, &TracyConcat(__tracy_gpu_source_location,TracyLine), TRACY_CALLSTACK, active , threadID);
#  define TracyTTZone(ctx, name, threadID) TracyTTNamedZoneS(ctx, __tracy_gpu_zone, name, TRACY_CALLSTACK, true, threadID)
#  define TracyTTZoneC(ctx, name, color, threadID) TracyTTNamedZoneCS(ctx, __tracy_gpu_zone, name, color, TRACY_CALLSTACK, true, threadID)
#  define TracyTTZoneTransient( ctx, varname, name, color, active, threadID ) tracy::TTCtxScope varname( ctx, TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), name, strlen( name ), color, TRACY_CALLSTACK, active , threadID);
#else
#  define TracyTTNamedZone(ctx, varname, name, active, threadID) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,TracyLine){ name, TracyFunction, TracyFile, (uint32_t)TracyLine, 0 }; tracy::TTCtxScope varname(ctx, &TracyConcat(__tracy_gpu_source_location,TracyLine), active, threadID);
#  define TracyTTNamedZoneC(ctx, varname, name, color, active, threadID) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,TracyLine){ name, TracyFunction, TracyFile, (uint32_t)TracyLine, color }; tracy::TTCtxScope varname(ctx, &TracyConcat(__tracy_gpu_source_location,TracyLine), active, threadID);
#  define TracyTTZone(ctx, name, threadID) TracyTTNamedZone(ctx, __tracy_gpu_zone, name, true, threadID)
#  define TracyTTZoneC(ctx, name, color, threadID) TracyTTNamedZoneC(ctx, __tracy_gpu_zone, name, color, true , threadID)
#  define TracyTTZoneTransient( ctx, varname, name, color, active, threadID ) tracy::TTCtxScope varname( ctx, TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), name, strlen( name ), color, active , threadID);
#endif

#ifdef TRACY_HAS_CALLSTACK
#  define TracyTTNamedZoneS(ctx, varname, name, depth, active, threadID) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,TracyLine){ name, TracyFunction, TracyFile, (uint32_t)TracyLine, 0 }; tracy::TTCtxScope varname(ctx, &TracyConcat(__tracy_gpu_source_location,TracyLine), depth, active, threadID);
#  define TracyTTNamedZoneCS(ctx, varname, name, color, depth, active, threadID) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,TracyLine){ name, TracyFunction, TracyFile, (uint32_t)TracyLine, color }; tracy::TTCtxScope varname(ctx, &TracyConcat(__tracy_gpu_source_location,TracyLine), depth, active, threadID);
#  define TracyTTZoneS(ctx, name, depth, threadID) TracyTTNamedZoneS(ctx, __tracy_gpu_zone, name, depth, true, 0, threadID)
#  define TracyTTZoneCS(ctx, name, color, depth, threadID) TracyTTNamedZoneCS(ctx, __tracy_gpu_zone, name, color, depth, true, 0, threadID)
#  define TracyTTZoneTransientS( ctx, varname, name, depth, active , threadID) tracy::TTCtxScope varname( ctx, TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), name, strlen( name ), color, depth, active , threadID);
#else
#  define TracyTTNamedZoneS(ctx, varname, name, depth, active, threadID) TracyTTNamedZone(ctx, varname, name, active, 0, 0, threadID)
#  define TracyTTNamedZoneCS(ctx, varname, name, color, depth, active, threadID) TracyTTNamedZoneC(ctx, varname, name, color, active, 0, threadID)
#  define TracyTTZoneS(ctx, name, depth, threadID) TracyTTZone(ctx, name, threadID)
#  define TracyTTZoneCS(ctx, name, color, depth, threadID) TracyTTZoneC(ctx, name, color, threadID)
#  define TracyTTZoneTransientS( ctx, varname, name, depth, color, active , threadID) TracyTTZoneTransient( ctx, varname, name, color, active , threadID)
#endif

#define TracyTTNamedZoneSetEvent(varname, event) varname.SetEvent(event)
#define TracyTTZoneSetEvent(event) __tracy_gpu_zone.SetEvent(event)

#define TracyTTCollect(ctx, deviceData) ctx->Collect(deviceData)

#define TracySetCpuTime() tracy::set_cpu_time();
#endif

#endif
