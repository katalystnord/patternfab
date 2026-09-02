#pragma once

#include <string>

namespace patternfab {

// No Qt dependency by design -- this library must stay embeddable in a
// non-GUI context (see "Development path" in CLAUDE.md).
std::string coreVersion();

} // namespace patternfab
