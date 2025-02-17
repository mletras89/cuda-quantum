set(LIBRARY_NAME cudaq-mqss-mqp-setup)
message(STATUS "Building CUDAQ MQSS Setup Library.")

add_library(${LIBRARY_NAME} SHARED MQSS-MQP-Platform.cpp)

target_include_directories(${LIBRARY_NAME} PRIVATE .
    PUBLIC
       $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/runtime>
       $<INSTALL_INTERFACE:include>)

target_link_libraries(${LIBRARY_NAME} PRIVATE
                      fmt::fmt-header-only
                      cudaq)

install(TARGETS ${LIBRARY_NAME} DESTINATION lib)
