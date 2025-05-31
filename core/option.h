#pragma once
#include <optional>
#include <functional>
#include <stdexcept>

namespace core {

template<typename T>
class Option {
private:
    std::optional<T> value_;
    
public:
    // Constructors
    Option() : value_(std::nullopt) {}
    static Option Some(T&& value) { return Option(std::move(value)); }
    static Option Some(const T& value) { return Option(value); }
    static Option None() { return Option(); }
    
    // State queries
    bool is_some() const { return value_.has_value(); }
    bool is_none() const { return !value_.has_value(); }
    
    // Unwrapping (unsafe)
    T unwrap() && {
        if (is_none()) throw std::runtime_error("Called unwrap on None value");
        return std::move(*value_);
    }
    
    const T& unwrap() const& {
        if (is_none()) throw std::runtime_error("Called unwrap on None value");
        return *value_;
    }
    
    T unwrap_or(T&& default_value) && {
        return is_some() ? std::move(*value_) : std::move(default_value);
    }
    
    T unwrap_or(const T& default_value) const& {
        return is_some() ? *value_ : default_value;
    }
    
    T expect(const std::string& msg) && {
        if (is_none()) throw std::runtime_error(msg);
        return std::move(*value_);
    }
    
    // Monadic operations
    template<typename F>
    auto map(F&& func) && -> Option<std::invoke_result_t<F, T>> {
        if (is_some()) {
            return Option<std::invoke_result_t<F, T>>::Some(func(std::move(*value_)));
        }
        return Option<std::invoke_result_t<F, T>>::None();
    }
    
    template<typename F>
    auto map(F&& func) const& -> Option<std::invoke_result_t<F, const T&>> {
        if (is_some()) {
            return Option<std::invoke_result_t<F, const T&>>::Some(func(*value_));
        }
        return Option<std::invoke_result_t<F, const T&>>::None();
    }
    
    template<typename F>
    auto and_then(F&& func) && -> std::invoke_result_t<F, T> {
        if (is_some()) {
            return func(std::move(*value_));
        }
        return std::invoke_result_t<F, T>::None();
    }
    
    template<typename F>
    auto and_then(F&& func) const& -> std::invoke_result_t<F, const T&> {
        if (is_some()) {
            return func(*value_);
        }
        return std::invoke_result_t<F, const T&>::None();
    }
    
    template<typename F>
    Option or_else(F&& func) && {
        if (is_none()) {
            return func();
        }
        return std::move(*this);
    }
    
    // Filter operation
    template<typename Predicate>
    Option filter(Predicate&& pred) && {
        if (is_some() && pred(*value_)) {
            return std::move(*this);
        }
        return Option::None();
    }
    
    template<typename Predicate>
    Option filter(Predicate&& pred) const& {
        if (is_some() && pred(*value_)) {
            return *this;
        }
        return Option::None();
    }
    
    // Convert to bool for if statements
    explicit operator bool() const { return is_some(); }
    
    // Dereference operators for convenience
    const T& operator*() const& { return unwrap(); }
    T& operator*() & { return *value_; }
    T operator*() && { return std::move(*this).unwrap(); }
    
    const T* operator->() const { return &(*value_); }
    T* operator->() { return &(*value_); }

private:
    Option(T&& value) : value_(std::move(value)) {}
    Option(const T& value) : value_(value) {}
};

// Helper functions
template<typename T>
Option<std::decay_t<T>> Some(T&& value) {
    return Option<std::decay_t<T>>::Some(std::forward<T>(value));
}

template<typename T>
Option<T> None() {
    return Option<T>::None();
}

} // namespace core