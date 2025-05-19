#pragma once

#include "request.h"

namespace NOracle::NCore {

class IGenerator {
public:
    virtual ~IGenerator() {}
    virtual TRequest* Next() = 0;
};

} // namespace NOracle::NCore
