// tr.c - tiny tr/sed-ish thing for xv6
 
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
 
static char *help =
"USAGE:\n"
"  tr\n"
"  tr h\n"
"  tr d <SET>\n"
"  tr s <MATCH> <SUBSTITUTION>\n"
"  tr t <SETA> <SETB>\n"
"\n"
"OPTIONS:\n"
"  no option given : simply copy STDIN to STDOUT.\n"
"  h  : print this help message.\n"
"  d  : Delete Mode. Delete occurrences of any character in the SET.\n"
"  s  : Substitute Mode. Substitute occurrences of the string MATCH with the string SUBSTITUTION.\n"
"  t  : Translate Mode. Replace occurrences of the n-th character in SET A with the n-th character in SET B. (Sets A and B must have the same length)\n";
 
static void
usage_and_exit(int code)
{
  write(1, help, strlen(help));
  exit(code);
}
 
static int
parse_escapes(const char *in, char *out, int max)
{
  int i = 0, j = 0;
 
  while (in[i] != '\0') {
    if (j >= max)
      return -1;
 
    if (in[i] == '\\') {
      char c = in[i+1];
      if (c == '\0') {
        // trailing backslash -> literal '\'
        out[j++] = '\\';
        i++;
      } else {
        if (c == 'n')      out[j++] = '\n';
        else if (c == 't') out[j++] = '\t';
        else if (c == 'r') out[j++] = '\r';
        else if (c == 'b') out[j++] = '\b';
        else if (c == 's') out[j++] = ' ';
        else if (c == '\\') out[j++] = '\\';
        else {
          // unknown escape: just take the character literally
          out[j++] = c;
        }
        i += 2;
      }
    } else {
      out[j++] = in[i++];
    }
  }
  out[j] = '\0';
  return j;
}
 
static void
mode_copy(void)
{
  char buf[512];
  int n;
  while ((n = read(0, buf, sizeof buf)) > 0) {
    write(1, buf, n);
  }
}
 
static int
in_set(char c, const char *set)
{
  int i;
  for (i = 0; set[i] != '\0'; i++)
    if (set[i] == c)
      return 1;
  return 0;
}
 
static void
mode_delete(const char *set)
{
  char buf[512], out[512];
  int n, i, o;
 
  while ((n = read(0, buf, sizeof buf)) > 0) {
    o = 0;
    for (i = 0; i < n; i++) {
      if (!in_set(buf[i], set)) {
        out[o++] = buf[i];
        if (o == sizeof out) {
          write(1, out, o);
          o = 0;
        }
      }
    }
    if (o > 0)
      write(1, out, o);
  }
}
 
static void
mode_translate(const char *seta, const char *setb)
{
  char buf[512], out[512];
  int n, i, j, o;
 
  while ((n = read(0, buf, sizeof buf)) > 0) {
    o = 0;
    for (i = 0; i < n; i++) {
      char c = buf[i];
      for (j = 0; seta[j] != '\0'; j++) {
        if (c == seta[j]) {
          c = setb[j];
          break;
        }
      }
      out[o++] = c;
      if (o == sizeof out) {
        write(1, out, o);
        o = 0;
      }
    }
    if (o > 0)
      write(1, out, o);
  }
}
 
static void
mode_substitute(const char *match, const char *sub)
{
  int mlen = strlen(match);
  int slen = strlen(sub);
 
  if (mlen == 0) {
    // empty MATCH: just copy input unchanged
    mode_copy();
    return;
  }
 
  int cap = 1024;
  char *buf = (char*)malloc(cap);
  if (!buf) {
    usage_and_exit(-1);
  }
 
  int len = 0, n;
  while ((n = read(0, buf + len, cap - len)) > 0) {
    len += n;
    if (len == cap) {
      int ncap = cap * 2;
      char *nbuf = (char*)malloc(ncap);
      if (!nbuf) {
        free(buf);
        usage_and_exit(-1);
      }
      memmove(nbuf, buf, len);
      free(buf);
      buf = nbuf;
      cap = ncap;
    }
  }
 
  int i = 0;
  while (i < len) {
    if (i + mlen <= len && memcmp(buf + i, match, mlen) == 0) {
      if (slen > 0)
        write(1, sub, slen);
      i += mlen;
    } else {
      write(1, buf + i, 1);
      i++;
    }
  }
 
  free(buf);
}
 
int
main(int argc, char *argv[])
{
  if (argc == 1) {
    // No args: raw copy
    mode_copy();
    exit(0);
  }
 
  // Help mode: "tr h"
  if (argv[1][0] == 'h' && argv[1][1] == '\0') {
    usage_and_exit(0);
  }
 
  // Mode must be a single char
  if (argv[1][1] != '\0') {
    usage_and_exit(-1);
  }
 
  char mode = argv[1][0];
  char set[128], seta[128], setb[128], match[128], sub[128];
  int len;
 
  switch (mode) {
  case 'd':
    if (argc != 3)
      usage_and_exit(-1);
    len = parse_escapes(argv[2], set, 127);
    if (len < 0)
      usage_and_exit(-1);
    mode_delete(set);
    break;
 
  case 't':
    if (argc != 4)
      usage_and_exit(-1);
    len = parse_escapes(argv[2], seta, 127);
    if (len < 0)
      usage_and_exit(-1);
    len = parse_escapes(argv[3], setb, 127);
    if (len < 0)
      usage_and_exit(-1);
    if (strlen(seta) != strlen(setb))
      usage_and_exit(-1);
    mode_translate(seta, setb);
    break;
 
  case 's':
    if (argc != 4)
      usage_and_exit(-1);
    len = parse_escapes(argv[2], match, 127);
    if (len < 0)
      usage_and_exit(-1);
    len = parse_escapes(argv[3], sub, 127);
    if (len < 0)
      usage_and_exit(-1);
    mode_substitute(match, sub);
    break;
 
  default:
    usage_and_exit(-1);
  }
 
  exit(0);
}