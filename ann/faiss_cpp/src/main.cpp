#include "vecs_io.hpp"

#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFFlat.h>

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
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using FaissId = faiss::idx_t;

struct Options {
    std::filesystem::path dataset_dir{"../datasets/siftsmall"};
    std::size_t nlist{100};
    std::size_t nprobe{10};
    std::size_t k{100};
};

struct SiftSmall {
    texmex::Matrix<float> base;
    texmex::Matrix<float> queries;
    texmex::Matrix<float> learning;
    texmex::Matrix<std::int32_t> ground_truth;
};

struct SearchResults {
    std::size_t rows{};
    std::size_t k{};
    std::vector<float> distances;
    std::vector<FaissId> ids;
};

std::size_t parse_positive_size(const std::string& value, const std::string& option) {
    std::size_t consumed = 0;
    unsigned long long parsed = 0;
    try {
        parsed = std::stoull(value, &consumed, 10);
    } catch (const std::exception&) {
        throw std::runtime_error(option + " requires a positive integer");
    }
    if (consumed != value.size() || parsed == 0 ||
        parsed > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(option + " requires a positive integer");
    }
    return static_cast<std::size_t>(parsed);
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program
              << " [--dataset-dir PATH] [--nlist N] [--nprobe N] [--k N]\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (i + 1 >= argc) {
            throw std::runtime_error("missing value for option: " + argument);
        }
        const std::string value = argv[++i];
        if (argument == "--dataset-dir") {
            options.dataset_dir = value;
        } else if (argument == "--nlist") {
            options.nlist = parse_positive_size(value, argument);
        } else if (argument == "--nprobe") {
            options.nprobe = parse_positive_size(value, argument);
        } else if (argument == "--k") {
            options.k = parse_positive_size(value, argument);
        } else {
            throw std::runtime_error("unknown option: " + argument);
        }
    }
    if (options.nprobe > options.nlist) {
        throw std::runtime_error("nprobe must not exceed nlist");
    }
    return options;
}

SiftSmall load_siftsmall(const std::filesystem::path& directory) {
    SiftSmall data{
        texmex::read_fvecs(directory / "siftsmall_base.fvecs"),
        texmex::read_fvecs(directory / "siftsmall_query.fvecs"),
        texmex::read_fvecs(directory / "siftsmall_learn.fvecs"),
        texmex::read_ivecs(directory / "siftsmall_groundtruth.ivecs")};

    const auto dimensions = data.base.dimensions;
    if (data.queries.dimensions != dimensions || data.learning.dimensions != dimensions) {
        throw std::runtime_error("base, query, and learning dimensions do not match");
    }
    if (data.ground_truth.rows != data.queries.rows) {
        throw std::runtime_error("ground-truth and query counts do not match");
    }
    for (const auto id : data.ground_truth.values) {
        if (id < 0 || static_cast<std::size_t>(id) >= data.base.rows) {
            throw std::runtime_error("ground truth contains an out-of-range base ID");
        }
    }
    return data;
}

SearchResults search(faiss::Index& index,
                     const texmex::Matrix<float>& queries,
                     std::size_t k) {
    SearchResults results{queries.rows, k, {}, {}};
    results.distances.resize(queries.rows * k);
    results.ids.resize(queries.rows * k);
    index.search(static_cast<FaissId>(queries.rows),
                 queries.values.data(),
                 static_cast<FaissId>(k),
                 results.distances.data(),
                 results.ids.data());
    return results;
}

double overlap_recall(const std::vector<FaissId>& found,
                      const texmex::Matrix<std::int32_t>& expected,
                      std::size_t k) {
    std::size_t matches = 0;
    for (std::size_t row = 0; row < expected.rows; ++row) {
        std::unordered_set<FaissId> expected_ids;
        expected_ids.reserve(k);
        for (std::size_t column = 0; column < k; ++column) {
            expected_ids.insert(expected.values[row * expected.dimensions + column]);
        }
        for (std::size_t column = 0; column < k; ++column) {
            matches += expected_ids.contains(found[row * k + column]) ? 1U : 0U;
        }
    }
    return static_cast<double>(matches) /
           static_cast<double>(expected.rows * k);
}

double overlap_recall(const std::vector<FaissId>& found,
                      const std::vector<FaissId>& expected,
                      std::size_t rows,
                      std::size_t k) {
    std::size_t matches = 0;
    for (std::size_t row = 0; row < rows; ++row) {
        std::unordered_set<FaissId> expected_ids(
            expected.begin() + static_cast<std::ptrdiff_t>(row * k),
            expected.begin() + static_cast<std::ptrdiff_t>((row + 1U) * k));
        for (std::size_t column = 0; column < k; ++column) {
            matches += expected_ids.contains(found[row * k + column]) ? 1U : 0U;
        }
    }
    return static_cast<double>(matches) / static_cast<double>(rows * k);
}

