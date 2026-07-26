# wubuocr CLI: image -> ocr_page / document JSON
add_executable(wubuocr_cli main.c)
target_link_libraries(wubuocr_cli PRIVATE wubuocr)
