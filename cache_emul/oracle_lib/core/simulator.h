#pragma once

#include "request.h"

namespace NOracle::NCore {

class ISimulator {
public:
    virtual void Process(const TRequest&) = 0;
    virtual void OnFinish() {};
};

} // namespace NOracle::NCore
