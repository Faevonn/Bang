#include "bang/Hash.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

bool check(std::string_view name, std::string_view actual, std::string_view expected) {
    if (actual != expected) {
        std::cerr << "FAIL " << name << ": expected " << expected << " got " << actual << '\n';
        return false;
    }
    std::cout << "PASS " << name << '\n';
    return true;
}

} // namespace

int main() {
    bool ok = true;

    // SHA-256 of the empty string is a well-known test vector.
    {
        bang::Sha256 hasher;
        ok &= check(
            "sha256_empty_string",
            hasher.finish(),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        );
    }

    // SHA-256 of "abc" is another standard NIST test vector.
    {
        bang::Sha256 hasher;
        hasher.update("abc", 3);
        ok &= check(
            "sha256_abc",
            hasher.finish(),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
