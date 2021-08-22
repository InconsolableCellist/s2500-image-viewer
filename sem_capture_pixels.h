#ifndef S2500_IMAGE_VIEWER_SEM_CAPTURE_PIXELS_H
#define S2500_IMAGE_VIEWER_SEM_CAPTURE_PIXELS_H

struct SEMCapturePixels {
    uint8_t *pixels;
    int32_t x = 0;
    int32_t y = 0;
    uint16_t min = 65535;
    uint16_t max = 0;
};

#endif //S2500_IMAGE_VIEWER_SEM_CAPTURE_PIXELS_H
