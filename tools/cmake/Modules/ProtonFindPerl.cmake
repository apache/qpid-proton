# - Find Perl Libraries
# This module searches for Perl libraries in the event that those files aren't
# found by the default Cmake module.

# include(${CMAKE_CURRENT_LIST_DIR}/FindPerlLibs.cmake)

include(FindPerl)
include(FindPerlLibs)

if(NOT PERLLIBS_FOUND)
  MESSAGE ( STATUS "Trying alternative search for Perl" )

  # taken from Cmake 2.8 FindPerlLibs.cmake
  EXECUTE_PROCESS ( COMMAND ${PERL_EXECUTABLE}
                     -V:installarchlib
                     OUTPUT_VARIABLE PERL_ARCHLIB_OUTPUT_VARIABLE
                     RESULT_VARIABLE PERL_ARCHLIB_RESULT_VARIABLE )

  if (NOT PERL_ARCHLIB_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_ARCHLIB ${PERL_ARCHLIB_OUTPUT_VARIABLE})
    file(TO_CMAKE_PATH "${PERL_ARCHLIB}" PERL_ARCHLIB)
  endif ( NOT PERL_ARCHLIB_RESULT_VARIABLE )

  EXECUTE_PROCESS ( COMMAND ${PERL_EXECUTABLE}
                    -MConfig -e "print \$Config{archlibexp}"
                    OUTPUT_VARIABLE PERL_OUTPUT
                    RESULT_VARIABLE PERL_RETURN_VALUE )

  IF ( NOT PERL_RETURN_VALUE )
    FIND_PATH ( PERL_INCLUDE_PATH perl.h ${PERL_OUTPUT}/CORE )

    IF (PERL_INCLUDE_PATH MATCHES .*-NOTFOUND OR NOT PERL_INCLUDE_PATH)
        MESSAGE(STATUS "Could not find perl.h")
    ENDIF ()

  ENDIF ( NOT PERL_RETURN_VALUE )

  # if either the library path is not found not set at all
  # then do our own search
  if ( NOT PERL_LIBRARY )
    EXECUTE_PROCESS( COMMAND ${PERL_EXECUTABLE} -V:libperl
                     OUTPUT_VARIABLE PERL_LIBRARY_OUTPUT
                     RESULT_VARIABLE PERL_LIBRARY_RESULT )

    IF ( NOT PERL_LIBRARY_RESULT )
      string(REGEX REPLACE "libperl='([^']+)'.*" "\\1" PERL_POSSIBLE_LIBRARIES ${PERL_LIBRARY_OUTPUT})
    ENDIF ( NOT PERL_LIBRARY_RESULT )

    MESSAGE ( STATUS  "Looking for ${PERL_POSSIBLE_LIBRARIES}" )

    find_file(PERL_LIBRARY
      NAMES ${PERL_POSSIBLE_LIBRARIES}
      PATHS /usr/lib
            ${PERL_ARCHLIB}/CORE
      )

  endif ( NOT PERL_LIBRARY )

  IF ( PERL_LIBRARY MATCHES .*-NOTFOUND OR NOT PERL_LIBRARY )
      EXECUTE_PROCESS ( COMMAND ${PERL_EXECUTABLE}
                        -MConfig -e "print \$Config{libperl}"
                        OUTPUT_VARIABLE PERL_OUTPUT
                        RESULT_VARIABLE PERL_RETURN_VALUE )

      IF ( NOT PERL_RETURN_VALUE )
        FIND_LIBRARY ( PERL_LIBRARY NAMES ${PERL_OUTPUT}
                                    PATHS ${PERL_INCLUDE_PATH} )

      ENDIF ( NOT PERL_RETURN_VALUE )
  ENDIF ( PERL_LIBRARY MATCHES .*-NOTFOUND OR NOT PERL_LIBRARY )

  IF(PERL_LIBRARY MATCHES .*-NOTFOUND OR NOT PERL_LIBRARY OR
     PERL_INCLUDE_PATH MATCHES .*-NOTFOUND OR NOT PERL_INCLUDE_PATH)
    MESSAGE (STATUS "No Perl devel environment found - skipping Perl bindings")
    SET (DEFAULT_PERL OFF)
  ELSE()
    MESSAGE ( STATUS "Found PerlLibs: ${PERL_LIBRARY}" )
    SET (DEFAULT_PERL ON)
  ENDIF()

endif(NOT PERLLIBS_FOUND)
