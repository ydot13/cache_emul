#pragma once

#include <cache_emul/experiments/lib/proto/config.pb.h>

namespace NExperiment {

class TConfig : public NProto::TConfig {
public:
    TConfig() = default;
    TConfig(const NProto::TConfig& cfg) : NExperiment::NProto::TConfig(cfg) {}
    TConfig(NProto::TConfig&& cfg) : NExperiment::NProto::TConfig(std::move(cfg)) {}

    TString RequestsFile() const;
    TString ReportFile() const;
};

} // namespace NExperiment
