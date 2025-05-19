#pragma once

#include <cache_emul/oracle_lib/core/generator.h>

namespace NOracle::NFileGen {

class TGenerator : public NOracle::NCore::IGenerator {
    class TImpl;

public:
    TGenerator(TString filePath);
    ~TGenerator();

    NCore::TRequest* Next() override;

private:
    THolder<TImpl> Pimpl_;
};

} // namespace NOracle::NFileGen
