file(REMOVE_RECURSE
  "libwubuconv.a"
  "libwubuconv.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/wubuconv.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
