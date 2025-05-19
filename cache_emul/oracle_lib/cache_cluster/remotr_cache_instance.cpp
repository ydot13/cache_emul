#include "remote_cache_instance.h"

#include <util/generic/vector.h>
#include <util/generic/deque.h>
#include <util/generic/xrange.h>
#include <util/generic/list.h>
#include <util/generic/hash_set.h>
#include <util/random/random.h>

using namespace NOracle;

class ICacher {
public:
    virtual ~ICacher() {}
    virtual size_t Size() const = 0;
    virtual bool LookupAndStore(ui64 hashKey, NCore::TStatistic& stat) = 0;
};

class TProdCacher : public ICacher {
protected:
    static const ui64 EstimatedRecordSize_ = (ui64)32 * 1024;
    ui64 KeysInChunk_ = 0;
    ui64 TotalKeysCapacity_ = 0;
    THashSet<ui64> Keys_;
    TDeque<ui64> Queue_;
public:
    TProdCacher(ui64 arenas, ui64 maxSize, ui64 avgMsgSize, ui64 expectedRequests) {
        ui64 arenaMaxSize = maxSize / arenas;
        const ui64 numRecords = arenaMaxSize / EstimatedRecordSize_ + 1;
        const ui64 numChunks = (ui64)log((double)numRecords) + 1;
        const ui64 chunkSize = arenaMaxSize / numChunks;
        KeysInChunk_ = chunkSize / avgMsgSize;
        TotalKeysCapacity_ = maxSize / avgMsgSize;
        Y_ENSURE(TotalKeysCapacity_ >= KeysInChunk_);

        Keys_.reserve(expectedRequests);
    }

    size_t Size() const override {
        return Queue_.size();
    }

    bool LookupAndStore(ui64 hashKey, NCore::TStatistic& stat) override {
        auto it = Keys_.find(hashKey);
        if (it != Keys_.end()) {
            return true;
        }

        Keys_.insert(it, hashKey);
        Queue_.push_back(hashKey);
        if (Y_UNLIKELY(Queue_.size() > TotalKeysCapacity_)) {
            for (size_t i = 0; i < KeysInChunk_; i += 1) {
                Keys_.erase(Queue_.front());
                Queue_.pop_front();   
            }
            stat.OnDisplacement(KeysInChunk_);
        }
        return false;
    }
};

class TSecondChanceCacher : public ICacher {
    protected:
        static const ui64 EstimatedRecordSize_ = (ui64)32 * 1024;
        ui64 KeysInChunk_ = 0;
        ui64 TotalKeysCapacity_ = 0;
        THashMap<ui64, bool> Keys_;
        TDeque<ui64> Queue_;
    public:
    TSecondChanceCacher(ui64 arenas, ui64 maxSize, ui64 avgMsgSize, ui64 expectedRequests) {
            ui64 arenaMaxSize = maxSize / arenas;
            const ui64 numRecords = arenaMaxSize / EstimatedRecordSize_ + 1;
            const ui64 numChunks = (ui64)log((double)numRecords) + 1;
            const ui64 chunkSize = arenaMaxSize / numChunks;
            KeysInChunk_ = chunkSize / avgMsgSize;
            TotalKeysCapacity_ = maxSize / avgMsgSize;
            Y_ENSURE(TotalKeysCapacity_ >= KeysInChunk_);
    
            Keys_.reserve(expectedRequests);
        }
    
        size_t Size() const override {
            return Queue_.size();
        }
    
        bool LookupAndStore(ui64 hashKey, NCore::TStatistic& stat) override {
            auto it = Keys_.find(hashKey);
            if (it != Keys_.end()) {
                it->second = true; // set second chance bit;
                return true;
            }
    
            Keys_[hashKey] = false;
            Queue_.push_back(hashKey);
            if (Y_UNLIKELY(Queue_.size() > TotalKeysCapacity_)) {
                while (Queue_.size() > TotalKeysCapacity_) {
                    ui64 key = Queue_.front();
                    const auto ptr = Keys_.FindPtr(key);
                    if (!*ptr) {
                        Keys_.erase(key);
                        Queue_.pop_front();
                        stat.OnDisplacement(1);
                        continue;    
                    }
                    *ptr = false;
                    Queue_.pop_front();
                    Queue_.push_back(key);
                }
            }
            return false;
        }
    };

