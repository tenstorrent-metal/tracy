#ifndef __TRACYTTDEVICEDATA_HPP__
#define __TRACYTTDEVICEDATA_HPP__

namespace tracy
{
    static std::string riscName[] = {"BRISC", "NCRISC", "TRISC_0", "TRISC_1", "TRISC_2", "ERISC"};

    enum TTDeviceEventPhase
    {
        begin,
        end,
        sum
    };

    struct TTDeviceEvent
    {
        static constexpr uint64_t RISC_BIT_COUNT =  3;
        static constexpr uint64_t CORE_X_BIT_COUNT =  4;
        static constexpr uint64_t CORE_Y_BIT_COUNT =  4;
        static constexpr uint64_t CHIP_BIT_COUNT =  8;
        static constexpr uint64_t RUN_NUM_BIT_COUNT =  16;

        static constexpr uint64_t CORE_X_BIT_SHIFT = RISC_BIT_COUNT;
        static constexpr uint64_t CORE_Y_BIT_SHIFT = CORE_X_BIT_SHIFT + CORE_X_BIT_COUNT;
        static constexpr uint64_t CHIP_BIT_SHIFT = CORE_Y_BIT_SHIFT + CORE_Y_BIT_COUNT;
        static constexpr uint64_t RUN_NUM_BIT_SHIFT = CHIP_BIT_SHIFT + CHIP_BIT_COUNT;

        static constexpr uint64_t INVALID_NUM = 1LL << 63;

        static_assert ((RISC_BIT_COUNT +
                    CORE_X_BIT_COUNT +
                    CORE_Y_BIT_COUNT +
                    CHIP_BIT_COUNT +
                    RUN_NUM_BIT_COUNT) <= (sizeof(uint64_t) * 8));

        uint64_t run_num;
        uint64_t chip_id;
        uint64_t core_x;
        uint64_t core_y;
        uint64_t risc;
        uint64_t marker;
        uint64_t timestamp;
        uint64_t line;
        std::string file;
        std::string zone_name;
        TTDeviceEventPhase zone_phase;

        TTDeviceEvent (): 
            run_num(INVALID_NUM),
            chip_id(INVALID_NUM),
            core_x(INVALID_NUM),
            core_y(INVALID_NUM),
            risc(INVALID_NUM),
            marker(INVALID_NUM),
            timestamp(INVALID_NUM),
            line(INVALID_NUM),
            file(""),
            zone_name(""),
            zone_phase(begin)
        {
        }

        TTDeviceEvent (
                uint64_t run_num,
                uint64_t chip_id,
                uint64_t core_x,
                uint64_t core_y,
                uint64_t risc,
                uint64_t marker,
                uint64_t timestamp,
                uint64_t line,
                std::string file,
                std::string zone_name,
                TTDeviceEventPhase zone_phase
                ): run_num(run_num),chip_id(chip_id),core_x(core_x),core_y(core_y),risc(risc),marker(marker),timestamp(timestamp),line(line),file(file),zone_name(zone_name),zone_phase(zone_phase)
        {
        }

        TTDeviceEvent (uint64_t threadID) :run_num(-1),marker(-1)
        {
            risc = (threadID) & ((1 << RISC_BIT_COUNT) - 1);
            core_x = (threadID >> CORE_X_BIT_SHIFT) & ((1 << CORE_X_BIT_COUNT) - 1);
            core_y = (threadID >> CORE_Y_BIT_SHIFT) & ((1 << CORE_Y_BIT_COUNT) - 1);
            chip_id = (threadID >> CHIP_BIT_SHIFT) & ((1 << CHIP_BIT_COUNT) - 1);
        }

        friend bool operator<(const TTDeviceEvent& lhs, const TTDeviceEvent& rhs)
        {
            return std::tie(lhs.timestamp, lhs.chip_id, lhs.core_x, lhs.core_y, lhs.risc, lhs.marker) < std::tie(rhs.timestamp, rhs.chip_id, rhs.core_x, rhs.core_y, rhs.risc, rhs.marker);
        }

        uint64_t get_thread_id() const
        {
            uint64_t threadID = risc |\
                               core_x << CORE_X_BIT_SHIFT |\
                               core_y << CORE_Y_BIT_SHIFT |\
                               chip_id << CHIP_BIT_SHIFT;

            return threadID;
        }
    };


}
#endif
