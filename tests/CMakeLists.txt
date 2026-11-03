include(ECMAddTests)

ecm_add_tests(
    aapprotocoltest.cpp
    devicetest.cpp
    upowerdevicetest.cpp
    LINK_LIBRARIES devicebattery_core Qt6::Test
)