class TSecondWriteCacher : public TProdCacher {
    THashSet<ui64> SeenKeys_;
public:
    TSecondWriteCacher(ui64 arenas, ui64 maxSize, ui64 avgMsgSize, ui64 expectedRequests)
        : TProdCacher(arenas, maxSize, avgMsgSize, expectedRequests)
    {}

    bool LookupAndStore(ui64 hashKey, NCore::TStatistic& stat) override {
        if (!SeenKeys_.contains(hashKey)) {
            SeenKeys_.insert(hashKey);
            return false;
        }
        auto it = Keys_.find(hashKey);
        if (it != Keys_.end()) {
            return true;
        }

        Keys_.insert(it, hashKey);
        Queue_.push_back(hashKey);
        if (Y_UNLIKELY(Queue_.size() > TotalKeysCapacity_)) {
            for (size_t i = 0; i < KeysInChunk_; i += 1) {
                SeenKeys_.erase(Queue_.front());
                Keys_.erase(Queue_.front());
                Queue_.pop_front();   
            }
            stat.OnDisplacement(KeysInChunk_);
        }
        return false;
    }
};

class TFIFOCache {
    THashMap<ui64, TList<ui64>::iterator> Keys_;
    TList<ui64> Queue_;
    size_t MaxKeys_;
public:
    TFIFOCache(size_t maxKeys) : MaxKeys_(maxKeys) {
        Keys_.reserve(maxKeys);
    }

    bool Has(ui64 key) const {
        return Keys_.contains(key);
    }
    void Insert(ui64 key) {
        const auto ptr = Keys_.FindPtr(key);
        if (ptr) {
            return;
        }
        CleanUp();
        Queue_.push_front(key);
        Keys_[key] = Queue_.begin();
    }
    void Remove(ui64 key) {
        const auto ptr = Keys_.find(key);
        if (ptr == Keys_.end()) {
            return;
        }
        Queue_.erase(ptr->second);
        Keys_.erase(ptr);
    }
private:
    void CleanUp() {
        while(Keys_.size() >= MaxKeys_) {
            ui64 key = Queue_.back();
            Remove(key);
        }
    }
};

// Second write cache with small (X%) FIFO cache for first requests 
class TSecondWriteCacherWithSDC : public TProdCacher {
    THashSet<ui64> SeenKeys_;
    TFIFOCache SmallCache_;
public:
    TSecondWriteCacherWithSDC(double perc, ui64 arenas, ui64 maxSize, ui64 avgMsgSize, ui64 expectedRequests)
        : TProdCacher(arenas, static_cast<ui64>(maxSize * (1 - perc)), avgMsgSize, expectedRequests)
        , SmallCache_(static_cast<ui64>(maxSize * perc) / avgMsgSize)
    {
        SeenKeys_.reserve(maxSize / avgMsgSize);
    }

    bool LookupAndStore(ui64 hashKey, NCore::TStatistic& stat) override {
        if (!SeenKeys_.contains(hashKey)) {
            SeenKeys_.insert(hashKey);
            SmallCache_.Insert(hashKey);
            return false;
        }
        auto it = Keys_.find(hashKey);
        if (it != Keys_.end()) {
            return true;
        }

        Keys_.insert(it, hashKey);
        Queue_.push_back(hashKey);
        if (Y_UNLIKELY(Queue_.size() > TotalKeysCapacity_)) {
            for (size_t i = 0; i < KeysInChunk_; i += 1) {
                SeenKeys_.erase(Queue_.front());
                Keys_.erase(Queue_.front());
                Queue_.pop_front();   
            }
            stat.OnDisplacement(KeysInChunk_);
        }
        bool wasInSmall = SmallCache_.Has(hashKey);
        if (wasInSmall) {
            SmallCache_.Remove(hashKey);
        }
        return wasInSmall;
    }
};

class TLRUCacher : public ICacher {
    THashMap<ui64, TList<ui64>::iterator> Keys_;
    TList<ui64> Queue_;
    size_t MaxKeys_;
public:
    TLRUCacher(ui64 _, ui64 maxSize, ui64 avgMsgSize, ui64 expectedRequests)
        : MaxKeys_(maxSize / avgMsgSize)
    {
        Keys_.reserve(expectedRequests);
    }

