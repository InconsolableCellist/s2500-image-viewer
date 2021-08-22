#ifndef S2500_IMAGE_VIEWER_SEM_CAPTURE_INFO_H
#define S2500_IMAGE_VIEWER_SEM_CAPTURE_INFO_H

struct SEMCapture {
    uint16_t *dataBuffer;
    const uint16_t BUF_SIZE_SAMPLES = 1024;
    const uint16_t BUF_SIZEOF_BYTES = sizeof(uint16_t) * BUF_SIZE_SAMPLES;
    int datafile = 0;
    uint16_t sourceWidth = 592; // must be divisible by 4
    uint16_t sourceHeight = 1134;
    double syncDuration = 0;
    double frameDuration = 0;
    uint8_t scanMode = 0;
    double minSync = 65535;
    double maxSync = 0;
    double syncAverage = 0;
    uint32_t syncNum = 0;
    uint32_t bytesRead = 0;
    uint8_t newFrame = 0;
};

#endif //S2500_IMAGE_VIEWER_SEM_CAPTURE_INFO_H
