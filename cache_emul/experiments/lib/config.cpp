#include "config.h"

using namespace NExperiment;

TString TConfig::RequestsFile() const {
    return TString::Join(GetRequestsDir(), "/requests_", GetDay(), ".proto");
}
TString TConfig::ReportFile() const {
    return TString::Join(GetReportDir(), "/report_", GetDay(), GetReportSuffix(), ".json");
}
