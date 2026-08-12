# CMake generated Testfile for 
# Source directory: /home/wubu/WuBuOffice/apps/wubuos
# Build directory: /home/wubu/WuBuOffice/build-plugin/apps/wubuos
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(view "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_view")
set_tests_properties(view PROPERTIES  LABELS "view;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;182;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(shell_ui "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_shell_ui")
set_tests_properties(shell_ui PROPERTIES  LABELS "view;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;189;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(plugin_abi "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_plugin_abi" "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/plugins/sample_plugin.so" "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/plugins/badabi_plugin.so")
set_tests_properties(plugin_abi PROPERTIES  LABELS "plugin;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;213;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(codefold "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_codefold")
set_tests_properties(codefold PROPERTIES  LABELS "codefold;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;228;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(bkmk "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_bkmk")
set_tests_properties(bkmk PROPERTIES  LABELS "bkmk;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;235;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(autocomp "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_autocomp")
set_tests_properties(autocomp PROPERTIES  LABELS "autocomp;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;248;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(findbar "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_findbar")
set_tests_properties(findbar PROPERTIES  LABELS "findbar;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;261;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(doccmd "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_doccmd")
set_tests_properties(doccmd PROPERTIES  LABELS "doccmd;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;321;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(macro "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_macro")
set_tests_properties(macro PROPERTIES  LABELS "macro;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;328;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(gotoline "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_gotoline")
set_tests_properties(gotoline PROPERTIES  LABELS "gotoline;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;335;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(dialog "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_dialog")
set_tests_properties(dialog PROPERTIES  LABELS "dialog;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;341;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
add_test(font "/home/wubu/WuBuOffice/build-plugin/apps/wubuos/test_font")
set_tests_properties(font PROPERTIES  LABELS "font;apps" _BACKTRACE_TRIPLES "/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;350;add_test;/home/wubu/WuBuOffice/apps/wubuos/CMakeLists.txt;0;")
