#pragma once

#include <string>
#include <concepts>

template<typename T>
T Deserialize(const std::string& s);

template<typename T>
std::string Serialize(const T& t);

template<typename T>
concept Serializable = requires(T t){
    { Serialize<T>(t) } -> std::same_as<std::string>;
    { Deserialize<const std::string&>(t) } -> std::same_as<T>;
};
