# Test code may use plain string literals and Qt Test macros that trip the
# strict conversion warnings applied to the product code.
remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII)
add_compile_options(-Wno-conversion -Wno-sign-conversion)

function(zion_test name)
  add_executable(${name} ${name}.cpp ${ARGN})
  target_link_libraries(${name} PRIVATE zioncore Qt6::Test Qt6::Network Qt6::Gui)
  target_compile_definitions(${name} PRIVATE
      ZION_TEST_TS_DIR="${CMAKE_SOURCE_DIR}/translations"
      ZION_TEST_QM_DIR="${CMAKE_BINARY_DIR}")
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT
      "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-home/config;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-home/data;LC_ALL=C.UTF-8")
endfunction()

zion_test(tst_sse)
zion_test(tst_core)
zion_test(tst_providers mockserver.h)
zion_test(tst_chat mockserver.h)
zion_test(tst_i18n)
add_dependencies(tst_i18n zionos-apps_lrelease)

# Optional tests against the real provider APIs. They cost money and need
# keys, so they are never part of the default test run:
#   ZIONOS_LIVE_API_TESTS=1 ANTHROPIC_API_KEY=... OPENAI_API_KEY=... ctest -R live
add_executable(tst_live_api tst_live_api.cpp)
target_link_libraries(tst_live_api PRIVATE zioncore Qt6::Test Qt6::Network)
add_test(NAME live_api COMMAND tst_live_api)
set_tests_properties(live_api PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "live")
