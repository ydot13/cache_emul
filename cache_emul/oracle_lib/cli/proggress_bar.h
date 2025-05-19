#pragma once

#include <util/stream/output.h>
#include <util/system/types.h>

namespace NOracle::NCLI {

class TProgressBar {
    ui64 MaxProgress_ = 0;
    ui64 CurrentProgress_ = -1;

    IOutputStream& Stream_;

public:
    TProgressBar(ui64 maxProgress, IOutputStream& stream = Cerr);

    void Update(ui64 progress);
};

} // namespace NOracle::NCLI
