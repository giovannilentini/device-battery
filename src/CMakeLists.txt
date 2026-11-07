# Pure logic with no D-Bus or socket access, shared by the plugin and the unit tests.
add_library(devicebattery_core STATIC
    aapprotocol.cpp
    device.cpp
    devicefilter.cpp
    devicelistmodel.cpp
    logging.cpp
    upowerdevice.cpp
)
target_include_directories(devicebattery_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(devicebattery_core PUBLIC Qt6::Core)
set_target_properties(devicebattery_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
# DeviceModel derives from DeviceListModel: let qmltyperegistrar describe the base too,
# so QML tooling (qmllint, the language server) knows its properties and roles.
qt_extract_metatypes(devicebattery_core)

# The QML module the plasmoid imports: io.github.giovannilentini.devicebattery
ecm_add_qml_module(devicebatteryplugin
    URI ${PLASMOID_ID}
    GENERATE_PLUGIN_SOURCE
)

# The device sources (D-Bus and the AAP socket). Separate from the plugin so tests can run
# them against fake services on a private bus.
add_library(devicebattery_sources STATIC
    aapsession.cpp
    airpodssource.cpp
    dbusutils.h
    devicesource.h
    upowersource.cpp
)
target_link_libraries(devicebattery_sources PUBLIC devicebattery_core Qt6::DBus)
set_target_properties(devicebattery_sources PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(devicebatteryplugin PRIVATE
    devicemodel.cpp
    qmltypes.h
)

target_link_libraries(devicebatteryplugin PRIVATE
    devicebattery_sources
    Qt6::Qml
)

ecm_finalize_qml_module(devicebatteryplugin DESTINATION ${KDE_INSTALL_QMLDIR})
