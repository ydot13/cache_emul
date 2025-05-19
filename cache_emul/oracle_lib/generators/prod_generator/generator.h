#pragma once

#include <cache_emul/oracle_lib/core/generator.h>

namespace NOracle::NProdGenerator {

namespace NProto {
    class TParams;
}

class TGenerator : public NOracle::NCore::IGenerator {
    struct TImpl;

public:
    TGenerator(const NProto::TParams& config);
    ~TGenerator();

    NCore::TRequest* Next() override;

private:
    THolder<TImpl> Pimpl_;
};

} // namespace NOracle::NProdGenerator
