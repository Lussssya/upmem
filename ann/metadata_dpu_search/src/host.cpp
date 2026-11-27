#include "metadata.hpp"
#include "search_common.h"
#include "vecs_io.hpp"

extern "C" {
#include <dpu.h>
}

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef DPU_BINARY
#define DPU_BINARY "build/dpu_search"
#endif

namespace {

constexpr const char* simulator_profile = "backend=simulator";
constexpr float distance_absolute_tolerance = 1.0e-3F;
constexpr float distance_relative_tolerance = 1.0e-6F;

static_assert(sizeof(search_config_t) == 8U);
static_assert(sizeof(search_output_t) == 48U);

struct Options {
    std::filesystem::path dataset_dir{"../datasets/siftsmall"};
    std::string minimum_date{"2027-05-01"};
    std::size_t query_id{0};
    std::uint32_t k{MAX_K};
};

struct Neighbor {
    std::uint32_t id{};
    float distance{};
};

struct DpuRun {
    search_output_t output{};
    std::size_t host_to_dpu_bytes{};
    std::size_t dpu_to_host_bytes{};
};

class DpuSet {
public:
    DpuSet() = default;
    DpuSet(const DpuSet&) = delete;
    DpuSet& operator=(const DpuSet&) = delete;

    ~DpuSet() {
        if (allocated_) {
            dpu_free(set_);
        }
    }

    void allocate() {
        check(dpu_alloc(1U, simulator_profile, &set_), "allocate one simulated DPU");
        allocated_ = true;
    }

    void release() {
        if (allocated_) {
            const dpu_error_t error = dpu_free(set_);
            allocated_ = false;
            check(error, "free DPU set");
        }
    }

    [[nodiscard]] struct dpu_set_t get() const {
        return set_;
    }

    static void check(dpu_error_t error, const std::string& operation) {
        if (error != DPU_OK) {
            throw std::runtime_error(operation + ": " + dpu_error_to_string(error));
        }
    }

private:
    struct dpu_set_t set_{};
    bool allocated_{false};
};

std::size_t parse_nonnegative(const std::string& value, const std::string& option) {
    if (value.empty() || value.front() == '-') {
        throw std::runtime_error(option + " requires a nonnegative integer");
    }
    std::size_t consumed = 0;
    unsigned long long parsed = 0;
    try {
        parsed = std::stoull(value, &consumed, 10);
    } catch (const std::exception&) {
        throw std::runtime_error(option + " requires a nonnegative integer");
    }
    if (consumed != value.size() || parsed > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(option + " requires a nonnegative integer");
    }
    return static_cast<std::size_t>(parsed);
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program
              << " [--dataset-dir PATH] [--min-date YYYY-MM-DD]"
                 " [--query-id N] [--k 1..5]\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(EXIT_SUCCESS);
        }
        if (index + 1 >= argc) {
            throw std::runtime_error("missing value for option: " + argument);
        }
        const std::string value = argv[++index];
        if (argument == "--dataset-dir") {
            options.dataset_dir = value;
        } else if (argument == "--min-date") {
            options.minimum_date = value;
        } else if (argument == "--query-id") {
            options.query_id = parse_nonnegative(value, argument);
        } else if (argument == "--k") {
            const auto parsed = parse_nonnegative(value, argument);
            if (parsed == 0 || parsed > MAX_K) {
                throw std::runtime_error("--k must be between 1 and 5");
            }
            options.k = static_cast<std::uint32_t>(parsed);
        } else {
            throw std::runtime_error("unknown option: " + argument);
        }
    }
    return options;
}

float squared_l2(const float* query, const float* candidate, std::size_t dimensions) {
    float distance = 0.0F;
    for (std::size_t dimension = 0; dimension < dimensions; ++dimension) {
        const float difference = query[dimension] - candidate[dimension];
        distance += difference * difference;
    }
    return distance;
}

