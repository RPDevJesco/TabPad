#ifndef DESK_H
#define DESK_H

#include <stdint.h>

#define DESK_FONT_UI 0
#define DESK_FONT_TEXT 1

int desk_font(int which, char *name, uint32_t name_max, char *path, uint32_t path_max, int *index);
int desk_register(const char *argv0, const char *const *exts, uint32_t n);
int desk_unregister(const char *const *exts, uint32_t n);
int desk_claim(const char *path);
void desk_hand_over(void);

#endif
