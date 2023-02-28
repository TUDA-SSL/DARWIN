#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef DARWIN_H
#define DARWIN_H

typedef unsigned int uint;

void DARWIN_init(uint64_t, unsigned);

int DARWIN_SelectOperator(uint64_t);

void DARWIN_NotifyFeedback(uint64_t, unsigned);

uint32_t DARWIN_get_parent_repr(uint64_t);

#endif
