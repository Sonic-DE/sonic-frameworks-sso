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

set(plugins_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins/SignOn)
add_library(signon-plugins SHARED
    ${plugins_dir}/authpluginif.h
    ${plugins_dir}/uisessiondata.h
    ${plugins_dir}/uisessiondata_priv.h)
set_target_properties(signon-plugins PROPERTIES VERSION 1.0.0 SOVERSION 1)
target_include_directories(signon-plugins PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/signon-plugins>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/signon-plugins/SignOn>)
target_link_libraries(signon-plugins PUBLIC signon-qt6 Qt6::Core)

set(plugins_common_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins/signon-plugins-common/SignOn)
add_library(signon-plugins-common SHARED ${plugins_common_dir}/blobiohandler.cpp)
set_target_properties(signon-plugins-common PROPERTIES VERSION 0.4 SOVERSION 0)
target_include_directories(signon-plugins-common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins/signon-plugins-common)
target_compile_definitions(signon-plugins-common PRIVATE SIGNON_PLUGIN_TRACE)
target_link_libraries(signon-plugins-common PUBLIC signon-plugins Qt6::DBus)

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

set(remoteplugin_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../src/remotepluginprocess)
add_executable(signonpluginprocess
    ${remoteplugin_dir}/main.cpp
    ${remoteplugin_dir}/debug.cpp
    ${remoteplugin_dir}/remotepluginprocess.cpp)
target_include_directories(signonpluginprocess PRIVATE
    ${remoteplugin_dir}
    ${signond_dir}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/plugins/signon-plugins-common)
target_compile_definitions(signonpluginprocess PRIVATE
    DEBUG_ENABLED
    SIGNON_PLUGIN_TRACE
    SIGNOND_PLUGINS_DIR="${CMAKE_INSTALL_FULL_LIBDIR}/signond")
target_link_libraries(signonpluginprocess PRIVATE signon-plugins-common Qt6::Network)

install(TARGETS signond RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(TARGETS signonpluginprocess RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS signon-extension signon-plugins signon-plugins-common
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
    ${plugins_dir}/AuthPluginInterface ${plugins_dir}/authpluginif.h
    ${plugins_dir}/signonplugincommon.h
    ${plugins_dir}/UiSessionData ${plugins_dir}/uisessiondata.h
    ${plugins_dir}/uisessiondata_priv.h ${plugins_common_dir}/blobiohandler.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/signon-plugins/SignOn)
install(FILES
    ${extension_dir}/AbstractAccessControlManager ${extension_dir}/abstract-access-control-manager.h
    ${extension_dir}/AbstractCryptoManager ${extension_dir}/abstract-crypto-manager.h
    ${extension_dir}/AbstractKeyAuthorizer ${extension_dir}/abstract-key-authorizer.h
    ${extension_dir}/AbstractKeyManager ${extension_dir}/abstract-key-manager.h
    ${extension_dir}/AbstractSecretsStorage ${extension_dir}/abstract-secrets-storage.h
    ${extension_dir}/Debug ${extension_dir}/debug.h ${extension_dir}/export.h
    ${extension_dir}/ExtensionInterface ${extension_dir}/extension-interface.h
    ${extension_dir}/KeyHandler ${extension_dir}/key-handler.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/signon-extension/SignOn)
foreach(package signon-plugins signon-plugins-common SignOnExtension)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../${package}.pc.in ${package}.pc @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${package}.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endforeach()
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
