#include "symtab.hpp"
#include "../macro/macro.hpp"

#include <utility>

bool Symtab::define(std::string name, std::unique_ptr<Macro> macro) {
    auto it = sym_index_.find(name);
    if (it != sym_index_.end()) {
        // Replace in place to preserve insertion order. Retire (do not
        // destroy) the superseded Macro: a raw Macro*/FunctionMacro*
        // obtained via an earlier lookup() may still be in use for the rest
        // of the current expansion (F14) — see the lifetime invariant in
        // symtab.hpp.
        if (sym_order_[it->second].macro)
            retired_.push_back(std::move(sym_order_[it->second].macro));
        sym_order_[it->second].macro = std::move(macro);
    } else {
        sym_index_[name] = sym_order_.size();
        sym_order_.push_back({name, std::move(macro)});
    }
    return true;
}

bool Symtab::exist(std::string_view name) const {
    auto it = sym_index_.find(name);
    if (it != sym_index_.end())
        return sym_order_[it->second].macro != nullptr;
    return false;
}

Macro* Symtab::lookup(std::string_view name) const {
    auto it = sym_index_.find(name);
    if (it != sym_index_.end())
        return sym_order_[it->second].macro.get();
    return nullptr;
}

void Symtab::undef(std::string_view name) {
    auto it = sym_index_.find(name);
    if (it != sym_index_.end()) {
        std::size_t idx = it->second;
        if (sym_order_[idx].macro)
            // Retire (do not destroy): see the lifetime invariant in
            // symtab.hpp (F14).
            retired_.push_back(std::move(sym_order_[idx].macro));
        // Keep the slot mapped in sym_index_ (F11): a subsequent define()
        // for the same name then replaces in place instead of appending a
        // new SymEntry, preserving insertion order in symbols()/-dM output.
    }
}

void Symtab::push_macro(std::string_view name) {
    std::shared_ptr<Macro> current;
    if (auto it = sym_index_.find(name); it != sym_index_.end())
        current = sym_order_[it->second].macro;
    auto st = push_stacks_.find(name);
    if (st == push_stacks_.end())
        st = push_stacks_.emplace(std::string(name), std::vector<std::shared_ptr<Macro>>{}).first;
    st->second.push_back(std::move(current));
}

bool Symtab::pop_macro(std::string_view name) {
    auto st = push_stacks_.find(name);
    if (st == push_stacks_.end() || st->second.empty())
        return false;
    std::shared_ptr<Macro> saved = std::move(st->second.back());
    st->second.pop_back();
    auto it = sym_index_.find(name);
    if (it == sym_index_.end()) {
        // The name has never been defined, so `saved` is "undefined" too: a
        // pushed definition had a SymEntry, and entries are never removed.
        return true;
    }
    SymEntry& entry = sym_order_[it->second];
    if (entry.macro == saved)
        return true;
    // Retire (do not destroy) the definition being replaced: see the
    // lifetime invariant in symtab.hpp (F14). The slot stays mapped (F11).
    if (entry.macro)
        retired_.push_back(std::move(entry.macro));
    entry.macro = std::move(saved);
    return true;
}

std::vector<std::pair<std::string, Macro*>> Symtab::symbols() const {
    std::vector<std::pair<std::string, Macro*>> result;
    result.reserve(sym_order_.size());
    for (const auto& entry : sym_order_)
        if (entry.macro != nullptr)
            result.emplace_back(entry.name, entry.macro.get());
    return result;
}
