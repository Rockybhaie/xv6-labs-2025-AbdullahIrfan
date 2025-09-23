#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

char buf[512];

void
find(char *path, char *target, int use_exec, int exec_argc, char *exec_argv[])
{
  int fd;
  struct stat st;
  struct dirent de;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if(st.type == T_FILE){
    char *filename = path;
    for(int i = strlen(path)-1; i >= 0; i--){
      if(path[i] == '/'){
        filename = &path[i+1];
        break;
      }
    }
    if(strcmp(filename, target) == 0){
      if(use_exec){
        if(fork() == 0){
          char *argv[MAXARG];
          int j;
          for(j = 0; j < exec_argc; j++){
            argv[j] = exec_argv[j];
          }
          argv[j++] = path;
          argv[j] = 0;
          exec(exec_argv[0], argv);
          fprintf(2, "exec %s failed\n", exec_argv[0]);
          exit(1);
        } else {
          wait(0);
        }
      } else {
        printf("%s\n", path);
      }
    }
  } else if(st.type == T_DIR){
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
      printf("find: path too long\n");
      close(fd);
      return;
    }
    strcpy(buf, path);
    char *p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0) continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, target, use_exec, exec_argc, exec_argv);
    }
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc < 3){
    fprintf(2, "usage: find path filename [-exec cmd ...]\n");
    exit(1);
  }

  int use_exec = 0;
  int exec_argc = 0;
  char *exec_argv[MAXARG];

  // detect -exec
  int i;
  for(i = 3; i < argc; i++){
    if(strcmp(argv[i], "-exec") == 0){
      use_exec = 1;
      for(int j = i+1; j < argc; j++){
        exec_argv[exec_argc++] = argv[j];
      }
      break;
    }
  }

  find(argv[1], argv[2], use_exec, exec_argc, exec_argv);
  exit(0);
}


