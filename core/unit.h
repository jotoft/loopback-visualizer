#pragma once

namespace core {

// Unit type for Result<Unit, E> when no value is needed
struct Unit {
    constexpr bool operator==(const Unit&) const { return true; }
    constexpr bool operator!=(const Unit&) const { return false; }
};

// Convenient constant for Unit
inline constexpr Unit unit{};

} // namespace core