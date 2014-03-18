FIND_PACKAGE(Doxygen)

IF (DOXYGEN_FOUND)
    CONFIGURE_FILE(Doxyfile.in Doxyfile @ONLY)
    ADD_CUSTOM_TARGET(doxygen
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating bemenu documentation with Doxygen" VERBATIM)
    MESSAGE("-!- Use 'make doxygen' to generate documentation")
ENDIF (DOXYGEN_FOUND)

# vim: set ts=8 sw=4 tw=0 :
