#include "DebugCommand.hpp"

namespace tinyengine::debugcmd {

DebugCommandRegistry& DebugCommandRegistry::instance()
{
    static DebugCommandRegistry s_instance;
    return s_instance;
}

void DebugCommandRegistry::addEntry(DebugCommandEntry entry)
{
    for (auto& e : entries_) {
        if (e.meta.name == entry.meta.name) {
            e = std::move(entry);
            return;
        }
    }
    entries_.push_back(std::move(entry));
}

size_t DebugCommandRegistry::count() const
{
    return entries_.size();
}

const DebugCommandEntry* DebugCommandRegistry::at(size_t index) const
{
    if (index >= entries_.size()) return nullptr;
    return &entries_[index];
}

const DebugCommandEntry* DebugCommandRegistry::find(const std::string& name) const
{
    for (const auto& e : entries_)
        if (e.meta.name == name) return &e;
    return nullptr;
}

std::string DebugCommandRegistry::invoke(const std::string& name, const std::vector<DebugArg>& args) const
{
    const DebugCommandEntry* e = find(name);
    if (!e) return "error: unknown command '" + name + "'";
    if (args.size() < e->meta.params.size())
        return "error: command '" + name + "' expects " +
               std::to_string(e->meta.params.size()) + " args";
    return e->invoke(args);
}

} // namespace tinyengine::debugcmd
