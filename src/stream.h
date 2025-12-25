#include <cstdint>
#include <memory>
#include <cassert>
#include <cstring>
#include "utils/utils.h"
#include "radix-tree.h"
namespace REDIS_NAMESPACE
{
    enum class StreamErrorType
    {
        OK,
        INVALID_ID_FORMAT,
        ID_NOT_GREATER_THAN_LAST
    };

    struct StreamID
    {
        uint64_t ms{};
        uint64_t seq{};
        inline std::string toBinary() const
        {
            uint64_t ms_be = toBigEndian64(ms);
            uint64_t seq_be = toBigEndian64(seq);
            std::string out;
            out.resize(16);
            std::memcpy(&out[0], &ms_be, sizeof(ms_be));
            std::memcpy(&out[8], &seq_be, sizeof(seq_be));
            return out;
        }
        static inline StreamID fromBinary(const std::string &string)
        {
            assert(string.size() >= 16);
            uint64_t ms_be;
            uint64_t seq_be;
            std::memcpy(&ms_be, &string[0], 8);
            std::memcpy(&seq_be, &string[8], 8);
            StreamID s;
            s.ms = toBigEndian64(ms_be);
            s.seq = toBigEndian64(seq_be);
            return s;
        }
        inline bool isGreater(const StreamID &other) const
        {
            if (ms > other.ms)
                return true;
            if (ms < other.ms)
                return false;
            // ms values are equal, compare seq
            return seq > other.seq;
        }
        static inline bool fromString(std::string_view str, StreamID &out)
        {
            // Parse format: "ms-seq" (e.g., "1526919030474-0")
            size_t dashPos = str.find('-');
            if (dashPos == std::string_view::npos)
                return false;

            std::string_view msStr = str.substr(0, dashPos);
            std::string_view seqStr = str.substr(dashPos + 1);

            unsigned long long ms_val, seq_val;
            if (!convert_positive_string_to_number(msStr, ms_val))
                return false;
            if (!convert_positive_string_to_number(seqStr, seq_val))
                return false;

            out.ms = ms_val;
            out.seq = seq_val;
            return true;
        }
    };

    class Stream
    {
    private:
        std::unique_ptr<RadixTree> radixTreePtr{};
        StreamID lastInsertStreamID{};

    public:
        StreamErrorType insert(std::string_view streamId, std::string_view value);
    };
}
