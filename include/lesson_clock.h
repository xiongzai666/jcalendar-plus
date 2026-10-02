#pragma once

#include "lesson_clock_core.h"

// Loads the currently active lesson times from the same daily routine that
// supplies the footer's activities. Invalid legacy local data falls back to
// the original schedule until a corrected cloud version arrives.
LessonClock getLessonClock();
