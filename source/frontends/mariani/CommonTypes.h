//
//  CommonTypes.h
//  Mariani
//
//  Created by sh95014 on 12/29/21.
//

#ifndef CommonTypes_h
#define CommonTypes_h

typedef struct {
    void *data;
    unsigned pixelWidth;
    unsigned pixelHeight;
    unsigned bufferWidth;
    unsigned bufferHeight;
    unsigned borderWidth;
    unsigned borderHeight;
} FrameBuffer;

#endif /* CommonTypes_h */
