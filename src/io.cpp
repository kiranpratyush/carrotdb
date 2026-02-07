#include "io.h"
#include <memory>

namespace IO_NAMESPACE
{

    std::vector<unsigned char> RIO::read(size_t bytes)
    {
        if (!fileStream.is_open())
            return {};
        std::vector<unsigned char> memblocks(bytes);
        fileStream.read(reinterpret_cast<char *>(memblocks.data()), bytes);
        auto bytesRead = fileStream.gcount();
        if (bytesRead == 0)
            return {};
        if (bytesRead < bytes)
        {
            memblocks.resize(bytesRead);
        }
        return memblocks;
    }

    bool RIO::write(std::vector<unsigned char> data)
    {
        if (!fileStream.is_open() || data.empty())
            return false;
        fileStream.write(reinterpret_cast<char *>(data.data()), data.size());
        return fileStream.good();
    }

    void RIO::seekBackRead(size_t bytes)
    {
        if (!fileStream.is_open())
            return;

        auto currentPos = fileStream.tellg();
        if (currentPos >= static_cast<std::streampos>(bytes))
        {
            fileStream.seekg(currentPos - static_cast<std::streampos>(bytes));
        }
    }

}
