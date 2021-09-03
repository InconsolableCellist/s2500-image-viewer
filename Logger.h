#ifndef S2500_IMAGE_VIEWER_LOGGER_H
#define S2500_IMAGE_VIEWER_LOGGER_H

#include <string>

class Logger {
    private:
        int logfile = -1;
         Logger() = default;
        ~Logger() = default;
        static Logger *m_pInstance;

    public:
        static Logger *Instance();
        void init();
        void quit();
        void const log(const char *logFormat, ...);
};

#endif //S2500_IMAGE_VIEWER_LOGGER_H
