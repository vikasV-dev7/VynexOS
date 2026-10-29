# Build-time helper that embeds binary assets (the UI fonts) into C++ sources.
add_executable(vynex_bin2c bin2c.cpp)
# It only runs at build time on trusted, in-repo inputs; the plain C stdio calls are intentional.
target_compile_definitions(vynex_bin2c PRIVATE _CRT_SECURE_NO_WARNINGS)
