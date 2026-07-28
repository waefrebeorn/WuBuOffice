# CMake generated Testfile for 
# Source directory: /home/wubu/WuBuOffice/apps/wubuos
# Build directory: /home/wubu/WuBuOffice/build-asan2/apps/wubuos
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(view "/home/wubu/WuBuOffice/build-asan2/apps/wubuos/test_view")
set_tests_properties(view PROPERTIES  LABELS "view;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;105;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(shell_ui "/home/wubu/WuBuOffice/build-asan2/apps/wubuos/test_shell_ui")
set_tests_properties(shell_ui PROPERTIES  LABELS "view;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;112;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(plugin_abi "/home/wubu/WuBuOffice/build-asan2/apps/wubuos/test_plugin_abi" "/home/wubu/WuBuOffice/build-asan2/apps/wubuos/plugins/sample_plugin.so" "/home/wubu/WuBuOffice/build-asan2/apps/wubuos/plugins/badabi_plugin.so")
set_tests_properties(plugin_abi PROPERTIES  LABELS "plugin;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;136;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
