#ifndef __TRACYTTDEVICE_HPP__
#define __TRACYTTDEVICE_HPP__

#if !defined TRACY_ENABLE

#define TracyTTContext() nullptr
#define TracyTTDestroy(c)
#define TracyTTContextName(c, x, y)
#define TracyTTContextPopulate(c, x, y)
#define TracyTTPushStartZone(c, e)
#define TracyTTPushEndZone(c, e)

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

        void PushStartZone (const TTDeviceEvent& event)
        {
            constexpr std::array<int,6> customColors = {
                tracy::Color::Orange2,
                tracy::Color::SeaGreen3,
                tracy::Color::SkyBlue3,
                tracy::Color::Turquoise2,
                tracy::Color::CadetBlue1,
                tracy::Color::Yellow3
            };

            const auto queryId = this->NextQueryId(EventInfo{ event, EventPhase::Begin });

            const auto srcloc = Profiler::AllocSourceLocation(
                    event.line,
                    event.file.c_str(),
                    event.file.length(),
                    event.zone_name.c_str(),
                    event.zone_name.length(),
                    event.zone_name.c_str(),
                    event.zone_name.length(),
                    customColors[event.risc % customColors.size()]);

            auto zoneBegin = Profiler::QueueSerial();
            MemWrite(&zoneBegin->hdr.type, QueueType::GpuZoneBeginAllocSrcLocSerial);
            MemWrite(&zoneBegin->gpuZoneBegin.cpuTime, Profiler::GetTime());
            MemWrite(&zoneBegin->gpuZoneBegin.srcloc, srcloc);
            MemWrite(&zoneBegin->gpuZoneBegin.thread, (uint32_t)event.get_thread_id());
            MemWrite(&zoneBegin->gpuZoneBegin.queryId, (uint16_t)queryId);
            MemWrite(&zoneBegin->gpuZoneBegin.context, this->GetId());
            Profiler::QueueSerialFinish();

            auto zoneTime = Profiler::QueueSerial();
            MemWrite(&zoneTime->hdr.type, QueueType::GpuTime);
            MemWrite(&zoneTime->gpuTime.gpuTime, (uint64_t)event.timestamp);
            MemWrite(&zoneTime->gpuTime.queryId, (uint16_t)queryId);
            MemWrite(&zoneTime->gpuTime.context, this->GetId());
            Profiler::QueueSerialFinish();
        }

        void PushEndZone (const TTDeviceEvent& event)
        {
            const auto queryId = this->NextQueryId(EventInfo{ event, EventPhase::End });

            auto zoneEnd = Profiler::QueueSerial();
            MemWrite(&zoneEnd->hdr.type, QueueType::GpuZoneEndSerial);
            MemWrite(&zoneEnd->gpuZoneEnd.cpuTime, Profiler::GetTime());
            MemWrite(&zoneEnd->gpuZoneBegin.thread, (uint32_t)event.get_thread_id());
            MemWrite(&zoneEnd->gpuZoneBegin.queryId, (uint16_t)queryId);
            MemWrite(&zoneEnd->gpuZoneBegin.context, this->GetId());
            Profiler::QueueSerialFinish();
            
            auto zoneTime = Profiler::QueueSerial();
            MemWrite(&zoneTime->hdr.type, QueueType::GpuTime);
            MemWrite(&zoneTime->gpuTime.gpuTime, (uint64_t)event.timestamp);
            MemWrite(&zoneTime->gpuTime.queryId, (uint16_t)queryId);
            MemWrite(&zoneTime->gpuTime.context, this->GetId());
            Profiler::QueueSerialFinish();
        }

    private:

        uint8_t m_contextId;

        EventInfo m_query[QueryCount];
        unsigned int m_head; // index at which a new event should be inserted
        unsigned int m_tail; // oldest event

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
#define TracyTTContextPopulate(ctx, timeshift, frequency) ctx->PopulateTTContext(timeshift, frequency)
#define TracyTTPushStartZone(ctx, event) ctx->PushStartZone(event);
#define TracyTTPushEndZone(ctx, event) ctx->PushEndZone(event);

#define TracySetCpuTime() tracy::set_cpu_time();

#endif
#endif
