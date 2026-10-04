cmake_minimum_required(VERSION 3.16)

string(REPLACE "|" ";" files "${FILES}")

foreach(file ${files})
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "FAIL freestanding ${PREFIX}: ${file} is not there to check")
    endif()
endforeach()

execute_process(COMMAND ${NM} -g --defined-only ${files} OUTPUT_VARIABLE defined_out ERROR_QUIET)
execute_process(COMMAND ${NM} -u ${files} OUTPUT_VARIABLE wanted_out ERROR_QUIET)

string(REGEX MATCHALL "[0-9a-fA-F]+ [A-Za-z] [^\n ]+" defined_lines "${defined_out}")
string(REGEX MATCHALL " U [^\n ]+" wanted_lines "${wanted_out}")

set(defined "")

foreach(line ${defined_lines})
    string(REGEX REPLACE "^[0-9a-fA-F]+ [A-Za-z] " "" name "${line}")
    list(APPEND defined "${name}")
endforeach()

set(leaks "")

foreach(line ${wanted_lines})
    string(REGEX REPLACE "^ U " "" name "${line}")

    if(NOT name IN_LIST defined AND NOT name MATCHES "^_?(${PREFIX}_[a-z0-9_]+|memcpy|memmove|memset|memcmp|_GLOBAL_OFFSET_TABLE_|__stack_pointer)$")
        list(APPEND leaks "${name}")
    endif()
endforeach()

list(REMOVE_DUPLICATES leaks)

if(leaks)
    message(FATAL_ERROR "FAIL freestanding ${PREFIX}: undefined symbols outside the port contract: ${leaks}")
endif()

message(STATUS "freestanding ${PREFIX}: ok")
