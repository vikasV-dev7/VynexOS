# Integrity checks for vendored third-party files. Included from the top-level CMakeLists.txt so that a modified,
# re-encoded (e.g. newline-converted) or subset file stops the build instead of silently changing behaviour or
# violating the font licence's "no modified Reserved Font Name" rule.
include("${CMAKE_CURRENT_LIST_DIR}/VynexVerifyFile.cmake")

set(_vynex_root "${CMAKE_CURRENT_LIST_DIR}/..")

vynex_verify_sha256("${_vynex_root}/third_party/stb/stb_truetype.h"
    "ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab" "stb_truetype.h v1.26")
vynex_verify_sha256("${_vynex_root}/third_party/stb/LICENSE"
    "bebfe904b14301657e4e5d655c811d51fd31b97c455b9cc2d8600d6bac6cff63" "stb LICENSE")

vynex_verify_sha256("${_vynex_root}/assets/fonts/inter/Inter-Regular.ttf"
    "40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82" "Inter 4.1 Regular")
vynex_verify_sha256("${_vynex_root}/assets/fonts/inter/Inter-Medium.ttf"
    "97ad806f526e41546d46365bb3a393145f75b7b1568913db74549ad8b8dba872" "Inter 4.1 Medium")
vynex_verify_sha256("${_vynex_root}/assets/fonts/inter/Inter-SemiBold.ttf"
    "78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3" "Inter 4.1 SemiBold")
vynex_verify_sha256("${_vynex_root}/assets/fonts/inter/LICENSE.txt"
    "262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a" "Inter OFL licence text")

unset(_vynex_root)
