################ Targets ################

if(NOT APPLE)
    install(TARGETS starviewer starviewer_crashreporter qtsingleapplication RUNTIME DESTINATION .)
    if(NOT STARVIEWER_LITE)
        install(TARGETS starviewer_sapwrapper RUNTIME DESTINATION .)
    endif()
endif()


################ Other files and directories ################

install(DIRECTORY dicomdumpdefaulttags DESTINATION .)
install(DIRECTORY hangingprotocols/Default/ DESTINATION hangingprotocols)
if(USE_SPECIFIC_HANGING_PROTOCOLS)
    install(DIRECTORY ${DEPLOYMENT_REPO_DIR}/hangingprotocols/IDI/ DESTINATION hangingprotocols/Specific)
endif()
install(DIRECTORY releasenotes DESTINATION . FILES_MATCHING PATTERN "*.html" PATTERN "*.css" PATTERN "*.svg")
install(FILES bin/log.conf bin/Part3.xml DESTINATION .)
get_property(PDF_MANUALS GLOBAL PROPERTY PDF_MANUALS)
install(FILES ${PDF_MANUALS} DESTINATION .)

if(WIN32)
    install(PROGRAMS bin/mkisofs.exe DESTINATION .)
elseif(UNIX AND NOT APPLE)
    install(PROGRAMS bin/starviewer.sh DESTINATION .)
    install(DIRECTORY src/main/themes/${THEME}/images/logo DESTINATION .)
    install(FILES src/main/themes/${THEME}/images/icons/mime-pdf.svg DESTINATION .)
endif()


################ Libraries ################

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
if(MSVC AND BUILD_TYPE STREQUAL "debug" AND NOT STARVIEWER_BUILD_DEBUG_WITH_RELEASE_LIBS)
    set(DEBUG_LIBRARY_SUFFIX d)
else()
    set(DEBUG_LIBRARY_SUFFIX "")
endif()

set(PLUGINS
    iconengines/qsvgicon
    imageformats/qgif
    imageformats/qsvg
    sqldrivers/qsqlite
)
if(WIN32)
    list(APPEND PLUGINS
        platforms/qwindows
        styles/qwindowsvistastyle
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND PLUGINS
        platforms/qxcb
    )
    # Integration plugins (Wayland, GTK theme, input methods) that a Qt
    # distribution may not ship: current qtwayland no longer provides the
    # wayland-graphics-integration plugins, for example. They are bundled
    # when present and skipped otherwise.
    set(OPTIONAL_PLUGINS
        platforminputcontexts/composeplatforminputcontextplugin
        platforminputcontexts/ibusplatforminputcontextplugin
        platforms/qwayland-egl
        platforms/qwayland-generic
        platforms/qwayland-xcomposite-egl
        platforms/qwayland-xcomposite-glx
        platformthemes/qgtk3
        wayland-decoration-client/bradient
        wayland-graphics-integration-client/dmabuf-server
        wayland-graphics-integration-client/drm-egl-server
        wayland-graphics-integration-client/qt-plugin-wayland-egl
        wayland-graphics-integration-client/shm-emulation-server
        wayland-graphics-integration-client/xcomposite-egl
        wayland-graphics-integration-client/xcomposite-glx
        wayland-graphics-integration-server/dmabuf-server
        wayland-graphics-integration-server/drm-egl-server
        wayland-graphics-integration-server/qt-plugin-wayland-egl
        wayland-graphics-integration-server/shm-emulation-server
        wayland-graphics-integration-server/wayland-eglstream-controller
        wayland-graphics-integration-server/xcomposite-egl
        wayland-graphics-integration-server/xcomposite-glx
        wayland-shell-integration/ivi-shell
        wayland-shell-integration/wl-shell
        wayland-shell-integration/xdg-shell
        wayland-shell-integration/xdg-shell-v5
        wayland-shell-integration/xdg-shell-v6
        xcbglintegrations/qxcb-egl-integration
        xcbglintegrations/qxcb-glx-integration
    )
