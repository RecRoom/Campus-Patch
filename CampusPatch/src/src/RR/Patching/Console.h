#pragma once

#include <string>

// Console Colors
#define CLR_RESET   "\x1b[0m"
#define CLR_DIM     "\x1b[90m"
#define CLR_CYAN    "\x1b[38;5;51m"
#define CLR_GREEN   "\x1b[38;5;120m"
#define CLR_YELLOW  "\x1b[38;5;227m"
#define CLR_RED     "\x1b[38;5;203m"
#define CLR_MAGENTA "\x1b[38;5;213m"
#define CLR_BLUE    "\x1b[38;5;111m"
#define CLR_ORANGE  "\x1b[38;5;215m"
#define CLR_VIOLET  "\x1b[38;5;141m"
#define CLR_BOLD    "\x1b[1m"
#define CLR_ERR     "\x1b[1m\x1b[38;5;196m"

// Log Tags
#define TAG_NET()     CLR_CYAN    "[NET]     " CLR_RESET
#define TAG_HTTP()    CLR_CYAN    "[HTTP]    " CLR_RESET
#define TAG_PATCH()   CLR_GREEN   "[PATCH]   " CLR_RESET
#define TAG_REFEREE() CLR_YELLOW  "[REFEREE] " CLR_RESET
#define TAG_TLS()     CLR_YELLOW  "[TLS]     " CLR_RESET
#define TAG_UI()      CLR_VIOLET  "[UI]      " CLR_RESET
#define TAG_HILE()    CLR_RED     "[HILE]    " CLR_RESET
#define TAG_PHOTON()  CLR_BLUE    "[PHOTON]  " CLR_RESET
#define TAG_WARN()    CLR_ORANGE  "[WARN]    " CLR_RESET
#define TAG_INIT()    CLR_BOLD CLR_GREEN "[INIT]    " CLR_RESET