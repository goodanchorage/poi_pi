##---------------------------------------------------------------------------
## Author:      nohal aka. Pavel Kalian
## Copyright:
## License:     wxWidgets License
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.2)

PROJECT(goodanchorage_pi)

SET(PACKAGE_NAME goodanchorage_pi)
SET(VERBOSE_NAME GoodAnchoragePlugin)
SET(TITLE_NAME GoodAnchoragePlugin)
SET(CPACK_PACKAGE_CONTACT "Artiom Viter")

SET(VERSION_MAJOR "0")
SET(VERSION_MINOR "1")

#SET(CMAKE_BUILD_TYPE Debug)

INCLUDE("cmake/PluginConfigure.cmake")

SET(SRC_GOODANCHORAGEPLUGIN
      src/goodanchorage_pi.h
      src/goodanchorage_pi.cpp
      src/icons.h
      src/icons.cpp
      src/sqlite/sqlite3.h
	  src/wx/json_defs.h
	  src/wx/jsonreader.h
	  src/wx/jsonval.h
	  src/wx/jsonwriter.h
	  src/wxJSON/jsonreader.cpp
	  src/wxJSON/jsonval.cpp
	  src/wxJSON/jsonwriter.cpp
    )
ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_GOODANCHORAGEPLUGIN})

INCLUDE("cmake/PluginInstall.cmake")
INCLUDE("cmake/PluginSQLite.cmake")
INCLUDE("cmake/PluginPackage.cmake")

