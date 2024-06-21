// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cmath>

#include <boxed-cpp/boxed.hpp>

namespace vtpty
{
/// Representsthe width in pixels of an image (see ImageSize).
using Width = boxed::boxed<unsigned>;

/// Representsthe height in pixels of an image (see ImageSize).
using Height = boxed::boxed<unsigned>;

/// ImageSize represents the 2-dimensional size of an image (pixmap).
struct ImageSize
{
    vtpty::Width width;
    vtpty::Height height;

    [[nodiscard]] constexpr size_t area() const noexcept
    {
        return unbox<size_t>(width) * unbox<size_t>(height);
    }
};

constexpr bool operator==(ImageSize a, ImageSize b) noexcept
{
    return a.width == b.width && a.height == b.height;
}

constexpr bool operator!=(ImageSize a, ImageSize b) noexcept
{
    return !(a == b);
}

constexpr bool operator<(ImageSize a, ImageSize b) noexcept
{
    return a.width < b.width || (a.width == b.width && a.height < b.height);
}

constexpr ImageSize operator+(ImageSize a, ImageSize b) noexcept
{
    return ImageSize { a.width + b.width, a.height + b.height };
}

constexpr ImageSize operator-(ImageSize a, ImageSize b) noexcept
{
    return ImageSize { a.width - b.width, a.height - b.height };
}

constexpr ImageSize operator/(ImageSize a, ImageSize b) noexcept
{
    return ImageSize { a.width / b.width, a.height / b.height };
}

inline ImageSize operator/(ImageSize a, double scalar) noexcept
{
    return ImageSize { Width::cast_from(std::ceil(double(*a.width) / scalar)),
                       Height::cast_from(std::ceil(double(*a.height) / scalar)) };
}

inline ImageSize operator*(ImageSize a, double scalar) noexcept
{
    return ImageSize { Width::cast_from(std::ceil(double(*a.width) * scalar)),
                       Height::cast_from(std::ceil(double(*a.height) * scalar)) };
}

constexpr ImageSize min(ImageSize a, ImageSize b) noexcept
{
    return ImageSize { std::min(a.width, b.width), std::min(a.height, b.height) };
}

constexpr ImageSize max(ImageSize a, ImageSize b) noexcept
{
    return ImageSize { std::max(a.width, b.width), std::max(a.height, b.height) };
}

} // namespace vtpty

template <>
struct fmt::formatter<vtpty::ImageSize>: fmt::formatter<std::string>
{
    auto format(vtpty::ImageSize value, format_context& ctx) -> format_context::iterator
    {
        return formatter<std::string>::format(fmt::format("{}x{}", value.width, value.height), ctx);
    }
};
