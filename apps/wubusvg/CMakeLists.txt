# wubusvg CLI: ingest an SVG and regurgitate it (byte-safe round-trip)
add_executable(wubusvg_cli main.c)
target_link_libraries(wubusvg_cli PRIVATE wubusvg)

# wubusvg_agent: NDJSON agent protocol (wubuOS "AGI GUI" surface)
add_executable(wubusvg_agent agent_main.c)
target_link_libraries(wubusvg_agent PRIVATE wubusvg)
