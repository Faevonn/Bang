// XDG data dir resolution. dataDirectory() is where library.sqlite3,
// tracks/, and artwork/ all live (see README "Library data").
#pragma once

#include <filesystem>

namespace bang {

std::filesystem::path dataDirectory();
std::filesystem::path temporaryDirectory();

} // namespace bang
