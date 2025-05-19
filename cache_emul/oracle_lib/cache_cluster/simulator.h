#pragma once

#include <cache_emul/oracle_lib/core/simulator.h>

#include <util/generic/ptr.h>

namespace NOracle {

namespace NProto {
    class TConfig;
}

namespace NCore {
    class TStatistic;
}

class TSimulator : public NCore::ISimulator {
    class TImpl;

public:
    TSimulator(const NProto::TConfig&);
    ~TSimulator();

    void Process(const NCore::TRequest&) override;
    void OnFinish() override;

    THashMap<TString, NCore::TStatistic> GetStatistics() const noexcept;

private:
    THolder<TImpl> Pimpl_;
};

} // namespace NOracle