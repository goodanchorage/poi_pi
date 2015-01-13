##---------------------------------------------------------------------------
## Author:      nohal aka. Pavel Kalian
## Copyright:
## License:     wxWidgets License
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.2)

PROJECT(my_plugin_pi)

SET(PACKAGE_NAME my_plugin_pi)
SET(VERBOSE_NAME MyPlugin)
SET(TITLE_NAME MyPlugin)
SET(CPACK_PACKAGE_CONTACT "Artiom Viter")

SET(VERSION_MAJOR "0")
SET(VERSION_MINOR "1")

#SET(CMAKE_BUILD_TYPE Debug)

INCLUDE("cmake/PluginConfigure.cmake")

SET(SRC_MYPLUGIN
      src/my_plugin_pi.h
      src/my_plugin_pi.cpp
      src/icons.h
      src/icons.cpp
	  src/wx/json_defs.h
	  src/wx/jsonreader.h
	  src/wx/jsonval.h
	  src/wx/jsonwriter.h
	  src/wxJSON/jsonreader.cpp
	  src/wxJSON/jsonval.cpp
	  src/wxJSON/jsonwriter.cpp
      
    )
ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_MYPLUGIN})

INCLUDE("cmake/PluginInstall.cmake")

