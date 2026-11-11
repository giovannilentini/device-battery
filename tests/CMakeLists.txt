include(ECMAddTests)

ecm_add_tests(
    aapprotocoltest.cpp
    devicefiltertest.cpp
    devicelistmodeltest.cpp
    devicetest.cpp
    upowerdevicetest.cpp
    usbreceiverstest.cpp
    LINK_LIBRARIES devicebattery_core Qt6::Test
)

# The AAP session runs against a local socket pair instead of an accessory.
ecm_add_tests(
    aapsessiontest.cpp
    LINK_LIBRARIES devicebattery_sources Qt6::Test
)

# The D-Bus sources run against fake services on a private bus, never the real ones.
find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
if(DBUS_RUN_SESSION_EXECUTABLE)
    foreach(test upowersourcetest airpodssourcetest)
        add_executable(${test} ${test}.cpp)
        target_link_libraries(${test} PRIVATE devicebattery_sources Qt6::Test)
        add_test(NAME ${test} COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:${test}>)
        set_tests_properties(${test} PROPERTIES ENVIRONMENT "DEVICEBATTERY_PRIVATE_BUS=1;QT_FORCE_STDERR_LOGGING=1")
    endforeach()
else()
    message(WARNING "dbus-run-session not found: the D-Bus source tests are not built")
endif()
