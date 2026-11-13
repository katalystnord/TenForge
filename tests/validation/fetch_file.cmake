# Fetch one file, verify its SHA-256, keep it. For reference files that are
# not committed (the PMD ontologies: 2.5 MB, and released upstream).
#
#   cmake -DURL=... -DSHA256=... -DDEST=<file> -P fetch_file.cmake

foreach(v URL SHA256 DEST)
    if(NOT DEFINED ${v})
        message(FATAL_ERROR "fetch_file.cmake needs -D${v}=...")
    endif()
endforeach()
if(EXISTS "${DEST}")
    file(SHA256 "${DEST}" have)
    if(have STREQUAL SHA256)
        return()
    endif()
    file(REMOVE "${DEST}")
endif()
file(DOWNLOAD "${URL}" "${DEST}" STATUS status TIMEOUT 120 EXPECTED_HASH SHA256=${SHA256})
list(GET status 0 code)
if(NOT code EQUAL 0)
    list(GET status 1 msg)
    file(REMOVE "${DEST}")
    message(FATAL_ERROR "could not fetch ${URL}: ${msg}")
endif()
