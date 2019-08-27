#ifndef RECORDER_H
#define RECORDER_H

#include "util.h"

int recorderInit(ApplicationBaseData* bd);
int recorderExit();

int recorderStart();
int recorderStop();

#endif //RECORDER_H