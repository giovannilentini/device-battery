# Pure logic with no D-Bus or socket access, shared by the plugin and the unit tests.
add_library(devicebattery_core STATIC
    aapprotocol.cpp
    device.cpp
    logging.cpp
)
target_include_directories(devicebattery_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(devicebattery_core PUBLIC Qt6::Core)
set_target_properties(devicebattery_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
