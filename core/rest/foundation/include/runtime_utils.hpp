//
// Created by boa on 13.05.19.
//

#ifndef RUNTIME_UTILS_HPP
#define RUNTIME_UTILS_HPP

#include <execinfo.h>
#include <unistd.h>

namespace cfx {
    class RuntimeUtils {
    public:
        static void printStackTrace() {
            const int MAX_CALLSTACK = 100;
            void * callstack[MAX_CALLSTACK];
            int frames;

            // get void*'s for all entries on the stack...
            frames = backtrace(callstack, MAX_CALLSTACK);

            // print out all the frames to stderr...
            backtrace_symbols_fd(callstack, frames, STDERR_FILENO);
        }
    };
}
#endif //RUNTIME_UTILS_HPP
