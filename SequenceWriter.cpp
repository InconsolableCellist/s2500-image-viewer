#include <cstdio>
#include "SequenceWriter.h"
#include "Logger.h"
#include <sys/stat.h>
#include <cstring>

SequenceWriter::SequenceWriter(int sequenceNumber) {
    this->sequenceNumber = sequenceNumber;
    this->fileNumber = 0;
    this->relativeDirectoryName = (char*)malloc(RELATIVE_DIRECTORY_NAME_LENGTH_BYTES);
    struct stat st = {0};
    this->t = std::time(nullptr);
    this->now = std::localtime(&t);

    if (stat("captures", &st) == -1) {
        mkdir("captures", 0750);
    }

    strftime(relativeDirectoryName, RELATIVE_DIRECTORY_NAME_LENGTH_BYTES, "captures/%F", now); // captures/2021-09-02
    if (stat(relativeDirectoryName, &st) == -1) {
        Logger::Instance()->log("Want to mkdir %s", relativeDirectoryName);
        mkdir(relativeDirectoryName, 0750);
    }

    strftime(relativeDirectoryName, RELATIVE_DIRECTORY_NAME_LENGTH_BYTES, "captures/%F/%H_%M_%S", now); // captures/2021-09-02/15_47_00
    if (stat(relativeDirectoryName, &st) == -1) {
        Logger::Instance()->log("Want to mkdir %s", relativeDirectoryName);
        mkdir(relativeDirectoryName, 0750);
    }
}

SequenceWriter::~SequenceWriter() {
    free(this->relativeDirectoryName);
}

/**
 * Given a valid SEMCapature and SEMCapturePixels, opens the next image in the sequence in the target directory
 * and writes it. Returns the relative file path and name to the saved file.
 *
 * The returned fileName MUST BE free()'d BY THE CALLING FUNCTION
 *
 * @param captureInfo
 * @param pixels
 * @return A pointer to the null-terminated fileName char array (max 64 chars) which MUST BE FREED BY THE CALLER.
 */
char *SequenceWriter::saveNextFileInSequence(SEMCapture &captureInfo, SEMCapturePixels &pixels) {
    char fileName[256];
    struct stat st = {0};

    snprintf(fileName, sizeof(fileName), "%s/%d", relativeDirectoryName, sequenceNumber);
    if (stat(fileName, &st) == -1) {
        Logger::Instance()->log("Want to mkdir %s", fileName);
        mkdir(fileName, 0750);
    }

    if (snprintf(fileName, sizeof(fileName), "%s/%d/%0.4d.png", relativeDirectoryName, sequenceNumber, fileNumber) == -1) {
        fileName[sizeof(fileName) - 1] = '\0';
    };
    Logger::Instance()->log("Want to save capture to %s", fileName);
    fileNumber += 1;

    return strdup(fileName);
}

void SequenceWriter::IncrementSequenceNumber() {
    sequenceNumber += 1;
    fileNumber = 0;
}
