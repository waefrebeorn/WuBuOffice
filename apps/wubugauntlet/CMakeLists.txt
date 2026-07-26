# wubugauntlet CLI: OCR robustness battery for the multi-font bank
add_executable(wubugauntlet_cli main.c)
target_link_libraries(wubugauntlet_cli PRIVATE wubuocr wubufont)