double texmex_true_nn_recall(const std::vector<FaissId>& found,
                             std::size_t result_k,
                             const texmex::Matrix<std::int32_t>& ground_truth,
                             std::size_t rank) {
    rank = std::min(rank, result_k);
    std::size_t matches = 0;
    for (std::size_t row = 0; row < ground_truth.rows; ++row) {
        const FaissId true_neighbor = ground_truth.values[row * ground_truth.dimensions];
        const auto begin = found.begin() + static_cast<std::ptrdiff_t>(row * result_k);
        matches += std::find(begin, begin + static_cast<std::ptrdiff_t>(rank),
                             true_neighbor) != begin + static_cast<std::ptrdiff_t>(rank)
                       ? 1U
                       : 0U;
    }
    return static_cast<double>(matches) / static_cast<double>(ground_truth.rows);
}

void verify_full_probe(const SearchResults& full, const SearchResults& exact) {
    if (full.ids != exact.ids) {
        throw std::runtime_error("IVF with nprobe=nlist does not match exact IDs");
    }
    for (std::size_t i = 0; i < full.distances.size(); ++i) {
        const float difference = std::abs(full.distances[i] - exact.distances[i]);
        const float tolerance = 1.0e-5F + 1.0e-5F * std::abs(exact.distances[i]);
        if (difference > tolerance) {
            throw std::runtime_error(
                "IVF with nprobe=nlist does not match exact distances");
        }
    }
}

int run(const Options& options) {
    auto data = load_siftsmall(options.dataset_dir);
    if (options.nlist > data.learning.rows) {
        throw std::runtime_error("nlist exceeds the number of learning vectors");
    }
    if (options.k > data.base.rows) {
        throw std::runtime_error("k exceeds the number of base vectors");
    }
    if (options.k > data.ground_truth.dimensions) {
        throw std::runtime_error("k exceeds the ground-truth neighbors per query");
    }

    const auto dimension = static_cast<FaissId>(data.base.dimensions);
    faiss::IndexFlatL2 quantizer(dimension);
    faiss::IndexIVFFlat ivf(&quantizer,
                            dimension,
                            static_cast<FaissId>(options.nlist),
                            faiss::METRIC_L2);
    ivf.train(static_cast<FaissId>(data.learning.rows), data.learning.values.data());
    ivf.add(static_cast<FaissId>(data.base.rows), data.base.values.data());
    ivf.nprobe = options.nprobe;
    const SearchResults approximate = search(ivf, data.queries, options.k);

    faiss::IndexFlatL2 exact_index(dimension);
    exact_index.add(static_cast<FaissId>(data.base.rows), data.base.values.data());
    const SearchResults exact = search(exact_index, data.queries, options.k);

    const double exact_ground_truth =
        overlap_recall(exact.ids, data.ground_truth, options.k);
    if (exact_ground_truth != 1.0) {
        throw std::runtime_error("exact FAISS does not match all ground-truth IDs");
    }
    for (std::size_t row = 0; row < data.queries.rows; ++row) {
        if (exact.ids[row * options.k] !=
            data.ground_truth.values[row * data.ground_truth.dimensions]) {
            throw std::runtime_error("exact FAISS true nearest neighbor mismatch");
        }
    }

    ivf.nprobe = options.nlist;
    const SearchResults full_probe = search(ivf, data.queries, options.k);
    verify_full_probe(full_probe, exact);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Dataset: TEXMEX SIFT-small\n"
              << "Dataset directory: "
              << std::filesystem::absolute(options.dataset_dir).lexically_normal().string()
              << "\n"
              << "Learning vectors: " << data.learning.rows << " x "
              << data.learning.dimensions << "\n"
              << "Database vectors: " << data.base.rows << " x "
              << data.base.dimensions << "\n"
              << "Query vectors: " << data.queries.rows << " x "
              << data.queries.dimensions << "\n"
              << "Ground truth: " << data.ground_truth.rows << " x "
              << data.ground_truth.dimensions << "\n"
              << "IVF parameters: nlist=" << options.nlist
              << ", nprobe=" << options.nprobe << ", k=" << options.k
              << ", metric=squared L2\n"
              << "IVF overlap recall@" << options.k << " vs ground truth: "
              << overlap_recall(approximate.ids, data.ground_truth, options.k) << "\n"
              << "IVF overlap recall@" << options.k << " vs exact FAISS: "
              << overlap_recall(approximate.ids, exact.ids, data.queries.rows, options.k)
              << "\n";
    for (const std::size_t rank : {1U, 10U, 100U}) {
        if (rank <= options.k) {
            std::cout << "IVF TEXMEX recall@" << rank << ": "
                      << texmex_true_nn_recall(
                             approximate.ids, options.k, data.ground_truth, rank)
                      << "\n";
        }
    }
    std::cout << "Exact FAISS overlap recall@" << options.k
              << " vs ground truth: " << exact_ground_truth << "\n"
              << "Verification: PASS\n"
              << "  - exact FAISS matches all provided ground-truth IDs\n"
              << "  - IVF with nprobe=nlist matches exact FAISS IDs and distances\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(parse_options(argc, argv));
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

