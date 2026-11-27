#pragma once

#include "vecs_io.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct UploadMetadata {
    std::uint32_t base_id{};
    std::uint32_t day_from_2000{};
};

struct FilteredCandidates {
    std::size_t dimensions{};
    std::vector<float> vectors;
    std::vector<std::uint64_t> original_ids;

    [[nodiscard]] std::size_t size() const {
        return original_ids.size();
    }
};

std::uint32_t parse_date(const std::string& date);
std::string format_date(std::uint32_t day_from_2000);
std::vector<UploadMetadata> generate_upload_metadata(std::size_t vector_count);
FilteredCandidates filter_by_minimum_date(
    const texmex::Matrix<float>& base,
    const std::vector<UploadMetadata>& metadata,
    std::uint32_t minimum_day);

