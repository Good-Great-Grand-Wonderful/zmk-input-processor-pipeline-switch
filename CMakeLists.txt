if(CONFIG_ZMK_INPUT_PROCESSOR_PIPELINE_SWITCH)
  zephyr_library()
  zephyr_library_include_directories(include)
  zephyr_library_sources(src/input_processor_pipeline_switch.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_PIPELINE_SWITCH src/behavior_pipeline_switch.c)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
  # Export this module's header (zip_pipeline_switch.h) so other modules (the
  # trackball config service) can introspect/drive the switch.
  zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