endif()
# QT_DIR is either a Qt installation prefix (Windows, a custom SDK
# installation) or a system prefix such as /usr, where Debian-based
# distributions put Qt under the multiarch library directory. Detect which
# layout is in use, so the same install rules work for both.
if(WIN32)
    set(QT_LIB_DIR ${QT_DIR}/bin)
    set(QT_PLUGINS_DIR ${QT_DIR}/plugins)
    set(QT_QML_DIR ${QT_DIR}/qml)
    set(QT_TRANSLATIONS_DIR ${QT_DIR}/translations)
else()
    if(EXISTS "${QT_DIR}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/qt5/plugins")
        set(QT_LIB_DIR ${QT_DIR}/lib/${CMAKE_LIBRARY_ARCHITECTURE})
        set(QT_PLUGINS_DIR ${QT_DIR}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/qt5/plugins)
        set(QT_QML_DIR ${QT_DIR}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/qt5/qml)
        set(QT_TRANSLATIONS_DIR ${QT_DIR}/share/qt5/translations)
    else()
        set(QT_LIB_DIR ${QT_DIR}/lib)
        set(QT_PLUGINS_DIR ${QT_DIR}/plugins)
        set(QT_QML_DIR ${QT_DIR}/qml)
        set(QT_TRANSLATIONS_DIR ${QT_DIR}/translations)
    endif()
endif()

foreach(PLUGIN ${PLUGINS})
    # TODO use cmake_path from CMake 3.20
    string(REGEX MATCH "([^/]+)/([^/]+)" _ ${PLUGIN})
    set(FILENAME ${CMAKE_SHARED_LIBRARY_PREFIX}${CMAKE_MATCH_2}${DEBUG_LIBRARY_SUFFIX}${CMAKE_SHARED_LIBRARY_SUFFIX})
    install(PROGRAMS "${QT_PLUGINS_DIR}/${CMAKE_MATCH_1}/${FILENAME}" DESTINATION "plugins/${CMAKE_MATCH_1}")
endforeach()

foreach(PLUGIN ${OPTIONAL_PLUGINS})
    string(REGEX MATCH "([^/]+)/([^/]+)" _ ${PLUGIN})
    set(FILENAME ${CMAKE_SHARED_LIBRARY_PREFIX}${CMAKE_MATCH_2}${DEBUG_LIBRARY_SUFFIX}${CMAKE_SHARED_LIBRARY_SUFFIX})
    if(EXISTS "${QT_PLUGINS_DIR}/${CMAKE_MATCH_1}/${FILENAME}")
        install(PROGRAMS "${QT_PLUGINS_DIR}/${CMAKE_MATCH_1}/${FILENAME}" DESTINATION "plugins/${CMAKE_MATCH_1}")
    endif()
endforeach()

set(QMLS
    plugins.qmltypes
    qmldir
    ${CMAKE_SHARED_LIBRARY_PREFIX}qtquick2plugin${DEBUG_LIBRARY_SUFFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}
)
foreach(QML ${QMLS})
    install(PROGRAMS "${QT_QML_DIR}/QtQuick.2/${QML}" DESTINATION qml/QtQuick.2)
endforeach()

# Qt only ships translations for the modules installed with it, so missing
# files are skipped instead of failing the package.
set(TRANSLATIONS
    qt_ca.qm
    qt_es.qm
    qtbase_ca.qm
    qtbase_es.qm
    qtmultimedia_ca.qm
    qtmultimedia_es.qm
    qtscript_ca.qm
    qtscript_es.qm
    qtxmlpatterns_ca.qm
    qtxmlpatterns_es.qm
)
foreach(TRANSLATION ${TRANSLATIONS})
    if(EXISTS "${QT_TRANSLATIONS_DIR}/${TRANSLATION}")
        install(FILES "${QT_TRANSLATIONS_DIR}/${TRANSLATION}" DESTINATION translations)
    endif()
endforeach()

# Qt
set(QT_LIBS
    Qt5Concurrent
    Qt5Core
    Qt5Gui
    Qt5Network
    Qt5OpenGL
    Qt5Qml
    Qt5Quick
    Qt5QuickWidgets
    Qt5Sql
    Qt5Svg
    Qt5Widgets
    Qt5Xml
    Qt5XmlPatterns
)
if(WIN32)
    list(APPEND QT_LIBS
        d3dcompiler_47
        libEGL
        libGLESv2
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND QT_LIBS
        Qt5DBus
        Qt5XcbQpa
        icudata
        icui18n
        icuuc
    )
