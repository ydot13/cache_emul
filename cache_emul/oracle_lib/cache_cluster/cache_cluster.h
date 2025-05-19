#pragma once

#include "remote_cache_instance.h"

#include <cache_emul/oracle_lib/cache_cluster/proto/config.pb.h>
#include <cache_emul/oracle_lib/core/statistic.h>

#include <util/generic/vector.h>
#include <util/generic/xrange.h>
#include <util/string/util.h>

namespace NOracle {

constexpr ui64 ExpectedRequests = 1'000'000'000;

class TCacheCluster {
    TInstant FlushTime_ = TInstant::Zero();
    bool WasFlushed_ = false;
    TVector<TRemoteCacheInstance> Instances_;
    NProto::TDataCenterConfig Config_;
    ui64 AvgMsgSize_ = 0;
public:
    TCacheCluster(const NProto::TDataCenterConfig& cfg, ui64 avgMsgSize)
        : Config_(cfg)
        , AvgMsgSize_(avgMsgSize)
    {
        ui64 expectedRequests = ExpectedRequests / Config_.GetCacheInstanceCount();
        FlushTime_ = TInstant::Seconds(cfg.GetFlushTimestamp());
        for (size_t i = 0; i < cfg.GetCacheInstanceCount(); ++i) {
            Instances_.push_back(
                TRemoteCacheInstance(
                    cfg.GetCacheAlgo(),
                    cfg.GetInstanceArenas(),
                    cfg.GetInstanceMaxSize(),
                    avgMsgSize,
                    expectedRequests
                )
            );
        }
    }

    bool IsOpen(uint64_t ts) const noexcept {
        if (Config_.HasCloseTimestamp()) {
            return ts < Config_.GetCloseTimestamp() || ts > Config_.GetOpenTimestamp();
        }
        return true;
    }

    TString Name() const noexcept {
        return Config_.GetName();
    }

    ui32 Weight() const noexcept {
        return Config_.GetWeight();
    }

    bool IsCacheHitAndStoreOnMiss(const TInstant& ts, ui64 hashKey, NCore::TStatistic& stat) {
        if (Y_UNLIKELY(!WasFlushed_ && ts >= FlushTime_)) {
            WasFlushed_ = true;
            Instances_.clear();
            ui64 expectedRequests = ExpectedRequests / Config_.GetCacheInstanceCount();
            for (size_t i = 0; i < Config_.GetCacheInstanceCount(); ++i) {
                Instances_.push_back(TRemoteCacheInstance(Config_.GetCacheAlgo(), Config_.GetInstanceArenas(), Config_.GetInstanceMaxSize(), AvgMsgSize_, expectedRequests));
            }
        }
        return Instances_[hashKey % Instances_.size()].IsCacheHitAndStoreOnMiss(hashKey, stat);
    }

    TString Dump() const {
        TString res;
        for (const auto& instance : Instances_) {
            auto sizes = instance.Size();
            for (auto arena_size : sizes) {
                res += ToString(arena_size) + " ";
            }
            res += "\n";
        }
        return res;
    }
};

} // namespace NOracle
