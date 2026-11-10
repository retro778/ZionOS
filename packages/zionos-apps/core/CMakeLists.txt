add_library(zioncore STATIC
  sysinfo.cpp sysinfo.h
  i18n.cpp i18n.h
  redact.cpp redact.h
  appsettings.cpp appsettings.h
  desktopconfig.cpp desktopconfig.h
  ai/types.h ai/types.cpp
  ai/sseparser.cpp ai/sseparser.h
  ai/provider.cpp ai/provider.h
  ai/anthropicprovider.cpp ai/anthropicprovider.h
  ai/openaiprovider.cpp ai/openaiprovider.h
  ai/credentialstore.cpp ai/credentialstore.h
  ai/history.cpp ai/history.h
  ai/markdown.cpp ai/markdown.h
  ai/attachments.cpp ai/attachments.h
  ai/chatcontroller.cpp ai/chatcontroller.h
)
target_include_directories(zioncore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(zioncore PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::DBus
                               PRIVATE Qt6Keychain::Qt6Keychain)
