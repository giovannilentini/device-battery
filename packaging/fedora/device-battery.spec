Name:           device-battery
Version:        1.1.0
Release:        1%{?dist}
Summary:        Plasma 6 widget showing the battery of AirPods and other wireless devices

License:        MIT
URL:            https://github.com/giovannilentini/device-battery
Source0:        %{url}/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  bluez-libs-devel
# For the unit tests of the UPower source, which run against a fake UPower on a private bus.
BuildRequires:  dbus-daemon

# The QML modules the widget imports (the same list as ecm_find_qmlmodule in CMakeLists.txt).
Requires:       qt6qml(QtQuick.Shapes)
Requires:       qt6qml(org.kde.kcmutils)
Requires:       qt6qml(org.kde.kirigami)
Requires:       qt6qml(org.kde.plasma.components)
Requires:       qt6qml(org.kde.plasma.configuration)
Requires:       qt6qml(org.kde.plasma.core)
Requires:       qt6qml(org.kde.plasma.extras)
Requires:       qt6qml(org.kde.plasma.plasmoid)
Requires:       upower
Recommends:     bluez

%description
A KDE Plasma 6 widget that shows the battery level of wireless devices, like
Apple's Batteries widget: AirPods and Beats (left and right earbud and the case,
read over Apple's own protocol), headphones, keyboards, mice and game
controllers. It runs in the panel, the system tray or on the desktop.

%prep
%autosetup -p1

%build
%cmake -DBUILD_TESTING=ON
%cmake_build

%install
%cmake_install

%check
%ctest

%files
%license LICENSE
%doc README.md
%{_qt6_qmldir}/io/github/giovannilentini/devicebattery/
%{_datadir}/plasma/plasmoids/io.github.giovannilentini.devicebattery/

%changelog
* Sat Nov 07 2026 giovannilentini <giovannilentini29@gmail.com> - 1.1.0-1
- AirPods: in-ear detection, noise control mode, model name, low battery notifications
- Recently disconnected devices, hide and rename devices, desktop grid
- Connected devices without battery information are named in the popup
