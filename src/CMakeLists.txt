# Pure logic with no D-Bus or socket access, shared by the plugin and the unit tests.
add_library(devicebattery_core STATIC
    aapprotocol.cpp
    device.cpp
    logging.cpp
    upowerdevice.cpp
)
target_include_directories(devicebattery_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(devicebattery_core PUBLIC Qt6::Core)
set_target_properties(devicebattery_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The QML module the plasmoid imports: io.github.giovannilentini.devicebattery
ecm_add_qml_module(devicebatteryplugin
    URI ${PLASMOID_ID}
    GENERATE_PLUGIN_SOURCE
)

target_sources(devicebatteryplugin PRIVATE
    aapsession.cpp
    airpodssource.cpp
    dbusutils.h
    devicemodel.cpp
    devicesource.h
    upowersource.cpp
)

target_link_libraries(devicebatteryplugin PRIVATE
    devicebattery_core
    Qt6::DBus
    Qt6::Qml
)

ecm_finalize_qml_module(devicebatteryplugin DESTINATION ${KDE_INSTALL_QMLDIR})
