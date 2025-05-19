#pragma once

#include "request.h"

#include <util/generic/vector.h>
#include <util/generic/maybe.h>
#include <util/datetime/base.h>
#include <util/system/types.h>

namespace NOracle::NCore {

struct TWindowStat {
    TInstant StartWindowTs;
    TInstant EndWindowTs;
    double CacheHit = 0;
    ui64 Requests = 0;
};

class TStatistic {
    ui64 Requests_ = 0;
    ui64 CacheHits_ = 0;

    TVector<TWindowStat> WindowStat_;
    ui64 WindowRequests_ = 0;
    ui64 WindowCacheHits_ = 0;

    ui64 KeyDisplacement_ = 0;
    int WindowsBeforeDisplacement_ = 0;

public:
    void OnDisplacement(ui64 cnt) {
        if (KeyDisplacement_ == 0) {
            WindowsBeforeDisplacement_ = WindowStat_.empty() ? 0 :  WindowStat_.size() - 1;
        }
        KeyDisplacement_ += cnt;
    }
    void OnHit(const NCore::TRequest& req, bool isCacheHit) {
        if (WindowStat_.empty()) {
            auto& stat = WindowStat_.emplace_back();
            stat.StartWindowTs = req.Timestamp;
        }
        auto& stat = WindowStat_.back();

        if ((req.Timestamp - stat.StartWindowTs) > TDuration::Minutes(5)) {
            stat.Requests = WindowRequests_;
            stat.CacheHit = WindowRequests_ == 0 ? 0 :
                static_cast<double>(WindowCacheHits_) / WindowRequests_;
            stat.EndWindowTs = req.Timestamp;
            WindowCacheHits_ = WindowRequests_ = 0;

            auto& newWindow = WindowStat_.emplace_back();
            newWindow.StartWindowTs = req.Timestamp;
        }
        Requests_ += 1;
        CacheHits_ += isCacheHit ? 1 : 0;
        WindowCacheHits_ += isCacheHit ? 1 : 0;
        WindowRequests_ += 1;
    }
    void OnFinish() {
        if (!WindowStat_.empty() && !WindowStat_.back().EndWindowTs) {
            WindowStat_.pop_back();
        }
    }
    double AvgCacheHit() const {
        return static_cast<double>(CacheHits_) / Requests_;
    }
    ui64 Requests() const {
        return Requests_;
    }
    ui64 CacheHits() const {
        return CacheHits_;
    }
    ui64 KeyDisplacement() const {
        return KeyDisplacement_;
    }
    TVector<TWindowStat> WindowStat() const {
        return WindowStat_;
    }
    int WindowsBeforeDisplacement() const {
        return WindowsBeforeDisplacement_;
    }
};

} // namespace NOracle::NCore