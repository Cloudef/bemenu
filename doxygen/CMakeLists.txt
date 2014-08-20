FIND_PACKAGE(Doxygen)

IF (DOXYGEN_FOUND)
    CONFIGURE_FILE(Doxyfile.in Doxyfile @ONLY)
    ADD_CUSTOM_TARGET(doxygen
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/doxygen-qmi-style/navtree ${CMAKE_CURRENT_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/doxygen-qmi-style/search ${CMAKE_CURRENT_BINARY_DIR}/html/search
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating bemenu documentation with Doxygen" VERBATIM)
    MESSAGE("-!- Use 'make doxygen' to generate documentation")
ENDIF (DOXYGEN_FOUND)

# vim: set ts=8 sw=4 tw=0 :
