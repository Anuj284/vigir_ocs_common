cmake_minimum_required(VERSION 2.8.3)
project(vigir_ocs_ortho_view)

## Find catkin and any catkin packages on which
## this package depends at build time
find_package(catkin REQUIRED COMPONENTS roscpp rospy std_msgs rviz vigir_perception_msgs vigir_ocs_base_3d_view)

## Declare things to be passed to other projects
catkin_package(	
	INCLUDE_DIRS src
	LIBRARIES ${PROJECT_NAME} 
	CATKIN_DEPENDS roscpp rospy std_msgs rviz vigir_perception_msgs vigir_ocs_base_3d_view
)

## This plugin includes Qt widgets, so we must include Qt like so:
find_package(Qt4 COMPONENTS QtCore QtGui REQUIRED)
include(${QT_USE_FILE})

## I prefer the Qt signals and slots to avoid defining "emit", "slots",
## etc because they can conflict with boost signals, so define QT_NO_KEYWORDS here.
add_definitions(-DQT_NO_KEYWORDS)

## Here we specify which header files need to be run through "moc",
## Qt's meta-object compiler.
qt4_wrap_cpp(MOC_FILES_WIDGET
  src/ortho_view.h
)

qt4_wrap_cpp(MOC_FILES
  src/ortho_view.h
  src/ui/ortho_view_widget.h
)

## Here we add QtCreator ui files
qt4_wrap_ui(UIC_FILES
  src/ui/ortho_view_widget.ui
)

## Here we specify the list of source files, including the output of
## the previous command which is stored in ``${MOC_FILES}``.
set(SOURCE_FILES_WIDGET
  src/ortho_view.cpp
  src/main_widget.cpp
  ${MOC_FILES_WIDGET}
)

set(SOURCE_FILES
  src/ortho_view.cpp
  src/ui/ortho_view_widget.cpp
  src/main.cpp
  ${MOC_FILES}
)

include_directories(include ${catkin_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} src)

add_library(${PROJECT_NAME} ${UIC_FILES} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES} ${QT_LIBRARIES})
add_dependencies(${PROJECT_NAME} ${catkin_EXPORTED_TARGETS})

add_executable(ortho_view ${UIC_FILES} ${SOURCE_FILES})
target_link_libraries(ortho_view ${catkin_LIBRARIES} ${QT_LIBRARIES})
add_dependencies(ortho_view ${catkin_EXPORTED_TARGETS})

add_executable(ortho_view_widget ${SOURCE_FILES_WIDGET})
target_link_libraries(ortho_view_widget ${PROJECT_NAME} ${catkin_LIBRARIES} ${QT_LIBRARIES})
add_dependencies(ortho_view_widget ${catkin_EXPORTED_TARGETS})

install( TARGETS ${PROJECT_NAME} ortho_view ortho_view_widget
   LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
   RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)
