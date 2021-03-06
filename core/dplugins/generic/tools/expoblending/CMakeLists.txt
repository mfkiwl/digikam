#
# Copyright (c) 2010-2019, Gilles Caulier, <caulier dot gilles at gmail dot com>
# Copyright (c) 2015,      Benjamin Girault, <benjamin dot girault at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(MacroDPlugins)

include_directories($<TARGET_PROPERTY:Qt5::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt5::Concurrent,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>

                    $<TARGET_PROPERTY:KF5::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF5::I18n,INTERFACE_INCLUDE_DIRECTORIES>
)

set(expoblendingplugin_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/manager/expoblendingmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/manager/expoblendingthread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/manager/enfusebinary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/expoblendingwizard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/expoblendingintropage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/expoblendingitemspage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/expoblendingpreprocesspage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/expoblendinglastpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blendingdlg/enfusesettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blendingdlg/bracketstack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blendingdlg/enfusestack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/blendingdlg/expoblendingdlg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/expoblendingplugin.cpp
)

DIGIKAM_ADD_GENERIC_PLUGIN(NAME    ExpoBlending
                                    SOURCES ${expoblendingplugin_SRCS}
)

add_subdirectory(icons)
