# wubua11y: dependency-free C11 accessibility auditor.
# Audits a wubumodel_doc and extracted EPUB parts for WCAG/EPUB-a11y gaps.
add_library(wubua11y STATIC a11y.c)
target_include_directories(wubua11y PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_link_libraries(wubua11y PUBLIC wubumodel)
