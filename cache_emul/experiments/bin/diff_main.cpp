#include <cache_emul/experiments/lib/load_requests.h>
#include <cache_emul/experiments/lib/proto/config.pb.h>
#include <cache_emul/oracle_lib/fwd.h>
#include <cache_emul/experiments/lib/report.h>
#include <cache_emul/oracle_lib/generators/file_generator/generator.h>

#include <library/cpp/protobuf/util/pb_io.h>
#include <library/cpp/getopt/last_getopt.h>

#include <util/stream/file.h>
#include <util/system/fs.h>

int diff_main(int argc, const char** argv) {
    TString configPath;
    {
        NLastGetopt::TOpts opts;
        opts
            .AddLongOption('p', "path", "Path to config")
            .RequiredArgument()
            .StoreResult(&configPath);
        NLastGetopt::TOptsParseResult {&opts, argc, argv};
    }
    NExperiment::NProto::TDiffConfig cfg(ParseFromTextFormat<NExperiment::NProto::TDiffConfig>(configPath));

    NYT::TNode finalReport;

    if (!NFs::Exists(cfg.GetReportDir())) {
        NFs::MakeDirectoryRecursive(cfg.GetReportDir());
    }

    for (const auto& simulationConfig : cfg.GetSimulationConfigs()) {
        for (const auto& generatorConfig : cfg.GetGeneratorConfigs()) {
            THolder<NOracle::NCore::IGenerator> generator;
            ui64 totalRequests = 0;
            if (generatorConfig.HasFileGeneratorConfig()) {
                const auto& config = generatorConfig.GetFileGeneratorConfig();
                totalRequests = NExperiment::LoadRequests(config.GetAmmoFilePath());
                generator = MakeHolder<NOracle::NFileGen::TGenerator>(config.GetAmmoFilePath());
            } else if (generatorConfig.HasProdGeneratorConfig()) {
                const auto& config = generatorConfig.GetProdGeneratorConfig();
                for (size_t i = 0; i < config.GetGenerateBucketsCnt(); ++i) {
                    totalRequests += config.GetRpsPerBucket(i % config.GetRpsPerBucket().size());
                }
                generator = MakeHolder<NOracle::NProdGenerator::TGenerator>(config);
            }
            Y_ENSURE(generator, "nullptr generator");
            Cerr << "sim: " << simulationConfig.GetComment() << "; gen: " << generatorConfig.GetComment() << Endl;
            NOracle::TSimulator simulator(simulationConfig);
            NOracle::NCore::TSimulation simulation(&simulator, &*generator);
            NOracle::NCLI::TProgressBar pb(totalRequests);
            simulation.SetProcessCallback([&pb] (ui64 val) {pb.Update(val);});
            simulation.Run();
            auto stats = simulator.GetStatistics();

            NYT::TNode report;
            for (const auto& [cluster, stat] : stats) {
                report[cluster]["avg_cache_hit"] = stat.AvgCacheHit();
                double max_cache_hit = -1;
                for (const auto& bucketStat : stat.WindowStat()) {
                    max_cache_hit = std::max(max_cache_hit, bucketStat.CacheHit);
                }
                report[cluster]["max_cache_hit"] = max_cache_hit;
                report[cluster]["min_without_displacement"] = stat.WindowsBeforeDisplacement() * 5;
            }
            finalReport[simulationConfig.GetComment()][generatorConfig.GetComment()] = std::move(report);
            
            {
                TFileOutput fout(TString::Join(cfg.GetReportDir(), "/", simulationConfig.GetComment(), "_", generatorConfig.GetComment(), ".json"));
                fout << NExperiment::ToJsonString(NExperiment::BuildReport(stats));
                fout.Finish();
            }
        }

    }

    {
        TFileOutput fout(TString::Join(cfg.GetReportDir(), "/final_grid_report.json"));
        fout << NExperiment::ToJsonString(finalReport);
        fout.Finish();
    }
    Cerr << "See " << cfg.GetReportDir() << " for details" << Endl;

    return 0;
}