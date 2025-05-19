#include "generator.h"
#include "file_reader.h"

#include <library/cpp/yson/node/node_io.h>

#include <util/generic/hash.h>
#include <util/stream/file.h>
#include <util/system/file.h>

using namespace NOracle;

class NFileGen::TGenerator::TImpl {
    TFileReader FileReader_;
    NCore::TRequest LastRequest_;
    ui64 TotalRows_ = 0;

public:
    TImpl(TString filePath)
        : FileReader_(TFileInput(filePath))
    {
        FileReader_.ReadBytes(&TotalRows_, sizeof(TotalRows_));
    }

    NCore::TRequest* Next() {
        ui64 ts = 0;
        ui64 hash = 0;
        ui64 uidHash = 0;
        try {
            FileReader_.ReadBytes(&ts, sizeof(ts));
            FileReader_.ReadBytes(&hash, sizeof(hash));
            FileReader_.ReadBytes(&uidHash, sizeof(uidHash));
        } catch (...) {
            return nullptr;
        }
        if (ts == 0) {
            return nullptr;
        }
        NCore::TRequest req;
        req.Timestamp = TInstant::Seconds(ts);
        req.HashKey = hash;
        req.Uid = uidHash;
        Y_ENSURE(req.Timestamp >= LastRequest_.Timestamp, ToString(req.Timestamp) + " <= " + ToString(LastRequest_.Timestamp));
        LastRequest_ = req;
        return &LastRequest_;
    }
};

NFileGen::TGenerator::TGenerator(TString filePath)
    : Pimpl_(MakeHolder<TImpl>(filePath))
{
}

NFileGen::TGenerator::~TGenerator() {
}

NCore::TRequest* NFileGen::TGenerator::Next() {
    return Pimpl_->Next();
}
