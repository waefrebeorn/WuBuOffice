# wubuspell: dependency-free C11 spell checker (hash-set dict + Levenshtein
# suggestions + UTF-8 word scanner). No third-party (Hunspell/Nuspell are C++).
add_library(wubuspell STATIC spell.c spell_seed.c)
target_include_directories(wubuspell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
