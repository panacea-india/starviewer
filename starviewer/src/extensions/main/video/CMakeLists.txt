set(SOURCES
    videoextensionmediator.cpp
    qvideoextension.cpp
)

set(RESOURCES
    video.qrc
)

add_library(video ${SOURCES} ${RESOURCES})

add_extension(video)


################ Internal library dependencies ################

target_link_libraries(video core)


################ External library dependencies ################

target_link_qt(video
    Core
    Gui
    Multimedia
    MultimediaWidgets
    Widgets
)
