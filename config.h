#ifndef CONFIG_H
#define CONFIG_H

#include "cmdline.h"

#define MINIMUM_KEY_LENGTH 2
#define MAXIMUM_CONNECTIONS 512
#define LOADER_CHUNK 1024

extern char random_char[];
extern gengetopt_args_info args;

#endif