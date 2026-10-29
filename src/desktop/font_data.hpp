#pragma once
#include <cstddef>
#include <span>
#include "vynexos/desktop/theme.hpp"

namespace vynexos::desktop::fonts {

// The Inter 4.1 static fonts from assets/fonts/inter/, embedded unmodified at build time by tools/bin2c.
// Defined in the generated file font_data_generated.cpp.
extern const unsigned char kInterRegular[];
extern const std::size_t kInterRegularSize;
extern const unsigned char kInterMedium[];
extern const std::size_t kInterMediumSize;
extern const unsigned char kInterSemiBold[];
extern const std::size_t kInterSemiBoldSize;

/// The embedded font bytes for a weight. Only Regular, Medium and SemiBold are embedded; Bold maps to SemiBold
/// (the nearest available weight).
[[nodiscard]] std::span<const unsigned char> embedded_font_data(FontWeight weight) noexcept;

} // namespace vynexos::desktop::fonts
