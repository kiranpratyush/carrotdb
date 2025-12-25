#include "stream.h"

namespace REDIS_NAMESPACE
{
    StreamErrorType Stream::insert(std::string_view streamId, std::string_view value)
    {
        // Step 1: Convert string to StreamID
        StreamID sid;
        if (!StreamID::fromString(streamId, sid))
        {
            // Invalid stream ID format
            return StreamErrorType::INVALID_ID_FORMAT;
        }

        // Step 2: Check if the new ID is greater than the last inserted ID
        if (lastInsertStreamID.ms != 0 || lastInsertStreamID.seq != 0)
        {
            if (!sid.isGreater(lastInsertStreamID))
            {
                return StreamErrorType::ID_NOT_GREATER_THAN_LAST;
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
        
        return StreamErrorType::OK;
    }
}
