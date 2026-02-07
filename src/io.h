#ifndef IO_H
#define IO_H
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

namespace IO_NAMESPACE
{
    class AbstractIO
    {
    public:
        std::vector<unsigned char> virtual read(size_t bytes) = 0;
        bool virtual write(std::vector<unsigned char> data) = 0;
        virtual void seekBackRead(size_t bytes) = 0;
        virtual ~AbstractIO() {};
    };

    class RIO : public AbstractIO
    {
    private:
        std::string filePath{};
        std::streampos getPos{};
        std::streampos putPos{};
        std::fstream fileStream{};

    public:
        inline RIO() : filePath{}
        {
        }
        inline void open()
        {

            if (!fileStream.is_open())
            {
                fileStream.open(filePath, std::ios::in | std::ios::binary);
                if (!fileStream.is_open())
                {
                    fileStream.open(filePath, std::ios::out | std::ios::binary);
                    fileStream.close();
                    fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
                }
                else
                {
                    // File exists, close and reopen for read/write without truncating
                    fileStream.close();
                    fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary | std::ios::ate | std::ios::ate);
                    fileStream.seekg(0, std::ios::beg);
                }
                fileStream.seekp(0, std::ios::beg);
                getPos = fileStream.tellg();
                putPos = fileStream.tellp();
            }
        }
        inline void setFilePath(std::string filePath)
        {
            this->filePath = filePath;
        }
        inline ~RIO()
        {
            if (fileStream.is_open())
            {
                fileStream.close();
            }
        }
        std::vector<unsigned char>
        read(size_t bytes) override;
        bool write(std::vector<unsigned char> data) override;
        void seekBackRead(size_t bytes) override;
    };

}

#endif