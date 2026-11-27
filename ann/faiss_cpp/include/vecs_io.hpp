#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace texmex {

template <typename T>
struct Matrix {
    std::size_t rows{};
    std::size_t dimensions{};
    std::vector<T> values;

    [[nodiscard]] const T* row(std::size_t index) const {
        return values.data() + index * dimensions;
    }
};

Matrix<float> read_fvecs(const std::filesystem::path& path);
Matrix<std::int32_t> read_ivecs(const std::filesystem::path& path);

}  // namespace texmex

