project(GLCAP)
cmake_minimum_required(VERSION 2.8)
include_directories(/usr/include/libavcodec /usr/include/libavformat /usr/include/libswscale )
link_directories( /usr/local/lib )
add_library(${PROJECT_NAME} SHARED mediarecorder.cpp main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fpermissive -fPIC -D__STDC_CONSTANT_MACROS"  )
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-fPIC -ldl" )
target_link_libraries(${PROJECT_NAME} avcodec avformat swscale pulse pulse-simple)
