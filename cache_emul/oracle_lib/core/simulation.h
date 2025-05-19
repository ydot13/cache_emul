#pragma once

#include "simulator.h"
#include "generator.h"

#include <util/generic/ptr.h>

namespace NOracle::NCore {

class TSimulation {
    ISimulator* Simulator_;
    IGenerator* Generator_;
    std::function<void(ui64)> OnProcess_ = 0;

public:
    TSimulation(ISimulator* sim, IGenerator* gen)
        : Simulator_(sim)
        , Generator_(gen)
    {
    }

    void SetProcessCallback(std::function<void(ui64)> func) {
        OnProcess_ = std::move(func);
    }

    ISimulator* GetSimulator() const noexcept {
        return Simulator_;
    }

    IGenerator* GetGenerator() const noexcept {
        return Generator_;
    }

    void Run() {
        ui64 requests = 0;
        while (TRequest* req = Generator_->Next()) {
            requests += 1;
            Simulator_->Process(*req);
            if (OnProcess_) {
                OnProcess_(requests);
            }
        }
        Simulator_->OnFinish();
    }
};

} // namespace NOracle::NCore
