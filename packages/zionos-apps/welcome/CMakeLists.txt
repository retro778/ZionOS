add_executable(zionos-welcome main.cpp welcomewindow.cpp welcomewindow.h)
target_link_libraries(zionos-welcome PRIVATE zionui)
install(TARGETS zionos-welcome RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
