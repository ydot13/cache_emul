#pragma once

#include <util/generic/ptr.h>
#include <util/system/types.h>

#include <cache_emul/oracle_lib/cache_cluster/proto/config.pb.h>
#include <cache_emul/oracle_lib/core/statistic.h>

namespace NOracle {

class TRemoteCacheInstance {
    class TImpl;

public:
    TRemoteCacheInstance(
        NProto::ECacheAlgo cacheAlgo,
        ui64 numArenas, ui64 maxSize, ui64 avgMsgSize, ui64 expectedRequests
    );
    TRemoteCacheInstance(TRemoteCacheInstance&&);
    ~TRemoteCacheInstance();

    TVector<size_t> Size() const;
    bool IsCacheHitAndStoreOnMiss(ui64 hashKey, NCore::TStatistic& stat);

private:
    THolder<TImpl> Pimpl_;
};

} // namespace NOracle
