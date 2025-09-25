#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int matchhere(char *re, char *text);
int matchstar(int c, char *re, char *text);

int match(char *re, char *text) {
  if(re[0] == '^') return matchhere(re+1, text);
  do {
    if(matchhere(re, text)) return 1;
  } while(*text++ != '\0');
  return 0;
}

int matchhere(char *re, char *text) {
  if(re[0] == '\0') return 1;
  if(re[1] == '*') return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0') return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

int matchstar(int c, char *re, char *text) {
  do {
    if(matchhere(re, text)) return 1;
  } while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

char* fmtname(char *path) {
  static char buf[DIRSIZ+1];
  char *p;
  for(p=path+strlen(path); p >= path && *p != '/'; p--);
  p++;
  if(strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;
  return buf;
}

void regfind(char *path, char *pattern) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  if((fd = open(path, 0)) < 0) return;
  if(fstat(fd, &st) < 0) { close(fd); return; }
  if(st.type == T_FILE) {
    if(match(fmtname(path), pattern)) printf("%s\n", path);

  } else if(st.type == T_DIR) {
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) { close(fd); return; }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
      if(de.inum == 0) continue;     
      if(!strcmp(de.name, ".") || !strcmp(de.name, "..")) continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      regfind(buf, pattern);
    }
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if(argc < 3) exit(0);
  regfind(argv[1], argv[2]);
  exit(0);
}

