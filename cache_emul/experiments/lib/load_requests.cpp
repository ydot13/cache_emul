#include "load_requests.h"

#include <cache_emul/experiments/lib/proto/request.pb.h>

#include <cache_emul/oracle_lib/fwd.h>
#include <cache_emul/oracle_lib/generators/file_generator/generator.h>

#include <util/system/fs.h>
#include <util/stream/file.h>

using namespace NOracle::NFileGen;

// File strcutr:
// (8 byte) totalRows
// (8 byte) ts, (8 byte) hash, (8 byte) uid_hash
ui64 NExperiment::LoadRequests(TString requestsFile) {
    ui64 totalRows = 0;
    Y_ENSURE(NFs::Exists(requestsFile), "no such file with requests");
    TFileInput fin(requestsFile);
    fin.Read(&totalRows, sizeof(totalRows));
    Cerr << "already has requests file" << Endl;
    return totalRows;
}
