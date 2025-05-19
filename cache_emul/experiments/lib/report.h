#pragma once

#include <cache_emul/oracle_lib/core/statistic.h>

namespace NExperiment {

TString ToJsonString(const NYT::TNode& n, bool prettyAndSort = true);

NYT::TNode BuildReport(const THashMap<TString, NOracle::NCore::TStatistic>& stats);

}