std::vector<Neighbor> cpu_exact_search(
    const float* query,
    const FilteredCandidates& candidates,
    std::uint32_t k) {
    std::vector<Neighbor> neighbors;
    neighbors.reserve(candidates.size());
    for (std::size_t candidate = 0; candidate < candidates.size(); ++candidate) {
        neighbors.push_back(
            {static_cast<std::uint32_t>(candidates.original_ids[candidate]),
             squared_l2(query,
                        candidates.vectors.data() + candidate * candidates.dimensions,
                        candidates.dimensions)});
    }
    std::sort(neighbors.begin(), neighbors.end(), [](const Neighbor& left,
                                                     const Neighbor& right) {
        return left.distance < right.distance ||
               (left.distance == right.distance && left.id < right.id);
    });
    if (neighbors.size() > k) {
        neighbors.resize(k);
    }
    return neighbors;
}

void copy_to_dpu(struct dpu_set_t set,
                 const char* symbol,
                 const void* source,
                 std::size_t bytes) {
    struct dpu_set_t dpu;
    DPU_FOREACH(set, dpu) {
        DpuSet::check(dpu_copy_to(dpu, symbol, 0, source, bytes),
                      std::string("copy to ") + symbol);
    }
}

DpuRun run_dpu(const float* query,
               const FilteredCandidates& candidates,
               std::uint32_t k) {
    if (candidates.size() > MAX_CANDIDATES ||
        candidates.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("filtered candidate count exceeds DPU capacity");
    }

    DpuSet dpu_set;
    dpu_set.allocate();
    auto set = dpu_set.get();

    std::uint32_t allocated_dpus = 0;
    DpuSet::check(dpu_get_nr_dpus(set, &allocated_dpus), "query DPU count");
    if (allocated_dpus != 1U) {
        throw std::runtime_error("SDK did not allocate exactly one DPU");
    }
    DpuSet::check(dpu_load(set, DPU_BINARY, nullptr), "load DPU binary");

    const search_config_t config{
        static_cast<std::uint32_t>(candidates.size()), k};
    copy_to_dpu(set, "search_config", &config, sizeof(config));
    copy_to_dpu(set, "mram_query", query, VECTOR_DIMENSIONS * sizeof(float));

    const std::size_t candidate_bytes = candidates.vectors.size() * sizeof(float);
    const std::size_t id_bytes = candidates.original_ids.size() * sizeof(std::uint64_t);
    if (!candidates.original_ids.empty()) {
        copy_to_dpu(set, "mram_candidates", candidates.vectors.data(), candidate_bytes);
        copy_to_dpu(set, "mram_candidate_ids", candidates.original_ids.data(), id_bytes);
    }

    DpuSet::check(dpu_launch(set, DPU_SYNCHRONOUS), "launch DPU kernel");

    search_output_t output{};
    struct dpu_set_t dpu;
    DPU_FOREACH(set, dpu) {
        DpuSet::check(dpu_copy_from(dpu, "search_output", 0, &output, sizeof(output)),
                      "copy search output from DPU");
    }
    dpu_set.release();

    return {output,
            sizeof(config) + VECTOR_DIMENSIONS * sizeof(float) + candidate_bytes +
                id_bytes,
            sizeof(output)};
}

bool distances_close(float dpu_distance, float cpu_distance) {
    const float tolerance = distance_absolute_tolerance +
                            distance_relative_tolerance * std::abs(cpu_distance);
    return std::abs(dpu_distance - cpu_distance) <= tolerance;
}

bool compare_results(const search_output_t& dpu,
                     const std::vector<Neighbor>& cpu,
                     std::string& error) {
    if (dpu.status != SEARCH_STATUS_OK) {
        error = "DPU reported status " + std::to_string(dpu.status);
        return false;
    }
    if (dpu.result_count != cpu.size()) {
        error = "result-count mismatch";
        return false;
    }
    for (std::size_t rank = 0; rank < cpu.size(); ++rank) {
        if (dpu.ids[rank] != cpu[rank].id) {
            error = "neighbor ID/ranking mismatch at rank " + std::to_string(rank);
            return false;
        }
        if (!distances_close(dpu.distances[rank], cpu[rank].distance)) {
            error = "distance mismatch at rank " + std::to_string(rank);
            return false;
        }
    }
    return true;
}

