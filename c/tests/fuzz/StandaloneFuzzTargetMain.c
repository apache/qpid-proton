/*===- StandaloneFuzzTargetMain.c - standalone main() for fuzz targets. ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This main() function can be linked to a fuzz target (i.e. a library
// that exports LLVMFuzzerTestOneInput() and possibly LLVMFuzzerInitialize())
// instead of libFuzzer. This main() function will not perform any fuzzing
// but will simply feed all input files one by one to the fuzz target.
//
// Use this file to provide reproducers for bugs when linking against libFuzzer
// or other fuzzing engine is undesirable.
//===----------------------------------------------------------------------===*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libFuzzingEngine.h"

/*
 * Use this to implement response file:
 * - Check if there is one file mentioned and its name starts with '@'
 * - If so then read the file line by line making up the new argv
 * - Modify argc/argv then return.
 *
 */

/* Free allocated memory at program exit to avoid the leak sanitizer complaining  */
static char *buf = 0;
static char **nargv = 0;

static void freeall(void)
{
  free(buf);
  free(nargv);
}

int ProcessResponseFile(int *argc, char ***argv) {
  if (*argc==2 && (*argv)[1][0]=='@') {
    const char* rfilename = (*argv)[1]+1;

    /* Read entire file into memory */
    fprintf(stderr, "Reading response file: %s\n", rfilename);
    FILE *f = fopen(rfilename, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char*)malloc(len+1);
    size_t n_read = fread(buf, 1, len, f);
    fclose(f);
    assert(n_read == len);
    buf[len] = '\0';

    /* scan file counting lines and replacing line ends with \0 */
    int line = 0;
    char *p = buf;
    while (p<&buf[len]) {
      p += strcspn(p, "\n\r ");
      *p++ = '\0';
      line +=1;
    };

    fprintf(stderr, "        response file: (%zd bytes, %d lines)\n", n_read, line);

    /* scan again putting each line into the argv array */
    nargv = (char**) calloc(line+1, sizeof(p));

    p = buf;
    line = 1;
    do {
      char* s = p;
      int l = strlen(p);
      p += l+1;
      if (l>0) nargv[line++] = s;
    } while (p<&buf[len]);

    int nargc = line;
    *argc = nargc;
    *argv = nargv;
  }
  return 0;
}

int main(int argc, char **argv) {
  fprintf(stderr, "StandaloneFuzzTargetMain: running %d inputs\n", argc - 1);
  LLVMFuzzerInitialize(&argc, &argv);

  // Process response file
  ProcessResponseFile(&argc, &argv);

  for (int i = 1; i < argc; i++) {
    fprintf(stderr, "Running: %s\n", argv[i]);
    FILE *f = fopen(argv[i], "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char*)malloc(len);
    size_t n_read = fread(buf, 1, len, f);
    fclose(f);
    assert(n_read == len);
    LLVMFuzzerTestOneInput(buf, len);
    free(buf);
    fprintf(stderr, "Done:    %s: (%zd bytes)\n", argv[i], n_read);
  }
  freeall();
}
