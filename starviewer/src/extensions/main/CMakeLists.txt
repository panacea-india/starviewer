add_subdirectory(q2dviewer)

if(NOT STARVIEWER_LITE)
    add_subdirectory(q3dviewer)
    add_subdirectory(mpr2d)
    add_subdirectory(dicomprint)
    add_subdirectory(pdf)
    add_subdirectory(sr)

    # Video is played by Qt Multimedia. Where it is not installed, the rest is built without the video extension.
    find_package(Qt5 COMPONENTS Multimedia MultimediaWidgets QUIET)
    if(Qt5Multimedia_FOUND AND Qt5MultimediaWidgets_FOUND)
        add_subdirectory(video)
    else()
        message(STATUS "Qt Multimedia was not found: the video extension is not built")
    endif()
endif()
