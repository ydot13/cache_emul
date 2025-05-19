#pragma once

#include <util/generic/yexception.h>
#include <util/stream/file.h>

namespace NOracle::NFileGen {

class TFileReader : public TFileInput {
public:
    TFileReader(TFileInput&& fin) : TFileInput(std::move(fin)) {}
    void ReadBytes(void* buff, size_t len) {
        size_t currLen = len;
        while (currLen > 0) {
            auto readBytes = Read(&static_cast<char*>(buff)[len - currLen], currLen);
            Y_ENSURE(readBytes, "failed to read");
            currLen -= readBytes;
        }
    }
};

}