    size_t Size() const override {
        return Keys_.size();
    }

    bool LookupAndStore(ui64 hashKey, NCore::TStatistic& stat) override {
        auto ptr = Keys_.FindPtr(hashKey);
        if (ptr) {
            Queue_.splice(Queue_.begin(), Queue_, *ptr);
            return true;
        }
        if (Size() < MaxKeys_) {
            Queue_.push_front(hashKey);
            Keys_[hashKey] = Queue_.begin();
            return false;
        }
        while (Size() >= MaxKeys_) {
            auto key = Queue_.back();
            Queue_.pop_back();
            Keys_.erase(key);
            stat.OnDisplacement(1);
        }
        Queue_.push_front(hashKey);
        Keys_[hashKey] = Queue_.begin();
        return false;
    }
};

class TRemoteCacheInstance::TImpl {
    THolder<ICacher> Cacher_;
public:
    TImpl(NProto::ECacheAlgo cacheAlgo, ui64 arenas, ui64 maxSize, ui64 avgMsgSize, ui64 expectedRequests)
    {
        switch(cacheAlgo) {
            case NProto::ECacheAlgo::PROD: {
                Cacher_ = MakeHolder<TProdCacher>(arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_WRITE: {
                Cacher_ = MakeHolder<TSecondWriteCacher>(arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_WRITE_WITH_SDC: {
                Cacher_ = MakeHolder<TSecondWriteCacherWithSDC>(0.1, arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_WRITE_WITH_SDC_15: {
                Cacher_ = MakeHolder<TSecondWriteCacherWithSDC>(0.15, arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_WRITE_WITH_SDC_20: {
                Cacher_ = MakeHolder<TSecondWriteCacherWithSDC>(0.2, arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_WRITE_WITH_SDC_25: {
                Cacher_ = MakeHolder<TSecondWriteCacherWithSDC>(0.25, arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_WRITE_WITH_SDC_30: {
                Cacher_ = MakeHolder<TSecondWriteCacherWithSDC>(0.30, arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_WRITE_WITH_SDC_35: {
                Cacher_ = MakeHolder<TSecondWriteCacherWithSDC>(0.45, arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_WRITE_WITH_SDC_40: {
                Cacher_ = MakeHolder<TSecondWriteCacherWithSDC>(0.40, arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_WRITE_WITH_SDC_45: {
                Cacher_ = MakeHolder<TSecondWriteCacherWithSDC>(0.45, arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::SECOND_CHANCE: {
                Cacher_ = MakeHolder<TSecondChanceCacher>(arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
            case NProto::ECacheAlgo::LRU: {
                Cacher_ = MakeHolder<TLRUCacher>(arenas, maxSize, avgMsgSize, expectedRequests);
                break;
            }
        }
    }

    TVector<size_t> Size() const {
        return {Cacher_->Size()};
    }

    bool LookupAndStore(ui64 hashKey, NCore::TStatistic& stat) {
        // Error write rate 0.08
        if (RandomNumber<ui32>(1000) < 80) {
            return false;
        }
        return Cacher_->LookupAndStore(hashKey, stat);
    }
};


TVector<size_t> TRemoteCacheInstance::Size() const {
    return Pimpl_->Size();
}

bool TRemoteCacheInstance::IsCacheHitAndStoreOnMiss(ui64 hashKey, NCore::TStatistic& stat) {
    return Pimpl_->LookupAndStore(hashKey, stat);
}

TRemoteCacheInstance::TRemoteCacheInstance(NProto::ECacheAlgo cacheAlgo, ui64 numArenas, ui64 maxSize, ui64 avgMsgSize, ui64 expectedRequests)
    : Pimpl_(MakeHolder<TImpl>(cacheAlgo, numArenas, maxSize, avgMsgSize, expectedRequests))
{
}

TRemoteCacheInstance::~TRemoteCacheInstance() {
}

TRemoteCacheInstance::TRemoteCacheInstance(TRemoteCacheInstance&&) = default;
