#include "Logger.h"
#include <sys/stat.h>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdarg>

Logger *Logger::m_pInstance = nullptr;
int logfile;

Logger *Logger::Instance() {
    if (!m_pInstance) {
        m_pInstance = new Logger;
    }
    return m_pInstance;
}

void Logger::init() {
    char buffer[256];
    std::time_t t = std::time(nullptr);
    std::tm *now = std::localtime(&t);
    struct stat st = {0};

    if (stat("logs", &st) == -1) {
        mkdir("logs", 0750);
    }

    strftime(buffer, sizeof(buffer), "logs/%F_%T.log", now); // "2021-09-02_15:47:00.log"
    printf("Want to open %s\n", buffer);
    logfile = open(buffer, O_CREAT | O_WRONLY, 0660);
    if (logfile == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Unable to open log file %s!\n", buffer);
        log(msg);
    }
}

void const Logger::log(const char *logFormat, ...) {
    char buffer[256];
    char msg[256];
    std::time_t t = std::time(0);
    std::tm *now = std::localtime(&t);
    va_list vl;
    char logString[256];

    va_start(vl, logFormat);
    if (vsnprintf(logString, sizeof(logString), logFormat, vl) == -1) {
        logString[sizeof(logString)-1] = '\0';
    }
    va_end(vl);

    strftime(buffer, sizeof(buffer), "%F %T", now); // "2021-09-02_15:47:00.log"
    sprintf(msg, "%s: %s\n", buffer, logString);
    printf("%s", msg);

    write(logfile, msg, strlen(msg));
}

void Logger::quit() {
    if (logfile != -1) {
        close(logfile);
    }
}