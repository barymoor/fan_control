#pragma once
// #include <stdio.h>
#include <syslog.h>

// #define app_log(prio, fmt, arg...) \
//     if(prio <= LOG_DEBUG) \
//         printf(fmt, ##arg)

#define app_log(prio, fmt, arg...) \
    if(prio <= LOG_INFO) \
        syslog(prio, fmt, ##arg)
