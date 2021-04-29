#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

"""Outputs the CFFI binding .c source file.

This is called from CMake to generate the file which CMake then compiles.
We will have to figure how to best support the other scenario, when Python Setuptools is
driving the build. Lets postpone that for later.
"""

import argparse

import cffi


def main():
    parser = argparse.ArgumentParser(description='')
    parser.add_argument('output_filename', type=str,
                        help='filename to write the generated .c code into')

    args = parser.parse_args()
    run_cffi(args.output_filename)


def run_cffi(output_filename):
    ffibuilder = cffi.FFI()
    ffibuilder.set_source(
        module_name='_proton_core',
        source="""
            #include "proton/message.h"
        """,
    )
    ffibuilder.cdef("""
    // ???
        typedef struct pn_data_t pn_data_t;
    
    // codec.h
        typedef struct pn_atom_t { ...; } pn_atom_t;
    
    // types.h
        typedef uint32_t pn_millis_t;
        typedef struct pn_bytes_t { ...; } pn_bytes_t;
        typedef int64_t pn_timestamp_t;
        typedef uint32_t  pn_sequence_t;
        typedef struct pn_rwbytes_t pn_rwbytes_t;
        typedef struct pn_link_t pn_link_t;
    
    // error.h
        #define PN_OVERFLOW ...
        
        typedef struct pn_error_t pn_error_t;
        
        const char *pn_error_text(pn_error_t *error);
    
    // message.h
        #define PN_DEFAULT_PRIORITY ...
    
        typedef struct pn_message_t pn_message_t;
    
        pn_message_t * pn_message(void);
        void           pn_message_free(pn_message_t *msg);
        
        bool           pn_message_is_durable            (pn_message_t *msg);
        int            pn_message_set_durable           (pn_message_t *msg, bool durable);
        
        uint8_t        pn_message_get_priority          (pn_message_t *msg);
        int            pn_message_set_priority          (pn_message_t *msg, uint8_t priority);
    """)

    ffibuilder.emit_c_code(output_filename)


if __name__ == "__main__":
    main()
