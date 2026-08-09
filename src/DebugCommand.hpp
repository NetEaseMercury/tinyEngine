#pragma once

// Lightweight reflective debug command system. A command is any callable whose
// parameters are drawn from a fixed whitelist of types; the registry stores
// signature metadata (extracted at compile time via variadic templates) so the
// C ABI can export it and the C# Editor can reflect it into UI controls.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Parameter type whitelist. Mirrored by TeDebugParamType in the C ABI and by
// the C# editor. Keep the numeric values in sync across all three.
enum class DebugParamType : int32_t {
    Int    = 0,
    Float  = 1,
    Bool   = 2,
    String = 3,
    Enum   = 4,   // backed by int32; enum option labels carried in metadata
    Vec3   = 5,
    UInt64 = 6,
    Color  = 7,   // rgba, backed by 4 floats
};

// Small POD vector types used as command parameter types, to avoid pulling glm
// into this header. Commands convert to glm inside their lambdas.
struct DebugVec3  { float x = 0, y = 0, z = 0; };
struct DebugColor { float r = 0, g = 0, b = 0, a = 1; };

// A single argument value passed from the Editor to a command invocation. Only
// the field matching 'type' is meaningful.
struct DebugArg {
    DebugParamType type = DebugParamType::Int;
    int64_t        i    = 0;      // Int / Enum / UInt64 (reinterpreted)
    double         f    = 0.0;    // Float
    bool           b    = false;  // Bool
    std::string    s;             // String
    float          v[4] = { 0, 0, 0, 0 };  // Vec3 (xyz) / Color (rgba)
};

// Static description of one parameter, exposed for UI generation.
struct DebugParamMeta {
    DebugParamType type = DebugParamType::Int;
    std::string    name;
    std::string    enumValues;   // "|"-separated labels when type == Enum
    double         minVal = 0.0; // UI hint for Int/Float sliders
    double         maxVal = 0.0; // maxVal <= minVal means "no range hint"
    DebugArg       defVal;       // default value used to seed the UI
};

struct DebugCommandMeta {
    std::string                 name;   // dotted namespace, e.g. "render.set_clear_color"
    std::string                 help;
    std::vector<DebugParamMeta> params;
};

// The type-erased command entry stored in the registry.
struct DebugCommandEntry {
    DebugCommandMeta                             meta;
    std::function<std::string(const std::vector<DebugArg>&)> invoke;
};

namespace tinyengine::debugcmd {

// ── Compile-time mapping from a C++ parameter type to DebugParamType ──────────
template <class T> struct ParamTypeOf;
template <> struct ParamTypeOf<int32_t>     { static constexpr DebugParamType value = DebugParamType::Int; };
template <> struct ParamTypeOf<float>       { static constexpr DebugParamType value = DebugParamType::Float; };
template <> struct ParamTypeOf<bool>        { static constexpr DebugParamType value = DebugParamType::Bool; };
template <> struct ParamTypeOf<std::string> { static constexpr DebugParamType value = DebugParamType::String; };
template <> struct ParamTypeOf<uint64_t>    { static constexpr DebugParamType value = DebugParamType::UInt64; };

// Extract a strongly-typed value from a DebugArg for parameter index I.
template <class T> T extractArg(const DebugArg& a);
template <> inline int32_t     extractArg<int32_t>(const DebugArg& a)     { return static_cast<int32_t>(a.i); }
template <> inline float       extractArg<float>(const DebugArg& a)       { return static_cast<float>(a.f); }
template <> inline bool        extractArg<bool>(const DebugArg& a)        { return a.b; }
template <> inline std::string extractArg<std::string>(const DebugArg& a) { return a.s; }
template <> inline uint64_t    extractArg<uint64_t>(const DebugArg& a)    { return static_cast<uint64_t>(a.i); }
template <> inline DebugVec3   extractArg<DebugVec3>(const DebugArg& a)   { return DebugVec3{ a.v[0], a.v[1], a.v[2] }; }
template <> inline DebugColor  extractArg<DebugColor>(const DebugArg& a)  { return DebugColor{ a.v[0], a.v[1], a.v[2], a.v[3] }; }

// The registry: a process-wide singleton holding all registered commands.
class DebugCommandRegistry {
public:
    static DebugCommandRegistry& instance();

    void addEntry(DebugCommandEntry entry);

    size_t                   count() const;
    const DebugCommandEntry* at(size_t index) const;
    const DebugCommandEntry* find(const std::string& name) const;

    // Execute by name with the provided args; returns a result/status string
    // (empty on success with no message). Logs a diagnostic when the command
    // is unknown or the arg count mismatches.
    std::string invoke(const std::string& name, const std::vector<DebugArg>& args) const;

private:
    std::vector<DebugCommandEntry> entries_;
};

// ── Registration helpers ──────────────────────────────────────────────────────
// A command bound to a std::function<void(Args...)>. Parameter metadata is
// supplied explicitly (names/ranges/enum labels) since C++ has no parameter
// names at runtime; the parameter *types* are derived from the signature and
// validated against the supplied metadata count.

namespace detail {

template <class... Args, size_t... I>
void callWithArgs(const std::function<void(Args...)>& fn,
                  const std::vector<DebugArg>& args,
                  std::index_sequence<I...>)
{
    fn(extractArg<Args>(args[I])...);
}

} // namespace detail

// Register a void(Args...) command. 'params' must have sizeof...(Args) entries;
// each entry's 'type' should match the corresponding Args type.
template <class... Args>
void registerCommand(const std::string& name,
                     const std::string& help,
                     std::vector<DebugParamMeta> params,
                     std::function<void(Args...)> fn)
{
    DebugCommandEntry entry;
    entry.meta.name   = name;
    entry.meta.help   = help;
    entry.meta.params = std::move(params);
    entry.invoke = [fn](const std::vector<DebugArg>& args) -> std::string {
        if (args.size() < sizeof...(Args)) return "error: not enough arguments";
        detail::callWithArgs<Args...>(fn, args, std::index_sequence_for<Args...>{});
        return {};
    };
    DebugCommandRegistry::instance().addEntry(std::move(entry));
}

} // namespace tinyengine::debugcmd
