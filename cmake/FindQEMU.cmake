# Set up variables for QEMU build and source dir
# QEMU build dir needs to be specified with QEMU_BUILD

IF ( NOT EXISTS ${QEMU_BUILD}/config-host.mak )
MESSAGE ( "ERROR: Cannot find config-host.mak in your Qemu build directory. Cannot configure Qemu." )
SET ( QEMU_FOUND false )
RETURN ()
ELSE ( NOT EXISTS ${QEMU_BUILD}/config-host.mak )
SET ( QEMU_FOUND true )
ENDIF ( NOT EXISTS ${QEMU_BUILD}/config-host.mak )

# Find QEMU source path
FILE ( STRINGS ${QEMU_BUILD}/config-host.mak QEMU_LINES REGEX "SRC_PATH=" )
STRING ( REGEX REPLACE "^SRC_PATH=(.*)$" "\\1" QEMU_SOURCE ${QEMU_LINES} )

# Find architecture for which TCG is built
FILE ( STRINGS ${QEMU_BUILD}/config-host.mak QEMU_LINES REGEX "QEMU_INCLUDES=" )
STRING ( REGEX REPLACE "^.*/tcg/([0-9a-z_-]+) -I.*$" "\\1" QEMU_TCG_ARCH ${QEMU_LINES} ) 

# Find targets for which are built
FILE ( STRINGS ${QEMU_BUILD}/config-host.mak QEMU_LINES REGEX "TARGET_DIRS=" )
STRING ( REPLACE "TARGET_DIRS=" "" QEMU_LINES_2 "${QEMU_LINES}" )
STRING ( REPLACE " " ";" QEMU_TARGET_DIRS "${QEMU_LINES_2}" )
FOREACH ( QEMU_TARGET_DIR ${QEMU_TARGET_DIRS} )
    FILE ( STRINGS ${QEMU_BUILD}/${QEMU_TARGET_DIR}/config-target.mak QEMU_LINES REGEX "TARGET_BASE_ARCH=" )
    STRING ( REPLACE "TARGET_BASE_ARCH=" "" QEMU_BASE_ARCH ${QEMU_LINES} )
    FILE ( STRINGS ${QEMU_BUILD}/${QEMU_TARGET_DIR}/config-target.mak QEMU_LINES REGEX "TARGET_NAME=" )
    STRING ( REPLACE "TARGET_NAME=" "" QEMU_ARCH ${QEMU_LINES} )
    LIST ( APPEND QEMU_TARGETS "${QEMU_BASE_ARCH},${QEMU_TARGET_DIR},${QEMU_ARCH}" )
ENDFOREACH ( QEMU_TARGET_DIR ${QEMU_TARGET_DIRS} )

#TODO: Improve above to get right source and build directories for each target
#SET ( QEMU_TARGET_ARCHITECTURES "i386" "arm" )

FILE ( STRINGS ${QEMU_BUILD}/config-host.mak QEMU_LINES REGEX "QEMU_INCLUDES=" )
STRING ( REPLACE "QEMU_INCLUDES="  "" QEMU_LINES_2 ${QEMU_LINES} )
STRING ( REPLACE "$(SRC_PATH)" "${QEMU_SOURCE}" QEMU_INCLUDES ${QEMU_LINES_2} )
STRING ( REPLACE "-I" ";" QEMU_INCLUDE_DIRECTORIES ${QEMU_INCLUDES} ) #${QEMU_SOURCE}/tcg ${QEMU_SOURCE}/include ${QEMU_BUILD} ${QEMU_SOURCE}/tcg/${QEMU_TCG_ARCH} )
LIST ( APPEND QEMU_INCLUDE_DIRECTORIES ${QEMU_BUILD} )

FILE ( STRINGS ${QEMU_BUILD}/config-host.mak QEMU_LINES REGEX "GTK_CFLAGS=" )
STRING ( REPLACE "GTK_CFLAGS="  "" QEMU_LINES_2 "${QEMU_LINES}" )
STRING ( REPLACE " " ";" QEMU_GTK_FLAGS "${QEMU_LINES_2}" )
FOREACH ( GTK_FLAG ${QEMU_GTK_FLAGS} )
    IF ( ${GTK_FLAG} MATCHES "^-I.*" )
        STRING ( REGEX REPLACE "^-I" "" INCLUDE_DIR ${GTK_FLAG} )
        LIST ( APPEND QEMU_INCLUDE_DIRECTORIES ${INCLUDE_DIR} )
    ENDIF ()
ENDFOREACH ()

FILE ( STRINGS ${QEMU_BUILD}/config-host.mak QEMU_LINES REGEX "QEMU_CFLAGS=" )
STRING ( REPLACE "QEMU_CFLAGS="  "" QEMU_LINES "${QEMU_LINES}" )
STRING ( REPLACE "$(SRC_PATH)" "${QEMU_SOURCE}" QEMU_LINES "${QEMU_LINES}" )
STRING ( REPLACE " " ";" QEMU_CFLAGS "${QEMU_LINES}" )
FOREACH ( C_FLAG ${QEMU_CFLAGS} )
    IF ( ${C_FLAG} MATCHES "^-I.*" )
        STRING ( REGEX REPLACE "^-I" "" INCLUDE_DIR ${C_FLAG} )
        LIST ( APPEND QEMU_INCLUDE_DIRECTORIES ${INCLUDE_DIR} )
    ENDIF ()
ENDFOREACH ()

MARK_AS_ADVANCED ( 
        QEMU_LINES
        QEMU_LINES_2
        QEMU_TCG_ARCH
        QEMU_TARGET_DIRS
        QEMU_TARGET_DIR
        QEMU_TARGET_ARCH )