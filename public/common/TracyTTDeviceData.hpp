#ifndef __TRACYTTDEVICEDATA_HPP__
#define __TRACYTTDEVICEDATA_HPP__

namespace tracy {
inline std::string riscName[] = {"BRISC", "NCRISC", "TRISC_0", "TRISC_1", "TRISC_2", "ERISC", "CORE_AGG"};

enum class TTDeviceMarkerType : uint8_t { START, END, TOTAL, TS_DATA, TS_EVENT };

struct MarkerDetails {
    enum class MarkerNameKeyword : uint16_t {
        BRISC_FW,
        ERISC_FW,
        SYNC_ZONE,
        PROFILER,
        DISPATCH,
        PROCESS_CMD,
        RUNTIME_HOST_ID_DISPATCH,
        PACKED_DATA_DISPATCH,
        PACKED_LARGE_DATA_DISPATCH,
        COUNT
    };

    static inline std::unordered_map<std::string, MarkerNameKeyword> marker_name_keywords_map = {
        {"BRISC-FW", MarkerNameKeyword::BRISC_FW},
        {"ERISC-FW", MarkerNameKeyword::ERISC_FW},
        {"SYNC-ZONE", MarkerNameKeyword::SYNC_ZONE},
        {"PROFILER", MarkerNameKeyword::PROFILER},
        {"DISPATCH", MarkerNameKeyword::DISPATCH},
        {"process_cmd", MarkerNameKeyword::PROCESS_CMD},
        {"runtime_host_id_dispatch", MarkerNameKeyword::RUNTIME_HOST_ID_DISPATCH},
        {"packed_data_dispatch", MarkerNameKeyword::PACKED_DATA_DISPATCH},
        {"packed_large_data_dispatch", MarkerNameKeyword::PACKED_LARGE_DATA_DISPATCH},
    };

    std::string marker_name;
    std::string source_file;
    uint64_t source_line_num;
    std::array<bool, static_cast<uint16_t>(MarkerNameKeyword::COUNT)> marker_name_keyword_flags;

    MarkerDetails(const std::string& marker_name, const std::string& source_file, uint64_t source_line_num) :
        marker_name(marker_name), source_file(source_file), source_line_num(source_line_num) {
        for (const auto& [keyword_str, keyword] : marker_name_keywords_map) {
            marker_name_keyword_flags[static_cast<uint16_t>(keyword)] =
                marker_name.find(keyword_str) != std::string::npos;
        }
    }
};

const MarkerDetails UnidentifiedMarkerDetails = MarkerDetails("", "", 0);

// put all data in this struct
struct TTDeviceMarker {
    static constexpr uint64_t RISC_BIT_COUNT = 3;
    static constexpr uint64_t CORE_X_BIT_COUNT = 4;
    static constexpr uint64_t CORE_Y_BIT_COUNT = 4;
    static constexpr uint64_t CHIP_BIT_COUNT = 8;
    static constexpr uint64_t RUNTIME_HOST_ID_BIT_COUNT = 16;

    static constexpr uint64_t CORE_X_BIT_SHIFT = RISC_BIT_COUNT;
    static constexpr uint64_t CORE_Y_BIT_SHIFT = CORE_X_BIT_SHIFT + CORE_X_BIT_COUNT;
    static constexpr uint64_t CHIP_BIT_SHIFT = CORE_Y_BIT_SHIFT + CORE_Y_BIT_COUNT;

    static constexpr uint64_t INVALID_NUM = 1LL << 63;

    static_assert(
        (RISC_BIT_COUNT + CORE_X_BIT_COUNT + CORE_Y_BIT_COUNT + CHIP_BIT_COUNT + RUNTIME_HOST_ID_BIT_COUNT) <=
        (sizeof(uint64_t) * 8));

    uint64_t runtime_host_id;
    uint64_t chip_id;
    uint64_t core_x;
    uint64_t core_y;
    uint64_t risc;
    uint64_t marker_id;  // rename to marker_id and make it 16 bits
    uint64_t timestamp;
    uint64_t data;
    std::string op_name;
    uint64_t line;
    std::string file;
    std::string marker_name;
    TTDeviceMarkerType marker_type;
    std::array<bool, static_cast<uint16_t>(MarkerDetails::MarkerNameKeyword::COUNT)> marker_name_keyword_flags;
    nlohmann::json meta_data;

