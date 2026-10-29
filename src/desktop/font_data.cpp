#include "font_data.hpp"

namespace vynexos::desktop::fonts {

std::span<const unsigned char> embedded_font_data(FontWeight weight) noexcept {
    switch (weight) {
    case FontWeight::Regular:
        return {kInterRegular, kInterRegularSize};
    case FontWeight::Medium:
        return {kInterMedium, kInterMediumSize};
    case FontWeight::SemiBold:
    case FontWeight::Bold:
    default:
        return {kInterSemiBold, kInterSemiBoldSize};
    }
}

} // namespace vynexos::desktop::fonts
