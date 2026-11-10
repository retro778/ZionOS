add_executable(zionos-settings main.cpp settingswindow.cpp settingswindow.h)
target_link_libraries(zionos-settings PRIVATE zionui Qt6::Network)
install(TARGETS zionos-settings RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
