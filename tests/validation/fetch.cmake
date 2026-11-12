# Fetch a validation dataset that may not be committed, verify it, unpack it.
#
#   cmake -DURL=... -DSHA256=... -DDEST=<dir> -P fetch.cmake
#
# Downloads once into DEST (the build tree, never the source tree), checks
# the SHA-256 so a changed or truncated file is refused rather than tested
# against, and extracts it. A second run finds the archive and only
# re-verifies it.

foreach(v URL SHA256 DEST)
    if(NOT DEFINED ${v})
        message(FATAL_ERROR "fetch.cmake needs -D${v}=...")
    endif()
endforeach()

set(archive "${DEST}/download.zip")
file(MAKE_DIRECTORY "${DEST}")

if(EXISTS "${archive}")
    file(SHA256 "${archive}" have)
    if(NOT have STREQUAL SHA256)
        file(REMOVE "${archive}")
    endif()
endif()

if(NOT EXISTS "${archive}")
    file(DOWNLOAD "${URL}" "${archive}" STATUS status TIMEOUT 120
         EXPECTED_HASH SHA256=${SHA256})
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        list(GET status 1 msg)
        file(REMOVE "${archive}")
        message(FATAL_ERROR "could not fetch ${URL}: ${msg}")
    endif()
endif()

file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${DEST}/data")
message(STATUS "validation data ready in ${DEST}/data")
