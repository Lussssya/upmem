#include "vecs_io.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace texmex {
namespace {

std::uint32_t decode_u32_le(const std::array<unsigned char, 4>& bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::uint32_t read_u32_le(std::ifstream& input, const std::filesystem::path& path) {
    std::array<unsigned char, 4> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        throw std::runtime_error("unexpected end of vector file: " + path.string());
    }
    return decode_u32_le(bytes);
}

template <typename T>
T decode_component(std::uint32_t bits) {
    if constexpr (std::is_same_v<T, float>) {
        static_assert(sizeof(float) == sizeof(bits));
        float value{};
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    } else {
        return static_cast<std::int32_t>(bits);
    }
}

template <typename T>
Matrix<T> read_vecs(const std::filesystem::path& path) {
    std::error_code error;
    const auto file_size = std::filesystem::file_size(path, error);
    if (error) {
        throw std::runtime_error("cannot inspect vector file '" + path.string() +
                                 "': " + error.message());
    }
    if (file_size < sizeof(std::uint32_t)) {
        throw std::runtime_error("empty or truncated vector file: " + path.string());
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open vector file: " + path.string());
    }

    const auto first_dimension_bits = read_u32_le(input, path);
    if (first_dimension_bits == 0 ||
        first_dimension_bits > static_cast<std::uint32_t>(
                                   std::numeric_limits<std::int32_t>::max())) {
        throw std::runtime_error("invalid vector dimension in: " + path.string());
    }
    const std::size_t dimensions = first_dimension_bits;
    if (dimensions > (std::numeric_limits<std::size_t>::max() / 4U) - 1U) {
        throw std::runtime_error("vector dimension is too large in: " + path.string());
    }
    const std::size_t record_bytes = (dimensions + 1U) * sizeof(std::uint32_t);
    if (file_size % record_bytes != 0) {
        throw std::runtime_error("malformed or truncated vector file: " + path.string());
    }
    const std::size_t rows = file_size / record_bytes;
    if (rows == 0 || dimensions > std::numeric_limits<std::size_t>::max() / rows) {
        throw std::runtime_error("invalid vector matrix size in: " + path.string());
    }

    Matrix<T> matrix{rows, dimensions, {}};
    matrix.values.resize(rows * dimensions);

    input.seekg(0, std::ios::beg);
    for (std::size_t row = 0; row < rows; ++row) {
        const auto row_dimension = read_u32_le(input, path);
        if (row_dimension != dimensions) {
            throw std::runtime_error("inconsistent vector dimension at record " +
                                     std::to_string(row) + " in: " + path.string());
        }
        for (std::size_t column = 0; column < dimensions; ++column) {
            matrix.values[row * dimensions + column] =
                decode_component<T>(read_u32_le(input, path));
        }
    }
    if (input.peek() != std::ifstream::traits_type::eof()) {
        throw std::runtime_error("unexpected trailing data in: " + path.string());
    }
    return matrix;
}

}  // namespace

Matrix<float> read_fvecs(const std::filesystem::path& path) {
    return read_vecs<float>(path);
}

Matrix<std::int32_t> read_ivecs(const std::filesystem::path& path) {
    return read_vecs<std::int32_t>(path);
}

}  // namespace texmex

