#if defined(ENABLE_TAP_DETECT)

#ifndef _TAP_DETECT_H
#define _TAP_DETECT_H


extern bool doubleTapDetected;
//extern bool isDoubleTapDetectionRunning;
extern float detectedAcc[];

void invokeDoubleTapDetectTask(void);
void stopDoubleTapDetectTask(void);
void resumeDoubleTapDetectTask(void);

#endif  //_TAP_DETECT_H
#endif  //ENABLE_TAP_DETECT