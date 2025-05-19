#include "proggress_bar.h"

#include <util/stream/format.h>
#include <util/stream/str.h>
#include <util/system/rusage.h>
#include <util/generic/size_literals.h>

using namespace NOracle;

NCLI::TProgressBar::TProgressBar(ui64 maxProgress, IOutputStream& stream)
    : MaxProgress_(maxProgress)
    , Stream_(stream)
{
}

void NCLI::TProgressBar::Update(ui64 progress) {
    ui32 previousPercent = CurrentProgress_ * 100 / MaxProgress_;
    ui32 actualPercent = progress * 100 / MaxProgress_;
    if (previousPercent != actualPercent) {
        TStringStream bar("[");
        for (size_t i = 0; i < 100; i += 1) {
            if (i < actualPercent) {
                bar << '=';
            } else {
                bar << '-';
            }
        }
        bar << "] " << LeftPad(actualPercent, 3, ' ') << "% RSS: " << TRusage::GetCurrentRSS() / 1_MB << "MB\r";
        Stream_ << bar.Str();
    }
    CurrentProgress_ = progress;
    if (progress == MaxProgress_) {
        Stream_ << Endl;
    }
}