cmake_minimum_required(VERSION 3.22)

project(device-battery VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.6.0)
set(KF_MIN_VERSION 6.0.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PLASMOID_ID "io.github.giovannilentini.devicebattery")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQmlModule)
include(ECMFindQmlModule)
include(FeatureSummary)
include(CheckIncludeFiles)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Qml)

# Only the BlueZ *headers* are needed (sockaddr_l2 for the L2CAP socket), no library.
# l2cap.h is not self-contained: it needs bluetooth.h first.
check_include_files("bluetooth/bluetooth.h;bluetooth/l2cap.h" HAVE_BLUEZ_HEADERS LANGUAGE CXX)
if(NOT HAVE_BLUEZ_HEADERS)
    message(FATAL_ERROR "BlueZ headers not found. Install bluez-libs-devel (Fedora) or libbluetooth-dev (Debian/Ubuntu).")
endif()

# QML modules the widget imports. They are only needed at runtime, so a missing one does
# not break the build but leaves an empty widget; report them in the feature summary.
if(NOT DEFINED QT_MAJOR_VERSION)
    set(QT_MAJOR_VERSION 6)
endif()
foreach(module
        QtQuick.Shapes
        org.kde.kcmutils
        org.kde.kirigami
        org.kde.plasma.components
        org.kde.plasma.configuration
        org.kde.plasma.core
        org.kde.plasma.extras
        org.kde.plasma.plasmoid)
    ecm_find_qmlmodule(${module})
endforeach()

add_subdirectory(src)

# The plasmoid itself is plain QML + metadata, installed as a KPackage.
install(DIRECTORY package/ DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids/${PLASMOID_ID})

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
