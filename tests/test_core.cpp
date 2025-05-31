#include "catch.hpp"
#include "../core/result.h"
#include "../core/option.h"
#include <string>

using namespace core;

TEST_CASE("Result monadic operations", "[result]") {
    
    SECTION("Ok value operations") {
        auto result = Result<int, std::string>::Ok(42);
        
        REQUIRE(result.is_ok());
        REQUIRE_FALSE(result.is_err());
        REQUIRE(result.unwrap() == 42);
        REQUIRE(result.unwrap_or(0) == 42);
    }
    
    SECTION("Err value operations") {
        auto result = Result<int, std::string>::Err("error");
        
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.is_err());
        REQUIRE(result.unwrap_or(99) == 99);
        REQUIRE(result.error() == "error");
    }
    
    SECTION("Map operation") {
        auto result = Result<int, std::string>::Ok(5)
            .map([](int x) { return x * 2; })
            .map([](int x) { return std::to_string(x); });
            
        REQUIRE(result.is_ok());
        REQUIRE(result.unwrap() == "10");
    }
    
    SECTION("Map preserves errors") {
        auto result = Result<int, std::string>::Err("failed")
            .map([](int x) { return x * 2; })
            .map([](int x) { return std::to_string(x); });
            
        REQUIRE(result.is_err());
        REQUIRE(result.error() == "failed");
    }
    
    SECTION("and_then operation") {
        auto divide = [](int x) -> Result<int, std::string> {
            if (x == 0) return Result<int, std::string>::Err("division by zero");
            return Result<int, std::string>::Ok(100 / x);
        };
        
        auto result1 = Result<int, std::string>::Ok(5)
            .and_then(divide);
        REQUIRE(result1.is_ok());
        REQUIRE(result1.unwrap() == 20);
        
        auto result2 = Result<int, std::string>::Ok(0)
            .and_then(divide);
        REQUIRE(result2.is_err());
        REQUIRE(result2.error() == "division by zero");
    }
}

TEST_CASE("Option monadic operations", "[option]") {
    
    SECTION("Some value operations") {
        auto opt = Option<int>::Some(42);
        
        REQUIRE(opt.is_some());
        REQUIRE_FALSE(opt.is_none());
        REQUIRE(opt.unwrap() == 42);
        REQUIRE(opt.unwrap_or(0) == 42);
        REQUIRE(static_cast<bool>(opt));
    }
    
    SECTION("None value operations") {
        auto opt = Option<int>::None();
        
        REQUIRE_FALSE(opt.is_some());
        REQUIRE(opt.is_none());
        REQUIRE(opt.unwrap_or(99) == 99);
        REQUIRE_FALSE(static_cast<bool>(opt));
    }
    
    SECTION("Map operation") {
        auto result = Some(5)
            .map([](int x) { return x * 2; })
            .map([](int x) { return std::to_string(x); });
            
        REQUIRE(result.is_some());
        REQUIRE(result.unwrap() == "10");
    }
    
    SECTION("Map on None") {
        auto result = None<int>()
            .map([](int x) { return x * 2; })
            .map([](int x) { return std::to_string(x); });
            
        REQUIRE(result.is_none());
    }
    
    SECTION("and_then operation") {
        auto safe_divide = [](int x) -> Option<int> {
            if (x == 0) return None<int>();
            return Some(100 / x);
        };
        
        auto result1 = Some(5).and_then(safe_divide);
        REQUIRE(result1.is_some());
        REQUIRE(result1.unwrap() == 20);
        
        auto result2 = Some(0).and_then(safe_divide);
        REQUIRE(result2.is_none());
    }
    
    SECTION("Filter operation") {
        auto is_even = [](int x) { return x % 2 == 0; };
        
        auto result1 = Some(4).filter(is_even);
        REQUIRE(result1.is_some());
        REQUIRE(result1.unwrap() == 4);
        
        auto result2 = Some(3).filter(is_even);
        REQUIRE(result2.is_none());
    }
    
    SECTION("Chaining operations") {
        auto result = Some(20)
            .filter([](int x) { return x > 10; })
            .map([](int x) { return x / 2; })
            .and_then([](int x) -> Option<std::string> {
                if (x == 10) return Some(std::string("ten"));
                return None<std::string>();
            });
            
        REQUIRE(result.is_some());
        REQUIRE(result.unwrap() == "ten");
    }
}