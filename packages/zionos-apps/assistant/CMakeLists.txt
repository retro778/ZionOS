add_executable(zionos-assistant main.cpp mainwindow.cpp mainwindow.h
               messagewidget.cpp messagewidget.h errortext.cpp errortext.h)
target_link_libraries(zionos-assistant PRIVATE zionui Qt6::Network)
install(TARGETS zionos-assistant RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
