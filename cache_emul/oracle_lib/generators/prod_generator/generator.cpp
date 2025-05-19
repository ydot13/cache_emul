#include "generator.h"

#include <cache_emul/oracle_lib/generators/prod_generator/proto/params.pb.h>

#include <util/random/random.h>
#include <util/generic/map.h>

using namespace NOracle::NProdGenerator;

namespace {

double GenerateExponential(double lambda) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::exponential_distribution<> d(lambda);
    return d(gen);
}

double GetZipf(int rank, double a, double b, double c) {
    return c / std::pow(rank + b, a);
}

struct TRareRequest : public NOracle::NCore::TRequest {
    int Type = 0;
    double Rate;
    ui32 RemainingRequests;

    TRareRequest() = default;
    TRareRequest(double rate, ui32 requests)
        : NOracle::NCore::TRequest(RandomNumber<ui64>(), 1, TInstant::Zero())
        , Type(requests)
        , Rate(rate)
        , RemainingRequests(requests - 1)
    {}

    TRareRequest(const TRareRequest& other) = default;
    TRareRequest& operator=(const TRareRequest& other) = default;

    NOracle::NCore::TRequest ToRequest() const noexcept {
        return *this;
    }

    TMaybe<std::pair<double, TRareRequest>> Next() const noexcept {
        if (RemainingRequests == 0) {
            return Nothing();
        }
        TRareRequest next = *this;
        next.RemainingRequests -= 1;
        return std::pair<double, TRareRequest>{GenerateExponential(Rate), next};
    }
};

}

struct TGenerator::TImpl {
    TVector<ui32> newRarePerBucket;
    TVector<double> twoRations;

    NProto::TParams Config;
    TInstant StartTimestamp;
    TMap<ui32, double> UniqRations;
    TVector<double> RareProbs;

    NCore::TRequest LastRequest;
    
    ui32 CurrentBucket = 0;
    double CurrentTimestamp = 0;
    double CurrentBucketTsStep = 0;

    double CurrentRareRequestIdx = 0;
    TVector<NCore::TRequest> BucketRequests;
    TMap<double, TRareRequest> RareRequests;
    TVector<int> RequestsInHour;

    TImpl(const NProto::TParams& config)
        : Config(config)
    {
        Y_ENSURE(Config.GetSingleRequests().size() == Config.GetHighFreqRations().size());
        Y_ENSURE(Config.GetSingleRequests().size() == Config.GetF5PerBucketRations().size());
        Y_ENSURE(Config.GetSingleRequests().size() == Config.GetRpsPerBucket().size());
        StartTimestamp = TInstant::Seconds(Config.GetStartTimestamp());

        for (int i = 0; i < Config.GetRpsPerBucket().size(); i += 12) {
            int reqInHour = 0;
            for (int j = i; j < i + 12 && j < Config.GetRpsPerBucket().size(); j++) {
                reqInHour += Config.GetRpsPerBucket(j);
            }
            RequestsInHour.push_back(reqInHour);
        }

        /*
            Explanation

            We have constant rations of rare requests which destributed Zipf like

            So, we know (p_2 * 2) / (p_k * k) = (ration_2 / ration_k)
            where p_2 - prob that new request is double request.
            We have rations and fact that p2 + ... + pn = 1, we can solve this system and find
            p_i
        */
        int numRareTypes = Config.GetMiddleRequests().GetPuasonRates().size();
        TVector<double> rations;
        for (int i = 1; i <= numRareTypes; ++i) {
            rations.push_back(GetZipf(i, Config.GetMiddleRequests().GetProbZipfA(), Config.GetMiddleRequests().GetProbZipfB(), Config.GetMiddleRequests().GetProbZipfC()));
        }

        double sum = 1;
        for (int i = 1; i < numRareTypes; ++i) {
            double a = (rations[0] * (i + 2)) / (rations[i] * 2);
            sum += 1 / a;
        }
        double prob_2 = 1 / sum;
        RareProbs.push_back(prob_2);
        for (int i = 1; i < numRareTypes; ++i) {
            double a = (rations[0] * (i + 2)) / (rations[i] * 2);
            RareProbs.push_back(prob_2 / a);
        }
    }

    ui32 GetCurrentDayBucket() {
        return CurrentBucket % Config.GetRpsPerBucket().size();
    }
    
    NCore::TRequest* Next() {
        if (CurrentBucket >= Config.GetGenerateBucketsCnt()) {
            return nullptr;
        }
        if (BucketRequests.empty()) {
            GenerateBucket();
            CurrentTimestamp = (StartTimestamp + TDuration::Minutes(5) * CurrentBucket).Seconds();
            CurrentBucketTsStep = (300. / BucketRequests.size());
        }
        LastRequest = BucketRequests.back();
        BucketRequests.pop_back();
        if (BucketRequests.empty()) {
            CurrentBucket += 1;
        }
        LastRequest.Timestamp = TInstant::Seconds(static_cast<ui64>(CurrentTimestamp));
        CurrentTimestamp += CurrentBucketTsStep;
        return &LastRequest;
    }

