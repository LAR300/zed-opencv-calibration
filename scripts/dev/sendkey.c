// Bench helper: press one key on an X display (XTest).
// Usage: DISPLAY=:98 sendkey <key>   e.g. sendkey s
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <unistd.h>
int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: sendkey <key>\n"); return 2; }
  Display* d = XOpenDisplay(NULL);
  if (!d) { fprintf(stderr, "cannot open display\n"); return 1; }
  KeySym sym = XStringToKeysym(argv[1]);
  KeyCode code = XKeysymToKeycode(d, sym);
  if (!code) { fprintf(stderr, "unknown key %s\n", argv[1]); return 1; }
  XTestFakeKeyEvent(d, code, True, 0); XFlush(d); usleep(50000);
  XTestFakeKeyEvent(d, code, False, 0); XFlush(d);
  XCloseDisplay(d);
  return 0;
}
