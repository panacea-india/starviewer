set(SOURCES
    srextensionmediator.cpp
    qsrextension.cpp
)

set(RESOURCES
    sr.qrc
)

add_library(sr ${SOURCES} ${RESOURCES})

add_extension(sr)


################ Internal library dependencies ################

target_link_libraries(sr core)


################ External library dependencies ################

target_link_qt(sr
    Core
    Gui
    Widgets
)
