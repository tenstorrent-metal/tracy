#ifndef __TRACYTTDEVICEDATA_HPP__
#define __TRACYTTDEVICEDATA_HPP__

namespace tracy
{
    static std::string riscName[] = {"BRISC", "NCRISC", "TRISC_0", "TRISC_1", "TRISC_2", "ERISC"};
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

        TTDeviceEvent (): run_num(-1),chip_id(-1),core_x(-1),core_y(-1),risc(-1),marker(-1)
        {
        }

        TTDeviceEvent (
                uint64_t run_num,
                uint64_t chip_id,
                uint64_t core_x,
                uint64_t core_y,
                uint64_t risc,
                uint64_t marker
                ): run_num(run_num),chip_id(chip_id),core_x(core_x),core_y(core_y),risc(risc),marker(marker)
        {
        }

        TTDeviceEvent (uint64_t threadID) :run_num(-1),marker(-1)
        {
            risc = (threadID) & ((1 << RISC_BIT_COUNT) - 1);
            core_x = (threadID >> CORE_X_BIT_SHIFT) & ((1 << CORE_X_BIT_COUNT) - 1);
            core_y = (threadID >> CORE_Y_BIT_SHIFT) & ((1 << CORE_Y_BIT_COUNT) - 1);
            chip_id = (threadID >> CHIP_BIT_SHIFT) & ((1 << CHIP_BIT_COUNT) - 1);
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

    struct TTDeviceEvent_cmp
    {
        bool operator() ( TTDeviceEvent a, TTDeviceEvent b ) const
        {
            if (a.get_thread_id() < b.get_thread_id())
            {
                return true;
            }
            else if (a.get_thread_id() > b.get_thread_id())
            {
                return false;
            }
            else
            {
                return a.marker < b.marker;
            }

        }
    };
}
#endif
