#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEBUG 1 // 1 to print debug info
#define SOURCEFILE "/etc/passwd"
#define TARGETFILE "/tmp/passwd"

int main(int argc, char **argv) {
  // #1: print own process ID
  printf("sneaky_process pid = %d\n", getpid());

  // #2: step 1 - copy from source to target
  FILE *source, *target;
  source = fopen(SOURCEFILE, "r");
  if (source == NULL) {
    printf("Cannot open source file...\n");
    exit(EXIT_FAILURE);
  }

  target = fopen(TARGETFILE, "w");
  if (source == NULL) {
    fclose(source);
    printf("Cannot open target file...\n");
    exit(EXIT_FAILURE);
  }

  char ch;
  while ((ch = fgetc(source)) != EOF) {
    fputc(ch, target);
  }

  if (DEBUG)
    printf("File copied successfully.\n");

  fclose(source);
  fclose(target);

  return 0;
}
