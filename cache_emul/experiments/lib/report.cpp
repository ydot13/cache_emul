#include "report.h"

#include <library/cpp/yson/node/node_visitor.h>
#include <library/cpp/yson/json/json_writer.h>

TString NExperiment::ToJsonString(const NYT::TNode& n, bool prettyAndSort) {
    TStringStream stream;
    {
        NYT::TJsonWriter writer(&stream, NYson::EYsonType::Node,
            prettyAndSort ? NYT::EJsonFormat::JF_PRETTY : NYT::EJsonFormat::JF_TEXT,
            NYT::EJsonAttributesMode::JAM_ON_DEMAND,
            NYT::ESerializedBoolFormat::SBF_BOOLEAN
        );
        NYT::TNodeVisitor visitor(&writer, /* sortKeys= */ prettyAndSort);
        visitor.Visit(n);
    }
    return stream.Str();
}

NYT::TNode NExperiment::BuildReport(const THashMap<TString, NOracle::NCore::TStatistic>& stats) {
    NYT::TNode report;
    for (const auto& [cluster, stat] : stats) {
        report[cluster]["avg"] = stat.AvgCacheHit();
        report[cluster]["requests"] = stat.Requests();
        report[cluster]["displacements"] = stat.KeyDisplacement();
        report[cluster]["window_stat"] = NYT::TNode::CreateList();
        for (const auto& windStat : stat.WindowStat()) {
            auto& elem = report[cluster]["window_stat"].AsList().emplace_back();
            elem["cache_hit"] = windStat.CacheHit;
            elem["timestamp"] = windStat.StartWindowTs.Seconds();
            elem["requests"] = windStat.Requests;
        }
    }
    return report;
}