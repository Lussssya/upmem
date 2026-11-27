#include "metadata.hpp"
#include "search_common.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {

constexpr int first_year = 2000;
constexpr int last_supported_year = 2100;

bool is_leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int days_in_month(int year, int month) {
    constexpr std::array<int, 12> days{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[static_cast<std::size_t>(month - 1)];
}

std::uint32_t date_to_day(int year, int month, int day) {
    if (year < first_year || year > last_supported_year ||
        day < 1 || day > days_in_month(year, month)) {
        throw std::runtime_error("date must be a valid YYYY-MM-DD between 2000 and 2100");
    }

    std::uint32_t result = 0;
    for (int current_year = first_year; current_year < year; ++current_year) {
        result += is_leap_year(current_year) ? 366U : 365U;
    }
    for (int current_month = 1; current_month < month; ++current_month) {
        result += static_cast<std::uint32_t>(days_in_month(year, current_month));
    }
    return result + static_cast<std::uint32_t>(day - 1);
}

}  // namespace

std::uint32_t parse_date(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        throw std::runtime_error("date must use YYYY-MM-DD format");
    }
    for (std::size_t index = 0; index < date.size(); ++index) {
        if (index != 4 && index != 7 &&
            (date[index] < '0' || date[index] > '9')) {
            throw std::runtime_error("date must use YYYY-MM-DD format");
        }
    }
    try {
        const int year = std::stoi(date.substr(0, 4));
        const int month = std::stoi(date.substr(5, 2));
        const int day = std::stoi(date.substr(8, 2));
        return date_to_day(year, month, day);
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("date must use YYYY-MM-DD format");
    } catch (const std::out_of_range&) {
        throw std::runtime_error("date component is out of range");
    }
}

std::string format_date(std::uint32_t day_from_2000) {
    int year = first_year;
    while (year <= last_supported_year) {
        const auto year_days = is_leap_year(year) ? 366U : 365U;
        if (day_from_2000 < year_days) {
            break;
        }
        day_from_2000 -= year_days;
        ++year;
    }
    if (year > last_supported_year) {
        throw std::runtime_error("synthetic date exceeds the supported year range");
    }

    int month = 1;
    while (month <= 12) {
        const auto month_days =
            static_cast<std::uint32_t>(days_in_month(year, month));
        if (day_from_2000 < month_days) {
            break;
        }
        day_from_2000 -= month_days;
        ++month;
    }

    std::ostringstream result;
    result << std::setfill('0') << std::setw(4) << year << '-'
           << std::setw(2) << month << '-' << std::setw(2)
           << day_from_2000 + 1U;
    return result.str();
}

std::vector<UploadMetadata> generate_upload_metadata(std::size_t vector_count) {
    if (vector_count > MAX_CANDIDATES ||
        vector_count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("too many vectors for synthetic metadata");
    }

    std::vector<UploadMetadata> metadata;
    metadata.reserve(vector_count);
    for (std::size_t id = 0; id < vector_count; ++id) {
        metadata.push_back(
            {static_cast<std::uint32_t>(id), static_cast<std::uint32_t>(id)});
    }
    return metadata;
}

FilteredCandidates filter_by_minimum_date(
    const texmex::Matrix<float>& base,
    const std::vector<UploadMetadata>& metadata,
    std::uint32_t minimum_day) {
    if (metadata.size() != base.rows) {
        throw std::runtime_error("metadata count does not match base-vector count");
    }

    FilteredCandidates filtered;
    filtered.dimensions = base.dimensions;
    for (const auto& record : metadata) {
        if (record.base_id >= base.rows) {
            throw std::runtime_error("metadata contains an out-of-range base ID");
        }
        if (record.day_from_2000 >= minimum_day) {
            filtered.original_ids.push_back(record.base_id);
        }
    }

    filtered.vectors.reserve(filtered.size() * filtered.dimensions);
    for (const auto id : filtered.original_ids) {
        const float* vector = base.row(static_cast<std::size_t>(id));
        filtered.vectors.insert(
            filtered.vectors.end(), vector, vector + filtered.dimensions);
    }
    return filtered;
}

