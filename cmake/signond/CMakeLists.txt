set(extension_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/signond/SignOn)
add_library(signon-extension SHARED
    ${extension_dir}/abstract-access-control-manager.cpp
    ${extension_dir}/abstract-crypto-manager.cpp
    ${extension_dir}/abstract-key-authorizer.cpp
    ${extension_dir}/abstract-key-manager.cpp
    ${extension_dir}/abstract-secrets-storage.cpp
    ${extension_dir}/debug.cpp
    ${extension_dir}/extension-interface.cpp
    ${extension_dir}/key-handler.cpp
    ${extension_dir}/misc.cpp
)
set_target_properties(signon-extension PROPERTIES VERSION 0.4 SOVERSION 0)
target_compile_definitions(signon-extension PRIVATE BUILDING_SIGNON SIGNON_ENABLE_UNSTABLE_APIS SIGNON_TRACE QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII)
target_include_directories(signon-extension PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/signond ${extension_dir})
target_link_libraries(signon-extension PUBLIC Qt6::Core Qt6::DBus)

set(plugins_common_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins/signon-plugins-common/SignOn)
add_library(signon-plugins-common SHARED ${plugins_common_dir}/blobiohandler.cpp)
set_target_properties(signon-plugins-common PROPERTIES VERSION 0.4 SOVERSION 0)
target_include_directories(signon-plugins-common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins/signon-plugins-common)
target_compile_definitions(signon-plugins-common PRIVATE SIGNON_PLUGIN_TRACE)
target_link_libraries(signon-plugins-common PUBLIC Qt6::Core Qt6::DBus)

set(signond_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../src/signond)
set(signond_sources
    ${signond_dir}/accesscontrolmanagerhelper.cpp
    ${signond_dir}/credentialsaccessmanager.cpp
    ${signond_dir}/credentialsdb.cpp
    ${signond_dir}/default-crypto-manager.cpp
    ${signond_dir}/default-key-authorizer.cpp
    ${signond_dir}/default-secrets-storage.cpp
    ${signond_dir}/erroradaptor.cpp
    ${signond_dir}/signonsessioncore.cpp
    ${signond_dir}/signonauthsessionadaptor.cpp
    ${signond_dir}/signonauthsession.cpp
    ${signond_dir}/signonidentity.cpp
    ${signond_dir}/signondaemonadaptor.cpp
    ${signond_dir}/signondisposable.cpp
    ${signond_dir}/signonui_interface.cpp
    ${signond_dir}/pluginproxy.cpp
    ${signond_dir}/main.cpp
    ${signond_dir}/signondaemon.cpp
    ${signond_dir}/signonidentityinfo.cpp
    ${signond_dir}/signonidentityadaptor.cpp
    ${signond_dir}/signonsecuritycontext.cpp
    ${signond_dir}/signonsessioncoretools.cpp
)
add_executable(signond ${signond_sources})
target_compile_definitions(signond PRIVATE
    QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII SIGNOND_TRACE
    SIGNOND_PLUGINS_DIR="${CMAKE_INSTALL_FULL_LIBDIR}/signond"
    SIGNOND_EXTENSIONS_DIR="${CMAKE_INSTALL_FULL_LIBDIR}/signond/extensions")
target_include_directories(signond PRIVATE
    ${signond_dir}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/signond
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins/signon-plugins-common)
target_link_libraries(signond PRIVATE
    Qt6::Core Qt6::DBus Qt6::Network Qt6::Sql Qt6::Xml
    signon-extension signon-plugins-common rt)

add_library(passwordplugin MODULE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/plugins/password/passwordplugin.cpp)
target_include_directories(passwordplugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins/signon-plugins-common)
target_link_libraries(passwordplugin PRIVATE Qt6::Core signon-plugins-common)
target_compile_definitions(passwordplugin PRIVATE SIGNON_PLUGIN_TRACE)
set_target_properties(passwordplugin PROPERTIES PREFIX "")

install(TARGETS signond RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(TARGETS signon-extension signon-plugins-common
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS passwordplugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/signond)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/signond/interfaces/com.google.code.AccountsSSO.SingleSignOn.AuthService.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/signond/interfaces/com.google.code.AccountsSSO.SingleSignOn.AuthSession.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/signond/interfaces/com.google.code.AccountsSSO.SingleSignOn.Identity.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/com.google.code.AccountsSSO.SingleSignOn.service
    CONTENT "[D-BUS Service]\nName=com.google.code.AccountsSSO.SingleSignOn\nExec=${CMAKE_INSTALL_FULL_LIBEXECDIR}/signond\n")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.google.code.AccountsSSO.SingleSignOn.service
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
install(FILES ${signond_dir}/signond.conf DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})
