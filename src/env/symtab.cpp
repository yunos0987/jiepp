#include "symtab.hpp"
#include "../macro/macro.hpp"

#include <utility>

bool Symtab::define(std::string name, std::unique_ptr<Macro> macro) {
    auto it = sym_index_.find(name);
    if (it != sym_index_.end()) {
        // Replace in place to preserve insertion order.
        sym_order_[it->second].macro = std::move(macro);
    } else {
        sym_index_[name] = sym_order_.size();
        sym_order_.push_back({name, std::move(macro)});
    }
    return true;
}

bool Symtab::exist(std::string_view name) const {
    auto it = sym_index_.find(std::string(name));
    if (it != sym_index_.end())
        return sym_order_[it->second].macro != nullptr;
    return false;
}

Macro* Symtab::lookup(std::string_view name) const {
    auto it = sym_index_.find(std::string(name));
    if (it != sym_index_.end())
        return sym_order_[it->second].macro.get();
    return nullptr;
}

std::unique_ptr<Macro> Symtab::undef(std::string_view name) {
    auto it = sym_index_.find(std::string(name));
    if (it != sym_index_.end()) {
        std::size_t idx = it->second;
        std::unique_ptr<Macro> old = std::move(sym_order_[idx].macro);
        sym_order_[idx].macro = nullptr; // mark as undef'd
        sym_index_.erase(it);
        return old;
    }
    return nullptr;
}

std::vector<std::pair<std::string, Macro*>> Symtab::symbols() const {
    std::vector<std::pair<std::string, Macro*>> result;
    result.reserve(sym_order_.size());
    for (const auto& entry : sym_order_)
        if (entry.macro != nullptr)
            result.emplace_back(entry.name, entry.macro.get());
    return result;
}
