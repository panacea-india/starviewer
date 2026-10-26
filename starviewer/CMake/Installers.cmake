set(CMAKE_INSTALL_UCRT_LIBRARIES ON)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)

include(InstallRequiredSystemLibraries)

# Common
set(CPACK_PACKAGE_VENDOR Starviewer)
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/installerresources\\\\package-icon.bmp")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/installerresources/CPackProjectConfigFile.cmake")
if(STARVIEWER_CE)
    configure_file(LICENSE-CE LICENSE.txt COPYONLY)
    set(STARVIEWER_VERSION ${STARVIEWER_VERSION}-CE)
else()
    configure_file(LICENSE LICENSE.txt COPYONLY)
endif()
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/LICENSE.txt")  # WiX requires a RTF or TXT

if(STARVIEWER_LITE)
    set(CPACK_PACKAGE_NAME starviewerlite)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Starviewer Lite")
    set(CPACK_PACKAGE_EXECUTABLES starviewer_lite "Starviewer Lite")
    set(CPACK_CREATE_DESKTOP_LINKS starviewer_lite)
else()
    set(CPACK_PACKAGE_NAME starviewer)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY Starviewer)
    set(CPACK_PACKAGE_EXECUTABLES starviewer Starviewer)
    set(CPACK_CREATE_DESKTOP_LINKS starviewer)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64|x86_64)
    set(STARVIEWER_ARCH amd64)
else()
    set(STARVIEWER_ARCH ${CMAKE_SYSTEM_PROCESSOR})
endif()

set(STARVIEWER_OS ${CPACK_SYSTEM_NAME})
if(WIN32)
    set(STARVIEWER_OS winnt)
elseif(LINUX)
    if(EXISTS /etc/os-release)
        file(STRINGS /etc/os-release LINUX_ID REGEX ^ID=)
        if(LINUX_ID)
            string(SUBSTRING ${LINUX_ID} 3 -1 LINUX_ID)
            set(STARVIEWER_OS ${LINUX_ID})
        endif()
    endif()
endif()

set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}~${STARVIEWER_VERSION}~${STARVIEWER_ARCH}-${STARVIEWER_OS})

# The PDF manuals are only installed when they are built, so the shortcuts
# to them are only created when they exist.
get_property(PDF_MANUALS GLOBAL PROPERTY PDF_MANUALS)
if(PDF_MANUALS)
    set(CPACK_INSTALL_IFU_DESKTOP ON)
else()
    set(CPACK_INSTALL_IFU_DESKTOP OFF)
endif()

# NSIS
set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/installerresources\\\\nsis-welcome.bmp")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_INSTALLED_ICON_NAME starviewer.exe)
set(CPACK_NSIS_EXECUTABLES_DIRECTORY .)
if(PDF_MANUALS)
    set(CPACK_NSIS_MENU_LINKS
        "[ca] User guide.pdf" "Manual d'usuari (ca)"
        "[en] User guide.pdf" "User manual (en)"
        "[es] User guide.pdf" "Manual de usuario (es)"
    )
endif()
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
    WriteRegStr HKCR 'starviewer' 'URL Protocol' ''
    WriteRegStr HKCR 'starviewer\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\starviewer.exe\\\" --url \\\"%1\\\"'
")

# WiX
if(STARVIEWER_LITE)
    set(CPACK_WIX_UPGRADE_GUID E34A4959-E0E4-4E4E-978A-E9D1EE764937)
else()
    set(CPACK_WIX_UPGRADE_GUID 86A85188-E570-467A-B5CB-2F7691311436)
endif()

set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/src/main/themes/${THEME}/images/logo/logo.ico")
set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/installerresources/wix-banner.bmp")
set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/installerresources/wix-dialog.bmp")
set(CPACK_WIX_PATCH_FILE "${CMAKE_SOURCE_DIR}/installerresources/wixpatch.xml")

# DEB
# System libraries used directly by Starviewer, by the bundled Qt, or by the
# bundled Qt plugins. Libraries they use in turn are pulled in by the
# package manager. The package bundles Qt, ICU and GDCM (see
# InstallTargets.cmake); DCMTK, VTK and ITK are static and ThreadWeaver comes
# from a system package. Built against Ubuntu 24.04; other distributions
# will need their own package names.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgcc-s1, libstdc++6,
    libegl1, libgl1,
    libx11-6, libx11-xcb1, libxext6, libxi6, libxcb1, libxcb-glx0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1,
    libxcb-randr0, libxcb-render0, libxcb-render-util0, libxcb-shape0, libxcb-shm0, libxcb-sync1,
    libxcb-xfixes0, libxcb-xinerama0, libxcb-xkb1, libxkbcommon0, libxkbcommon-x11-0,
    libwayland-client0, libwayland-cursor0, libwayland-egl1, libwayland-server0,
    libfontconfig1, libfreetype6, libharfbuzz0b, libmd4c0, libpng16-16,
    libglib2.0-0, libdbus-1-3, libdouble-conversion3, libpcre2-8-0, libb2-1, libzstd1, liblzma5,
    libgssapi-krb5-2, libssl3, zlib1g, libsqlite3-0,
    libgtk-3-0, libkf5threadweaver5, libxt6")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Starviewer Team <support@starviewer.udg.edu>")

include(CPack)
