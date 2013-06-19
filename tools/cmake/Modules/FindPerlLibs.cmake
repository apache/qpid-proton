# - Find Perl Libraries
# This module searches for Perl libraries in the event that those files aren't
# found by the default Cmake module.

# include(${CMAKE_CURRENT_LIST_DIR}/FindPerlLibs.cmake)

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
  ENDIF ( NOT PERL_RETURN_VALUE )

  # if either the library path is not found not set at all
  # then do our own search
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

  IF ( PERL_LIBRARY )
    MESSAGE ( STATUS "Found PerlLibs: ${PERL_LIBRARY}" )
  ELSE()
    MESSAGE ( STATUS "PerlLibs Not Found" )
  ENDIF ( PERL_LIBRARY )

  if (PERL_LIBRARY)
    set (DEFAULT_PERL ON)
  endif (PERL_LIBRARY)
endif(NOT PERLLIBS_FOUND)
