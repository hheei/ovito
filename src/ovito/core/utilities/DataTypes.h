// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file
 * \brief This header file defines the default data types and numeric constants used throughout the program.
 */

#pragma once

#include <cstdint>
#include <concepts>
#include <numbers>
#include <limits>

namespace Ovito {

#ifdef FLOATTYPE_FLOAT

    /// The default floating-point type used by OVITO.
    using FloatType = float;

    /// The format specifier to be passed to the sscanf() function to parse floating-point numbers of type Ovito::FloatType.
    #define FLOATTYPE_SCANF_STRING      "%g"

#else

    /// The default floating-point type used by OVITO.
    using FloatType = double;

    /// The format specifier to be passed to the sscanf() function to parse floating-point numbers of type Ovito::FloatType.
    #define FLOATTYPE_SCANF_STRING      "%lg"

#endif

/// Low-precision floating-point type used for graphics-related data.
using GraphicsFloatType = float;

/// The constant number pi.
template<std::floating_point T>
inline constexpr T pi_v = std::numbers::pi_v<T>;
inline constexpr FloatType pi = pi_v<FloatType>; // pi for our default floating-point type

/// A small epsilon, which is used in OVITO to test if a number is (almost) zero:
template<typename T> requires (std::integral<T> || std::floating_point<T>)
inline constexpr T epsilon_v = (T)0;
template<>
inline constexpr double epsilon_v<double> = 1e-12;
template<>
inline constexpr float epsilon_v<float> = 1e-6f;
inline constexpr FloatType epsilon = epsilon_v<FloatType>; // Epsilon for our default floating-point type

/// The maximum value for floating-point variables of type Ovito::FloatType.
inline constexpr FloatType FLOATTYPE_MAX = std::numeric_limits<FloatType>::max();

/// The lowest value for floating-point variables of type Ovito::FloatType.
inline constexpr FloatType FLOATTYPE_MIN = std::numeric_limits<FloatType>::lowest();

/// The format specifier to be passed to the sscanf() function to parse low-precision floating-point numbers of type
/// Ovito::GraphicsFloatType.
#define GRAPHICS_FLOATTYPE_SCANF_STRING "%g"

/// Data type used for storing unique identifiers.
using IdentifierIntType = int64_t;

/// Data type used for storing element selections.
using SelectionIntType = int8_t;

}   // End of namespace
