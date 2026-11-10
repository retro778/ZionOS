add_library(zionui STATIC
  apphelpers.cpp apphelpers.h
  safetextbrowser.cpp safetextbrowser.h
  aisettingswidget.cpp aisettingswidget.h
  aboutwidget.cpp aboutwidget.h
)
target_link_libraries(zionui PUBLIC zioncore Qt6::Widgets)
