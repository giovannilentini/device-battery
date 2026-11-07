include(ECMAddTests)

ecm_add_tests(
    aapprotocoltest.cpp
    devicefiltertest.cpp
    devicelistmodeltest.cpp
    devicetest.cpp
    upowerdevicetest.cpp
    LINK_LIBRARIES devicebattery_core Qt6::Test
)

# The D-Bus sources run against fake services on a private bus, never the real ones.
find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
if(DBUS_RUN_SESSION_EXECUTABLE)
    add_executable(upowersourcetest upowersourcetest.cpp)
    target_link_libraries(upowersourcetest PRIVATE devicebattery_sources Qt6::Test)
    add_test(NAME upowersourcetest COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:upowersourcetest>)
    set_tests_properties(upowersourcetest PROPERTIES ENVIRONMENT "DEVICEBATTERY_PRIVATE_BUS=1;QT_FORCE_STDERR_LOGGING=1")
else()
    message(WARNING "dbus-run-session not found: the D-Bus source tests are not built")
endif()