    TRareRequest GenerateRareRequest(int bucket) {
        double rnd = RandomNumber<double>();
        int i = 1;
        for (; i < Config.GetMiddleRequests().GetPuasonRates().size(); ++i) {
            if (RareProbs[i-1] > rnd) {
                break;
            }
            rnd -= RareProbs[i-1];
        }
        int hourId = bucket / (60 / 5);
        double normalizedRate = Config.GetMiddleRequests().GetPuasonRates(i - 1).GetValue(hourId);
        double rate = normalizedRate / RequestsInHour.at(hourId);
        return TRareRequest(rate, i + 1);
    }

    void GenerateBucket(bool warmup = false) {
        ui32 bucket = GetCurrentDayBucket();

        i32 uniqRequestsCnt = Config.GetSingleRequests(bucket) * Config.GetRpsPerBucket(bucket);;
        i32 highFreqRequestsCnt = highFreqRequestsCnt = Config.GetHighFreqRations(bucket) * Config.GetRpsPerBucket(bucket);
        // unique
        if (!warmup) {
            for (int i = 0; i < uniqRequestsCnt; ++i) {
                BucketRequests.push_back(
                    NCore::TRequest {
                        .HashKey = RandomNumber<ui64>(),
                        .Uid = 1, // TODO: different users
                        .Timestamp = TInstant::Zero()
                    }
                );
            }
        }

        // high freq
        if (!warmup) {
            for (int i = 0; i < highFreqRequestsCnt; ++i) {
                BucketRequests.push_back(
                    NCore::TRequest {
                        .HashKey = 0,
                        .Uid = 1, // TODO: different users
                        .Timestamp = TInstant::Zero()
                    }
                );
            }
        }

        ui32 newRare = 0;
        ui32 twoCnt = 0;
        i32 rareRequestsCnt = 0;
        // rare
        {
            constexpr double newInterval = 1;  
            rareRequestsCnt = std::max(0ul, Config.GetRpsPerBucket(bucket) - uniqRequestsCnt - highFreqRequestsCnt);
            for (int i = 0; i < rareRequestsCnt; ++i) {
                // generate new
                if (RareRequests.empty() || (RareRequests.begin()->first >= CurrentRareRequestIdx + newInterval)) {
                    TRareRequest newRequest = GenerateRareRequest(bucket);
                    if (newRequest.Type == 2) {
                        twoCnt += 1;
                    }
                    newRare += 1;
                    if (!warmup) {
                        BucketRequests.push_back(newRequest.ToRequest());
                    }
                    if (auto next = newRequest.Next(); next) {
                        RareRequests[CurrentRareRequestIdx + next->first] = next->second;
                    }
                    CurrentRareRequestIdx += newInterval;
                } else { // take from map
                    TRareRequest req = std::move(RareRequests.begin()->second);
                    double idx = RareRequests.begin()->first;
                    RareRequests.erase(RareRequests.begin());
                    if (req.Type == 2) {
                        twoCnt += 1;
                    }
                    if (!warmup) {
                        BucketRequests.push_back(req.ToRequest());
                    }
                    if (auto next = req.Next(); next) {
                        RareRequests[idx + next->first] = next->second;
                    }

                    if (RareRequests.empty() || (RareRequests.begin()->first >= CurrentRareRequestIdx + newInterval)) {
                        CurrentRareRequestIdx += newInterval;
                    }
                }
            }
        }
        twoRations.push_back(twoCnt * 1.0 / Config.GetRpsPerBucket(bucket));
        newRarePerBucket.push_back(newRare);

        // process f5
        if (!warmup) {
            TVector<NCore::TRequest> tmp;
            std::swap(tmp, BucketRequests);
            for (auto& r : tmp) {
                if (RandomNumber<double>() <= Config.GetF5PerBucketRations(bucket)) {
                    BucketRequests.push_back(r);
                }
                BucketRequests.push_back(std::move(r));
            }
            std::random_shuffle(BucketRequests.begin(), BucketRequests.end());
        }
    }

};

NOracle::NProdGenerator::TGenerator::TGenerator(const NProto::TParams& config)
    : Pimpl_(MakeHolder<TImpl>(config))
{}

NOracle::NCore::TRequest* NOracle::NProdGenerator::TGenerator::Next() {
    return Pimpl_->Next();
}

NOracle::NProdGenerator::TGenerator::~TGenerator() = default;
