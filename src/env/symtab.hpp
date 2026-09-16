#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Macro; // forward declaration

class Symtab {
public:
    Symtab() = default;
    virtual ~Symtab() = default;
    Symtab(Symtab&&) noexcept = default;
    Symtab& operator=(Symtab&&) noexcept = default;

    bool define(std::string name, std::unique_ptr<Macro> macro);
    bool exist(std::string_view name) const;
    Macro* lookup(std::string_view name) const;
    std::unique_ptr<Macro> undef(std::string_view name);
    // Returns (name, Macro*) pairs in insertion order, skipping undefined entries.
    std::vector<std::pair<std::string, Macro*>> symbols() const;

private:
    struct SymEntry {
        std::string            name;
        std::unique_ptr<Macro> macro; // nullptr means the entry was undef'd
    };
    // Transparent hash so exist()/lookup()/undef() can probe the map with a
    // std::string_view directly, without allocating a temporary std::string.
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
    };
    std::vector<SymEntry> sym_order_; // insertion order
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>>
        sym_index_; // name -> index
};
