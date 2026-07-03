#pragma once

#include "types.h"
#include "dyn_aos.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <clang-c/Index.h>
#include <clang-c/CXCompilationDatabase.h>

extern bool arg_verbose;

ThreadInfo init_thread_pool(size_t num_threads);
void destroy_thread_pool(ThreadInfo *prong_tp);

void *parse_file_thread(void *arg);

