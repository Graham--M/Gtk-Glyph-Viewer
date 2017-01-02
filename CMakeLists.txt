#=============================================================================
# Font Rendering Viewer
#

cmake_minimum_required (VERSION 3.4)


#-----------------------------------------------------------------------------
# Font rendering viewer - main project
#
project (gtkglyphviewer)

set(VIEWER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

set (VIEWER_SOURCES
  ${VIEWER_SOURCE_DIR}/main.c
  ${VIEWER_SOURCE_DIR}/glyphblending.c
  ${VIEWER_SOURCE_DIR}/utils.c
  ${VIEWER_SOURCE_DIR}/outlineprocessing.c
  ${VIEWER_SOURCE_DIR}/controls.c
  ${VIEWER_SOURCE_DIR}/interface.glade.c
  ${VIEWER_SOURCE_DIR}/dialog_gotoindex.c
  ${VIEWER_SOURCE_DIR}/dialog_gotochar.c
  ${VIEWER_SOURCE_DIR}/dialog_selectface.c
)

add_executable (gtkglyphviewer WIN32 ${VIEWER_SOURCES})


#--------------------------------------
# PKGCONFIG STUFF

find_package(PkgConfig REQUIRED)

#Link GTK
pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
include_directories(${GTK2_INCLUDE_DIRS})
link_directories(${GTK2_LIBRARY_DIRS})
add_definitions(${GTK2_CFLAGS_OTHER})
target_link_libraries(gtkglyphviewer ${GTK2_LIBRARIES})

pkg_check_modules(FREETYPE REQUIRED freetype2)
# These are redundant as the GTK PKGCONFIG picks them up.
#include_directories(${FREETYPE_INCLUDE_DIRS})
#link_directories(${FREETYPE_LIBRARY_DIRS})
#add_definitions(${FREETYPE_CFLAGS_OTHER})
target_link_libraries(gtkglyphviewer ${FREETYPE_LIBRARIES})