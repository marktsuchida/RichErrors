#pragma once

// Automatically generate strings containing line number (facilitates debugging
// memory leaks)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define TESTSTR(p) (p "-" TOSTRING(__LINE__))
