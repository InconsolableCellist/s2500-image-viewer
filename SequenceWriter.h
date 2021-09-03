#ifndef S2500_IMAGE_VIEWER_SEQUENCEWRITER_H
#define S2500_IMAGE_VIEWER_SEQUENCEWRITER_H

#include <ctime>
#include "sem_capture_info.h"
#include "sem_capture_pixels.h"

#define RELATIVE_DIRECTORY_NAME_LENGTH_BYTES 64

class SequenceWriter {
    private:
        int fileNumber = 0;
        int sequenceNumber = 0;
        std::time_t t;
        std::tm *now;
        char *relativeDirectoryName;

    public:
        bool shouldWrite = false;

        SequenceWriter(int sequenceNumber);
        ~SequenceWriter();
        int getCurrentFileNum();
        char *saveNextFileInSequence(SEMCapture &captureInfo, SEMCapturePixels &pixels);
        void IncrementSequenceNumber();
};

#endif //S2500_IMAGE_VIEWER_SEQUENCEWRITER_H
