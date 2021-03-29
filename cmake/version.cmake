# From https://www.mattkeeter.com/blog/2018-01-06-versioning/

message("Writing script_version.hpp")

find_program(GIT "git")

if(GIT)
    message("Found git at:")
    message(${GIT})
else()
    message("Did not find git!")
endif()

execute_process(COMMAND ${GIT} log --pretty=format:'%h' -n 1
                OUTPUT_VARIABLE GIT_REV
                WORKING_DIRECTORY ${SRCDIR}
               # ERROR_QUIET
               )

# Check whether we got any revision (which isn't
# always the case, e.g. when someone downloaded a zip
# file from Github instead of a checkout)
if ("${GIT_REV}" STREQUAL "")
    set(VERSION_MAJOR "0")
    set(VERSION_MINOR "0")
    set(VERSION_PATCH "0")
    set(VERSION_BUILD "0")
else()
    execute_process(
        COMMAND ${GIT} describe --tag --always
        OUTPUT_VARIABLE GIT_TAG ERROR_QUIET
        WORKING_DIRECTORY ${SRCDIR})

    message("Using git tag ${GIT_TAG}")

    string(REPLACE "-" ";" GIT_TAG_LIST ${GIT_TAG})
    list(LENGTH GIT_TAG_LIST GIT_TAG_LIST_LENGTH)

    list(GET GIT_TAG_LIST 0 VERSION_TAG)
    if(GIT_TAG_LIST_LENGTH GREATER 1)
        list(GET GIT_TAG_LIST 1 VERSION_BUILD)
    else()
        set(VERSION_BUILD "0")
    endif()
    message("Tag: ${VERSION_TAG}")
    message("Build: ${VERSION_BUILD}")
    string(REPLACE "." ";" GIT_VERSION_LIST ${VERSION_TAG})

    list(LENGTH GIT_VERSION_LIST GIT_VERSION_LIST_LENGTH)

    if(GIT_VERSION_LIST_LENGTH EQUAL 3)
        list(GET GIT_VERSION_LIST 0 VERSION_MAJOR)
        list(GET GIT_VERSION_LIST 1 VERSION_MINOR)
        list(GET GIT_VERSION_LIST 2 VERSION_PATCH)
    else()
        set(VERSION_MAJOR 0)
        set(VERSION_MINOR 0)
        set(VERSION_PATCH 0)
    endif()
    message("Version: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
    message("")
endif()

set(VERSION "#define MAJOR ${VERSION_MAJOR}
#define MINOR ${VERSION_MINOR}
#define PATCHLVL ${VERSION_PATCH}
#define BUILD ${VERSION_BUILD}
")

if(EXISTS ${VERSIONFILE_PATH})
    file(READ ${VERSIONFILE_PATH} VERSION_)
else()
    set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${VERSIONFILE_PATH} "${VERSION}")
endif()
