#pragma once

#include <cache_emul/oracle_lib/proto/request.pb.h>

#include <library/cpp/yson/node/node.h>

#include <util/datetime/base.h>
#include <util/system/types.h>
#include <util/string/split.h>

namespace NOracle::NCore {

struct TRequest {
    ui64 HashKey = 0;
    ui64 Uid = 0;
    TInstant Timestamp;

    static TRequest From(const TString& line) {
        NProto::TRequest requestProto;
        requestProto.ParseFromStringOrThrow(line);
        TRequest request;
        request.Timestamp = TInstant::Seconds(requestProto.GetTimestamp());
        request.HashKey = requestProto.GetHash();
        request.Uid = requestProto.GetUidHash();
        return request;
    }
};

} // namespace NOracle::NCore

