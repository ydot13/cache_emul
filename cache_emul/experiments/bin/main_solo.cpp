#include <cache_emul/experiments/lib/config.h>
#include <cache_emul/experiments/lib/load_requests.h>
#include <cache_emul/experiments/lib/report.h>
#include <cache_emul/oracle_lib/fwd.h>

#include <library/cpp/protobuf/util/pb_io.h>
#include <library/cpp/getopt/last_getopt.h>

#include <util/system/thread.h>
#include <util/stream/file.h>

using namespace NExperiment;

int main_solo(int argc, const char** argv) {
    TString configPath;
    {
        NLastGetopt::TOpts opts;
        opts
            .AddLongOption('p', "path", "Path to config")
            .RequiredArgument()
            .StoreResult(&configPath);
        NLastGetopt::TOptsParseResult {&opts, argc, argv};
    }

    TConfig cfg(ParseFromTextFormat<NExperiment::NProto::TConfig>(configPath));
    ui64 TotalRequests = NExperiment::LoadRequests(cfg.RequestsFile());

    NOracle::NFileGen::TGenerator generator(cfg.RequestsFile());
    NOracle::TSimulator simulator(cfg.GetSimulationConfig());

    NOracle::NCore::TSimulation simulation(&simulator, &generator);
    NOracle::NCLI::TProgressBar pb(TotalRequests);
    simulation.SetProcessCallback([&pb] (ui64 val) {pb.Update(val);});
    simulation.Run();

    auto stats = simulator.GetStatistics();
    NYT::TNode report = BuildReport(stats);

    {
        TFileOutput fout(cfg.ReportFile());
        fout << NExperiment::ToJsonString(report);
        fout.Finish();
    }

    Cerr << "See " << cfg.ReportFile() << " for details" << Endl;

    return 0;
}
