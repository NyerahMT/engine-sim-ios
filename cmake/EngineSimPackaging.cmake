set(CPACK_PACKAGE_NAME "open-engine-simulator")
set(CPACK_PACKAGE_VENDOR "Open Engine Simulator contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Portable SDL desktop fork of AngeTheGreat's Engine Simulator")
set(CPACK_PACKAGE_FILE_NAME
    "open-engine-simulator-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_MONOLITHIC_INSTALL ON)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
if(WIN32 OR APPLE)
    set(CPACK_GENERATOR "ZIP")
elseif(UNIX)
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