    TTDeviceMarker() :
        runtime_host_id(INVALID_NUM),
        chip_id(INVALID_NUM),
        core_x(INVALID_NUM),
        core_y(INVALID_NUM),
        risc(INVALID_NUM),
        marker_id(INVALID_NUM),
        timestamp(INVALID_NUM),
        data(INVALID_NUM),
        op_name(""),
        line(INVALID_NUM),
        file(""),
        marker_name(""),
        marker_type(TTDeviceMarkerType::START),
        marker_name_keyword_flags(std::array<bool, static_cast<uint16_t>(MarkerDetails::MarkerNameKeyword::COUNT)>()),
        meta_data(nlohmann::json::object()) {}

    TTDeviceMarker(
        uint64_t runtime_host_id,
        uint64_t chip_id,
        uint64_t core_x,
        uint64_t core_y,
        uint64_t risc,
        uint64_t marker_id,
        uint64_t timestamp,
        uint64_t data,
        const std::string& op_name,
        uint64_t line,
        const std::string& file,
        const std::string& marker_name,
        TTDeviceMarkerType marker_type,
        const std::array<bool, static_cast<uint16_t>(MarkerDetails::MarkerNameKeyword::COUNT)>& marker_name_keyword_flags,
        const nlohmann::json& meta_data) :
        runtime_host_id(runtime_host_id),
        chip_id(chip_id),
        core_x(core_x),
        core_y(core_y),
        risc(risc),
        marker_id(marker_id),
        timestamp(timestamp),
        data(data),
        op_name(op_name),
        line(line),
        file(file),
        marker_name(marker_name),
        marker_type(marker_type),
        marker_name_keyword_flags(marker_name_keyword_flags),
        meta_data(meta_data) {}

    TTDeviceMarker(uint64_t threadID) : runtime_host_id(-1), marker_id(-1) {
        risc = (threadID) & ((1 << RISC_BIT_COUNT) - 1);
        core_x = (threadID >> CORE_X_BIT_SHIFT) & ((1 << CORE_X_BIT_COUNT) - 1);
        core_y = (threadID >> CORE_Y_BIT_SHIFT) & ((1 << CORE_Y_BIT_COUNT) - 1);
        chip_id = (threadID >> CHIP_BIT_SHIFT) & ((1 << CHIP_BIT_COUNT) - 1);
    }

    friend bool operator<(const TTDeviceMarker& lhs, const TTDeviceMarker& rhs) {
        if (lhs.timestamp != rhs.timestamp) {
            return lhs.timestamp < rhs.timestamp;
        }
        if (lhs.chip_id != rhs.chip_id) {
            return lhs.chip_id < rhs.chip_id;
        }
        if (lhs.core_x != rhs.core_x) {
            return lhs.core_x < rhs.core_x;
        }
        if (lhs.core_y != rhs.core_y) {
            return lhs.core_y < rhs.core_y;
        }
        if (lhs.risc != rhs.risc) {
            return lhs.risc < rhs.risc;
        }
        return lhs.marker_id < rhs.marker_id;
    }

    friend bool operator==(const TTDeviceMarker& lhs, const TTDeviceMarker& rhs) {
        return lhs.timestamp == rhs.timestamp && lhs.chip_id == rhs.chip_id && lhs.core_x == rhs.core_x &&
               lhs.core_y == rhs.core_y && lhs.risc == rhs.risc && lhs.marker_id == rhs.marker_id;
    }

    uint64_t get_thread_id() const {
        uint64_t threadID = risc | core_x << CORE_X_BIT_SHIFT | core_y << CORE_Y_BIT_SHIFT | chip_id << CHIP_BIT_SHIFT;

        return threadID;
    }
};
}  // namespace tracy

namespace std {
template <>
struct hash<tracy::TTDeviceMarker> {
    std::size_t operator()(const tracy::TTDeviceMarker& obj) const {
        std::hash<uint64_t> hasher;
        std::size_t hash_value = 0;
        constexpr std::size_t hash_combine_prime = 0x9e3779b9;
        hash_value ^= hasher(obj.timestamp) + hash_combine_prime + (hash_value << 6) + (hash_value >> 2);
        hash_value ^= hasher(obj.chip_id) + hash_combine_prime + (hash_value << 6) + (hash_value >> 2);
        hash_value ^= hasher(obj.core_x) + hash_combine_prime + (hash_value << 6) + (hash_value >> 2);
        hash_value ^= hasher(obj.core_y) + hash_combine_prime + (hash_value << 6) + (hash_value >> 2);
        hash_value ^= hasher(obj.risc) + hash_combine_prime + (hash_value << 6) + (hash_value >> 2);
        hash_value ^= hasher(obj.marker_id) + hash_combine_prime + (hash_value << 6) + (hash_value >> 2);
        return hash_value;
    }
};
}  // namespace std
#endif
