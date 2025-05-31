#pragma once
#include <variant>
#include <functional>
#include <stdexcept>
#include <concepts>

namespace core {

template<typename T, typename E>
class Result {
private:
    std::variant<T, E> value_;
    bool is_ok_;
    
public:
    // Constructors
    static Result Ok(T&& value) { return Result(std::move(value), OkTag{}); }
    static Result Ok(const T& value) { return Result(value, OkTag{}); }
    static Result Err(E&& error) { return Result(std::move(error), ErrTag{}); }
    static Result Err(const E& error) { return Result(error, ErrTag{}); }
    
    // State queries
    bool is_ok() const { return is_ok_; }
    bool is_err() const { return !is_ok_; }
    
    // Unwrapping (unsafe)
    T unwrap() && {
        if (is_err()) throw std::runtime_error("Called unwrap on Err value");
        return std::move(std::get<0>(value_));
    }
    
    const T& unwrap() const& {
        if (is_err()) throw std::runtime_error("Called unwrap on Err value");
        return std::get<0>(value_);
    }
    
    T unwrap_or(T&& default_value) && {
        return is_ok() ? std::move(std::get<0>(value_)) : std::move(default_value);
    }
    
    T unwrap_or(const T& default_value) const& {
        return is_ok() ? std::get<0>(value_) : default_value;
    }
    
    T expect(const std::string& msg) && {
        if (is_err()) throw std::runtime_error(msg);
        return std::move(std::get<0>(value_));
    }
    
    // Monadic operations
    template<typename F>
    auto map(F&& func) && -> Result<std::invoke_result_t<F, T>, E> {
        if (is_ok()) {
            return Result<std::invoke_result_t<F, T>, E>::Ok(func(std::move(std::get<0>(value_))));
        }
        return Result<std::invoke_result_t<F, T>, E>::Err(std::move(std::get<1>(value_)));
    }
    
    template<typename F>
    auto map(F&& func) const& -> Result<std::invoke_result_t<F, const T&>, E> {
        if (is_ok()) {
            return Result<std::invoke_result_t<F, const T&>, E>::Ok(func(std::get<0>(value_)));
        }
        return Result<std::invoke_result_t<F, const T&>, E>::Err(std::get<1>(value_));
    }
    
    template<typename F>
    auto and_then(F&& func) && -> std::invoke_result_t<F, T> {
        if (is_ok()) {
            return func(std::move(std::get<0>(value_)));
        }
        return std::invoke_result_t<F, T>::Err(std::move(std::get<1>(value_)));
    }
    
    template<typename F>
    auto and_then(F&& func) const& -> std::invoke_result_t<F, const T&> {
        if (is_ok()) {
            return func(std::get<0>(value_));
        }
        return std::invoke_result_t<F, const T&>::Err(std::get<1>(value_));
    }
    
    template<typename F>
    Result or_else(F&& func) && {
        if (is_err()) {
            return func(std::move(std::get<1>(value_)));
        }
        return std::move(*this);
    }
    
    // Error access
    const E& error() const& {
        if (is_ok()) throw std::runtime_error("Called error() on Ok value");
        return std::get<1>(value_);
    }
    
    E error() && {
        if (is_ok()) throw std::runtime_error("Called error() on Ok value");
        return std::move(std::get<1>(value_));
    }

private:
    struct OkTag {};
    struct ErrTag {};
    
    Result(T&& value, OkTag) : value_(std::in_place_index<0>, std::move(value)), is_ok_(true) {}
    Result(const T& value, OkTag) : value_(std::in_place_index<0>, value), is_ok_(true) {}
    Result(E&& error, ErrTag) : value_(std::in_place_index<1>, std::move(error)), is_ok_(false) {}
    Result(const E& error, ErrTag) : value_(std::in_place_index<1>, error), is_ok_(false) {}
};

// Helper functions for cleaner syntax
template<typename T>
auto Ok(T&& value) {
    return [value = std::forward<T>(value)]<typename E>() mutable {
        return Result<std::decay_t<T>, E>::Ok(std::forward<T>(value));
    };
}

template<typename E>  
auto Err(E&& error) {
    return [error = std::forward<E>(error)]<typename T>() mutable {
        return Result<T, std::decay_t<E>>::Err(std::forward<E>(error));
    };
}

} // namespace core