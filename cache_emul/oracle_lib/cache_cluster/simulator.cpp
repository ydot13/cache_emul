#include "simulator.h"
#include "cache_cluster.h"

#include <cache_emul/oracle_lib/core/statistic.h>

#include <util/generic/hash.h>
#include <util/stream/file.h>

using namespace NOracle;

class TSimulator::TImpl {
    TVector<TCacheCluster> Clusters_;
    THashMap<TString, NCore::TStatistic> Statistics_;

public:
    TImpl(const NProto::TConfig& cfg);

    void Process(const NCore::TRequest& req);

    void OnFinish();

    ui32 WeightSum(uint64_t ts) const;

    THashMap<TString, NCore::TStatistic> GetStatistics() const noexcept;
};

ui32 TSimulator::TImpl::WeightSum(uint64_t ts) const {
    ui32 res = 0;
    for (const auto& cluster : Clusters_) {
        if (cluster.IsOpen(ts)) {
            res += cluster.Weight();
        }
    }
    return res;
}

TSimulator::TImpl::TImpl(const NProto::TConfig& config) {
    for (const auto& clusterCfg : config.GetLocationConfigs()) {
        Clusters_.emplace_back(clusterCfg, config.GetAvgMessageSize());
    }
}

void TSimulator::TImpl::Process(const NCore::TRequest& req) {
    ui32 clusterStamp = req.Uid % WeightSum(req.Timestamp.Seconds());
    for (auto& cluster : Clusters_) {
        if (!cluster.IsOpen(req.Timestamp.Seconds())) {
            continue;
        }
        if (cluster.Weight() <= clusterStamp) {
            clusterStamp -= cluster.Weight();
        } else {
            auto& statistic = Statistics_[cluster.Name()];
            for (size_t intShard = 0; intShard < 7; ++intShard) {
                statistic.OnHit(req, cluster.IsCacheHitAndStoreOnMiss(req.Timestamp, CombineHashes(intShard, req.HashKey), statistic));
            }
            return;
        }
    }
}

void TSimulator::TImpl::OnFinish() {
    for (auto& [_, stat] : Statistics_) {
        stat.OnFinish();
    }
}

THashMap<TString, NCore::TStatistic> TSimulator::TImpl::GetStatistics() const noexcept {
    return Statistics_;
}

TSimulator::TSimulator(const NProto::TConfig& config)
    : Pimpl_(MakeHolder<TImpl>(config))
{
}

TSimulator::~TSimulator() {
}

void TSimulator::Process(const NCore::TRequest& req) {
    Pimpl_->Process(req);
}

void TSimulator::OnFinish() {
    Pimpl_->OnFinish();
}

THashMap<TString, NCore::TStatistic> TSimulator::GetStatistics() const noexcept {
    return Pimpl_->GetStatistics();
}
