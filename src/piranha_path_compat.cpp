// Piranha's public Path API exposes Boost only as an implementation detail.
// Build a compatible local implementation over C++17 std::filesystem so the
// script compiler does not impose Boost on every desktop platform.
#include "../dependencies/submodules/piranha/include/path.h"
#include "../dependencies/submodules/piranha/include/memory_tracker.h"

#include <filesystem>

namespace boost::filesystem {

class path {
public:
    path() = default;
    path(const std::string &value) : m_path(value) { }
    explicit path(const std::filesystem::path &value) : m_path(value) { }
    std::string string() const { return m_path.string(); }
    path parent_path() const { return path(m_path.parent_path()); }
    path extension() const { return path(m_path.extension()); }
    path stem() const { return path(m_path.stem()); }
    bool is_complete() const { return m_path.is_absolute(); }
    const std::filesystem::path &native() const { return m_path; }

private:
    std::filesystem::path m_path;
};

inline path operator/(const path &left, const path &right) {
    return path(left.native() / right.native());
}

inline bool equivalent(const path &left, const path &right) {
    std::error_code error;
    const bool result = std::filesystem::equivalent(left.native(), right.native(), error);
    return !error && result;
}

inline bool exists(const path &value) {
    std::error_code error;
    return std::filesystem::exists(value.native(), error) && !error;
}

} // namespace boost::filesystem

piranha::Path::Path(const std::string &value) : m_path(nullptr) { setPath(value); }
piranha::Path::Path(const char *value) : Path(std::string(value)) { }
piranha::Path::Path(const Path &other) : m_path(TRACK(new boost::filesystem::path)) { *m_path = other.getBoostPath(); }
piranha::Path::Path() : m_path(nullptr) { }
piranha::Path::Path(const boost::filesystem::path &value) : m_path(TRACK(new boost::filesystem::path(value))) { }
piranha::Path::~Path() { if (m_path != nullptr) delete FTRACK(m_path); }

std::string piranha::Path::toString() const { return m_path->string(); }
void piranha::Path::setPath(const std::string &value) {
    if (m_path != nullptr) delete FTRACK(m_path);
    m_path = TRACK(new boost::filesystem::path(value));
}
bool piranha::Path::operator==(const Path &other) const { return boost::filesystem::equivalent(getBoostPath(), other.getBoostPath()); }
piranha::Path piranha::Path::append(const Path &other) const { return Path(getBoostPath() / other.getBoostPath()); }
void piranha::Path::getParentPath(Path *output) const {
    if (output->m_path != nullptr) delete FTRACK(output->m_path);
    output->m_path = TRACK(new boost::filesystem::path(m_path->parent_path()));
}
const piranha::Path &piranha::Path::operator=(const Path &other) {
    if (m_path != nullptr) delete FTRACK(m_path);
    m_path = TRACK(new boost::filesystem::path(other.getBoostPath()));
    return *this;
}
std::string piranha::Path::getExtension() const { return m_path->extension().string(); }
std::string piranha::Path::getStem() const { return m_path->stem().string(); }
bool piranha::Path::isAbsolute() const { return m_path->is_complete(); }
bool piranha::Path::exists() const { return boost::filesystem::exists(*m_path); }