void print_neighbors(const char* label,
                     const search_output_t& output) {
    std::cout << label << '\n';
    if (output.result_count == 0U) {
        std::cout << "  (none)\n";
        return;
    }
    for (std::uint32_t rank = 0; rank < output.result_count; ++rank) {
        std::cout << "  rank " << rank + 1U << ": id=" << output.ids[rank]
                  << ", squared_l2=" << output.distances[rank] << '\n';
    }
}

void print_neighbors(const char* label,
                     const std::vector<Neighbor>& neighbors) {
    std::cout << label << '\n';
    if (neighbors.empty()) {
        std::cout << "  (none)\n";
        return;
    }
    for (std::size_t rank = 0; rank < neighbors.size(); ++rank) {
        std::cout << "  rank " << rank + 1U << ": id=" << neighbors[rank].id
                  << ", squared_l2=" << neighbors[rank].distance << '\n';
    }
}

int run(const Options& options) {
    const auto base =
        texmex::read_fvecs(options.dataset_dir / "siftsmall_base.fvecs");
    const auto queries =
        texmex::read_fvecs(options.dataset_dir / "siftsmall_query.fvecs");
    if (base.dimensions != VECTOR_DIMENSIONS ||
        queries.dimensions != VECTOR_DIMENSIONS) {
        throw std::runtime_error("this experiment requires 128-dimensional SIFT vectors");
    }
    if (base.rows > MAX_CANDIDATES) {
        throw std::runtime_error("base-vector count exceeds the one-DPU capacity");
    }
    if (options.query_id >= queries.rows) {
        throw std::runtime_error("query ID is out of range");
    }

    const auto metadata = generate_upload_metadata(base.rows);
    const auto minimum_day = parse_date(options.minimum_date);
    const auto candidates =
        filter_by_minimum_date(base, metadata, minimum_day);
    const float* query = queries.row(options.query_id);

    const auto cpu_results = cpu_exact_search(query, candidates, options.k);
    const auto dpu_run = run_dpu(query, candidates, options.k);

    std::string comparison_error;
    const bool correct = compare_results(dpu_run.output, cpu_results, comparison_error);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Backend: UPMEM SDK functional simulator (backend=simulator)\n"
              << "Dataset: "
              << std::filesystem::absolute(options.dataset_dir).lexically_normal().string()
              << "\n"
              << "Base vectors: " << base.rows << " x " << base.dimensions << "\n"
              << "Query ID: " << options.query_id << "\n"
              << "K: " << options.k << "\n"
              << "Synthetic metadata: base ID i -> 2000-01-01 + i days\n"
              << "Synthetic date range: " << format_date(metadata.front().day_from_2000)
              << " through " << format_date(metadata.back().day_from_2000) << "\n"
              << "Filter: upload_date >= " << options.minimum_date << "\n"
              << "Eligible candidates: " << candidates.size() << "\n"
              << "Host-to-DPU bytes: " << dpu_run.host_to_dpu_bytes << "\n"
              << "DPU-to-host bytes: " << dpu_run.dpu_to_host_bytes << "\n";
    print_neighbors("DPU neighbors:", dpu_run.output);
    print_neighbors("CPU reference neighbors:", cpu_results);
    std::cout << std::scientific << std::setprecision(3)
              << "Distance tolerance: abs <= " << distance_absolute_tolerance
              << " + " << distance_relative_tolerance << " * abs(reference)\n"
              << std::fixed << std::setprecision(4);
    if (!correct) {
        std::cout << "Correctness: FAIL (" << comparison_error << ")\n";
        return EXIT_FAILURE;
    }
    std::cout << "Correctness: PASS\n";
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(parse_options(argc, argv));
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

