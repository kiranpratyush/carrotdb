#include "stream.h"

namespace REDIS_NAMESPACE
{
    StreamStatus Stream::insert(std::string_view streamId, std::string_view value)
    {
        // Step 1: Convert string to StreamID
        StreamID sid;
        if (!fromString(streamId, sid))
        {
            // Invalid stream ID format
            return StreamStatus{StreamErrorType::INVALID_ID_FORMAT, ""};
        }
        if (sid.ms == 0 && sid.seq == 0)
            return StreamStatus{StreamErrorType::NOT_GREATER_THAN_ZERO, ""};

        // Step 2: Check if the new ID is greater than the last inserted ID
        if (lastInsertStreamID.ms != 0 || lastInsertStreamID.seq != 0)
        {
            if (!sid.isGreater(lastInsertStreamID))
            {
                return StreamStatus{StreamErrorType::ID_NOT_GREATER_THAN_LAST, ""};
            }
        }

        // Step 3: Convert StreamID to binary format
        std::string binaryKey = sid.toBinary();

        // Step 4: Store in radix tree
        if (!radixTreePtr)
        {
            radixTreePtr = std::make_unique<RadixTree>();
        }
        radixTreePtr->insert(binaryKey, value);

        // Update last inserted stream ID
        lastInsertStreamID = sid;

        return StreamStatus{StreamErrorType::OK, sid.toString()};
    }
    bool Stream::fromString(std::string_view str, StreamID &out)
    {

        size_t dashPos = str.find('-');
        if (dashPos == std::string_view::npos)
            return false;

        std::string_view msStr = str.substr(0, dashPos);
        std::string_view seqStr = str.substr(dashPos + 1);

        unsigned long long ms_val, seq_val;
        if (!convert_positive_string_to_number(msStr, ms_val))
            return false;

        if (seqStr == "*")
        {
            if (lastInsertStreamID.ms == ms_val)
                seq_val = lastInsertStreamID.seq + 1;
            else
                seq_val = 0;
        }
        else
        {
            if (!convert_positive_string_to_number(seqStr, seq_val))
                return false;
        }

        out.ms = ms_val;
        out.seq = seq_val;
        return true;
    }
}