endif()
foreach(LIB ${QT_LIBS})
    set(FILENAME ${CMAKE_SHARED_LIBRARY_PREFIX}${LIB}${DEBUG_LIBRARY_SUFFIX}${CMAKE_SHARED_LIBRARY_SUFFIX})
    if(WIN32)
        install(PROGRAMS "${QT_LIB_DIR}/${FILENAME}" DESTINATION .)
    else()
        # Every versioned file of the library, so that the rules do not
        # need updating when the Qt version changes. FILENAME already ends
        # in the shared library suffix; the versioned files are the
        # unversioned name plus the version. The unversioned files are
        # development symlinks and are not needed at runtime.
        file(GLOB QT_LIB_FILES "${QT_LIB_DIR}/${FILENAME}.*")
        if(NOT QT_LIB_FILES)
            message(FATAL_ERROR "No Qt runtime library files found for ${LIB} in ${QT_LIB_DIR}")
        endif()
        list(FILTER QT_LIB_FILES EXCLUDE REGEX "\\.py$")
        install(FILES ${QT_LIB_FILES} DESTINATION lib)
    endif()
endforeach()

# SDK
# DCMTK, VTK and ITK are built static and need no runtime files. ThreadWeaver
# comes from a system package on Linux (see CPACK_DEBIAN_PACKAGE_DEPENDS in
# Installers.cmake). The libraries the SDK builds shared (GDCM and vtkgdcm,
# plus ThreadWeaver on Windows) are bundled with globs, so that the rules do
# not need updating when a library version changes.
if(WIN32)
    # All shared libraries the SDK installs (GDCM, vtkgdcm, ThreadWeaver,
    # and OpenSSL if DCMTK was ever built with it). DCMTK's command line
    # tools are .exe files and are not bundled.
    file(GLOB SDK_RUNTIME_FILES "${SDK_INSTALL_DIR}/bin/*.dll")
    install(FILES ${SDK_RUNTIME_FILES} DESTINATION .)

    # QtNetwork uses OpenSSL for HTTPS (WADO). DCMTK is built without
    # OpenSSL on Windows, so the SDK install directory does not have the
    # DLLs, and Qt does not ship them either: the OpenSSL 1.1.1 tools
    # component has been removed from Qt's repository. The package is
    # built without them, which only means WADO over HTTPS does not work.
    set(OPENSSL_LIBS
        libcrypto-1_1-x64
        libssl-1_1-x64
    )
    foreach(LIB ${OPENSSL_LIBS})
        set(FILENAME ${LIB}${CMAKE_SHARED_LIBRARY_SUFFIX})
        if(EXISTS "${SDK_INSTALL_DIR}/bin/${FILENAME}")
            install(FILES "${SDK_INSTALL_DIR}/bin/${FILENAME}" DESTINATION .)
        elseif(EXISTS "${QT_LIB_DIR}/${FILENAME}")
            install(FILES "${QT_LIB_DIR}/${FILENAME}" DESTINATION .)
        else()
            message(WARNING "OpenSSL runtime library ${FILENAME} not found in ${SDK_INSTALL_DIR}/bin or ${QT_LIB_DIR}; the package will not support HTTPS (WADO)")
        endif()
    endforeach()
else()
    foreach(SDK_LIB_DIR ${SDK_INSTALL_DIR}/lib ${SDK_INSTALL_DIR}/lib64)
        if(EXISTS "${SDK_LIB_DIR}")
            file(GLOB SDK_LIB_FILES "${SDK_LIB_DIR}/*.so.*")
            if(SDK_LIB_FILES)
                list(FILTER SDK_LIB_FILES EXCLUDE REGEX "\\.py$|\\.debug$")
                install(FILES ${SDK_LIB_FILES} DESTINATION lib)
            endif()
        endif()
    endforeach()
endif()
