/*
 * loadstore_benches.cpp
 */

#include <iostream>
#include <cassert>
#include <sstream>
#include <iomanip>

#include "hedley.h"
#include "util.hpp"
#include "benches.hpp"
#include "context.hpp"
#include "timers.hpp"

extern "C" {
bench2_f  store16_any;
bench2_f  store32_any;
bench2_f  store64_any;
bench2_f store128_any; // AVX2 128-bit store
bench2_f store256_any; // AVX2 256-bit store

bench2_f  load16_any;
bench2_f  load32_any;
bench2_f  load64_any;
bench2_f load128_any; // AVX (REX-encoded) 128-bit store
bench2_f load256_any; // AVX (REX-encoded) 256-bit store
}

using namespace std;

/*
 * A specialization of BenchmarkGroup that outputs its results in a 4 x 16 grid for all 64 possible
 * offsets within a 64B cache line.
 */
class LoadStoreGroup : public BenchmarkGroup {
    static constexpr unsigned DEFAULT_ROWS =  4;
    static constexpr unsigned DEFAULT_COLS = 16;

    unsigned rows_, cols_, total_cells_, op_size_;
public:
    LoadStoreGroup(const string& id, const string& name, unsigned op_size, unsigned rows, unsigned cols)
: BenchmarkGroup(id, name), rows_(rows), cols_(cols), total_cells_(rows * cols), op_size_(op_size) {
        assert(rows < 10000 && cols < 10000);
    }

    HEDLEY_NEVER_INLINE std::string make_name(ssize_t misalign);
    HEDLEY_NEVER_INLINE std::string make_id(ssize_t misalign);

    HEDLEY_NEVER_INLINE static shared_ptr<LoadStoreGroup> make_group(const string& id, const string& name, ssize_t op_size);

    template<typename TIMER, bench2_f METHOD>
    static shared_ptr<LoadStoreGroup> make(const string& id, unsigned op_size) {
        shared_ptr<LoadStoreGroup> group = make_group(id, id, op_size);
        using maker = BenchmarkMaker<TIMER>;
        for (ssize_t misalign = 0; misalign < 64; misalign++) {
            group->add(maker::template make_bench<METHOD>(group.get(), group->make_id(misalign), group->make_name(misalign), 128,
                    [misalign]() { return misaligned_ptr(64, 64,  misalign); }, 1000));
        }
        return group;
    }

    virtual void runIf(Context& c, const TimerInfo &ti, const predicate_t& predicate) override {

        // because this BenchmarkGroup is really more like a single benchmark (i.e., the 64 actual Benchmark objects
        // don't their name printed but are show in a grid instead, we run the predicate on a fake Benchmark created
        // based on the group name
        Benchmark fake = std::make_shared<LoopedBenchmark>(this, "fake", getDescription(), 1, full_bench_t(), 1);
        if (!predicate(fake)) {
            return;
        }

        std::ostream& os = c.out();
        os << endl << "** Inverse throughput for " << getDescription() << " **" << endl;

        // column headers
        os << "offset  ";
        for (unsigned col = 0; col < cols_; col++) {
            os << setw(5) << col;
        }
        os << endl;

        auto benches = getBenches();
        assert(benches.size() == rows_ * cols_);

        // collect all the results up front, before any output
        vector<double> results(benches.size());
        for (size_t i = 0; i < benches.size(); i++) {
            Benchmark& b = benches[i];
            results[i] = b->run().getCycles();
        }

        for (unsigned row = 0, i = 0; row < rows_; row++) {
            os << setw(3) << (row * cols_) << " :   ";
            for (unsigned col = 0; col < cols_; col++, i++) {
                os << setprecision(1) << fixed << setw(5) << results[i];
            }
            os << endl;
        }
    }

    void printBenches(std::ostream& out) const override {
        auto& benches = getBenches();
        assert(benches.size() >= 2);
        printBench(out, benches[0]);
        printBench(out, benches[1]);
        out << "...\n";
        printBench(out, benches.back());
    }
};

constexpr unsigned LoadStoreGroup::DEFAULT_ROWS;
constexpr unsigned LoadStoreGroup::DEFAULT_COLS;

shared_ptr<LoadStoreGroup> LoadStoreGroup::make_group(const string& id, const string& name, ssize_t op_size) {
    return make_shared<LoadStoreGroup>(id, name, op_size, DEFAULT_ROWS, DEFAULT_COLS);
}

std::string LoadStoreGroup::make_name(ssize_t misalign) {
    std::stringstream ss;
    ss << "Misaligned " << (op_size_ * 8) << "-bit " << getDescription() << " [" << setw(2) << misalign << "]";
    return ss.str();
}

std::string LoadStoreGroup::make_id(ssize_t misalign) {
    std::stringstream ss;
    ss << "misaligned-" << misalign;
    return ss.str();
}

template <typename TIMER>
void register_loadstore(GroupList& list) {
    // load throughput benches
    list.push_back(LoadStoreGroup::make<TIMER,  load16_any>("load/16-bit",  2));
    list.push_back(LoadStoreGroup::make<TIMER,  load32_any>("load/32-bit",  4));
    list.push_back(LoadStoreGroup::make<TIMER,  load64_any>("load/64-bit",  8));
    list.push_back(LoadStoreGroup::make<TIMER, load128_any>("load/128-bit", 16));
    list.push_back(LoadStoreGroup::make<TIMER, load256_any>("load/256-bit", 32));

    // store throughput
    list.push_back(LoadStoreGroup::make<TIMER,  store16_any>( "store/16-bit",  2));
    list.push_back(LoadStoreGroup::make<TIMER,  store32_any>( "store/32-bit",  4));
    list.push_back(LoadStoreGroup::make<TIMER,  store64_any>( "store/64-bit",  8));
    list.push_back(LoadStoreGroup::make<TIMER, store128_any>("store/128-bit", 16));
    list.push_back(LoadStoreGroup::make<TIMER, store256_any>("store/256-bit", 32));
}

#define REG_LOADSTORE(CLOCK) template void register_loadstore<CLOCK>(GroupList& list);

ALL_TIMERS_X(REG_LOADSTORE)



