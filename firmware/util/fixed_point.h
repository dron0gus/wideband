#pragma once

#include <cstdint>
#include <type_traits>
#include <limits>

/**
 * @brief A lightweight class for storing scaled values using a base type with ratio scale factor.
 * 
 * This class is designed to be compatible with packed structures
 * 
 * Examples:
 * - ScaledValue<int16_t, 10> multiplies raw values by 10.0 (e.g., 13 raw -> 130.0f)
 * - ScaledValue<int16_t, 1, 10> multiplies raw values by 0.1 (e.g., 128 raw -> 12.8f)
 * 
 * @tparam TStorage Base type for storage (e.g., int8_t, uint8_t, int16_t, uint16_t, etc.).
 * @tparam TScaleFactorNumerator Numerator of the scale factor (e.g., 10 for scale 10.0).
 * @tparam TScaleFactorDenominator Denominator of the scale factor (e.g., 10 for scale 0.1).
 */
template<typename TStorage, uint16_t TScaleFactorNumerator, uint16_t TScaleFactorDenominator = 1>
struct ScaledValue {
    static_assert(std::is_integral<TStorage>::value, "TStorage must be an integral type");
    static_assert(TScaleFactorDenominator != 0, "TScaleFactorDenominator must not be zero");

    static constexpr float scale() {
        return static_cast<float>(TScaleFactorNumerator) / static_cast<float>(TScaleFactorDenominator);
    }

    TStorage value; // Storage using the base type

    // Convert to float
    constexpr float getValue() const {
        return static_cast<float>(value) * scale();
    }

    // Convert from float
    constexpr void setValue(float val) {
        float scaled = val / scale();
        if (scaled < minRawValue()) {
            value = minRawValue();
            return;
        }
        if (scaled > maxRawValue()) {
            value = maxRawValue();
            return;
        }
        value = static_cast<TStorage>(scaled + (scaled >= 0 ? 0.5f : -0.5f)); // Round to nearest
    }

    constexpr TStorage getRaw() const {
        return value;
    }

    constexpr void setRaw(TStorage raw) {
        value = raw;
    }

    // Implicit conversion to float
    constexpr operator float() const {
        return getValue();
    }

    // Implicit assignment from float
    constexpr ScaledValue& operator=(float val) {
        setValue(val);
        return *this;
    }

private:
    // Maximum raw value that can be stored
    constexpr TStorage maxRawValue() const {
        return std::numeric_limits<TStorage>::max();
    }

    // Minimum raw value that can be stored
    constexpr TStorage minRawValue() const {
        return std::numeric_limits<TStorage>::min();
    }
};

// Alias for fixed-point
template<typename TStorage, int TScaleFactor>
using FixedPoint = ScaledValue<TStorage, 1, TScaleFactor>;