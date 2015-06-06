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
SET(VERSION_MINOR "3")

#SET(CMAKE_BUILD_TYPE Debug)

INCLUDE("cmake/PluginConfigure.cmake")

SET(SRC_GOODANCHORAGEPLUGIN
      src/goodanchorage_pi.h
      src/goodanchorage_pi.cpp
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
IF(WIN32)
    SET(SRC_LSQLITE
        src/sqlite/sqlite3.h
    )
ENDIF(WIN32)
ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_GOODANCHORAGEPLUGIN} ${SRC_LSQLITE})

INCLUDE("cmake/PluginInstall.cmake")
INCLUDE("cmake/PluginSQLite.cmake")
INCLUDE("cmake/PluginLocalization.cmake")
INCLUDE("cmake/PluginPackage.cmake")

