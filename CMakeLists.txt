cmake_minimum_required(VERSION 3.20)
project(SignOnQt6 VERSION 8.61 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus Network Sql Xml)

set(SIGNON_PUBLIC_HEADERS
    lib/SignOn/sessiondata.h
    lib/SignOn/identityinfo.h
    lib/SignOn/authservice.h
    lib/SignOn/identity.h
    lib/SignOn/authsession.h
    lib/SignOn/libsignoncommon.h
    lib/SignOn/securitycontext.h
    lib/SignOn/signon.h
    lib/SignOn/signonerror.h
)
set(SIGNON_SOURCES
    lib/SignOn/identityinfo.cpp
    lib/SignOn/async-dbus-proxy.cpp
    lib/SignOn/authservice.cpp
    lib/SignOn/authserviceimpl.cpp
    lib/SignOn/authsession.cpp
    lib/SignOn/authsessionimpl.cpp
    lib/SignOn/connection-manager.cpp
    lib/SignOn/dbusinterface.cpp
    lib/SignOn/debug.cpp
    lib/SignOn/identity.cpp
    lib/SignOn/identityimpl.cpp
    lib/SignOn/identityinfoimpl.cpp
    lib/SignOn/securitycontext.cpp
)
add_library(signon-qt6 SHARED ${SIGNON_SOURCES} ${SIGNON_PUBLIC_HEADERS})
add_library(SignOnQt6::SignOnQt6 ALIAS signon-qt6)
set_target_properties(signon-qt6 PROPERTIES VERSION 0.4 SOVERSION 0 EXPORT_NAME SignOnQt6)
target_compile_definitions(signon-qt6 PRIVATE QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII)
target_include_directories(signon-qt6 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/SignOn>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/signon-qt6>)
target_link_libraries(signon-qt6 PUBLIC Qt6::Core Qt6::DBus)

install(TARGETS signon-qt6 EXPORT SignOnQt6Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${SIGNON_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/signon-qt6/SignOn)
install(FILES
    lib/SignOn/AuthService lib/SignOn/AuthSession lib/SignOn/Error
    lib/SignOn/Identity lib/SignOn/IdentityInfo lib/SignOn/SecurityContext
    lib/SignOn/SessionData
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/signon-qt6/SignOn)

configure_package_config_file(cmake/SignOnQt6Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SignOnQt6Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SignOnQt6)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/SignOnQt6ConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(EXPORT SignOnQt6Targets NAMESPACE SignOnQt6:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SignOnQt6)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SignOnQt6Config.cmake ${CMAKE_CURRENT_BINARY_DIR}/SignOnQt6ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SignOnQt6)

set(PC_PREFIX ${CMAKE_INSTALL_PREFIX})
set(PC_LIBDIR ${CMAKE_INSTALL_FULL_LIBDIR})
configure_file(cmake/libsignon-qt6.pc.in libsignon-qt6.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libsignon-qt6.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

option(BUILD_SIGNOND "Build the signond daemon and runtime plugins" ON)
if(BUILD_SIGNOND)
    add_subdirectory(cmake/signond)
endif()
