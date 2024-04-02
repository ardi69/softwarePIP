#ifndef setup_h__
#define setup_h__
enum eFrameMode {
	kFrameModeI,
	kFrameModeIP,
	kFrameModeIPB
};

typedef struct {
	eFrameMode FrameMode(bool h264) { return h264 ? h264FrameMode : mpeg2FrameMode; }
	eFrameMode mpeg2FrameMode = kFrameModeIP;
	eFrameMode h264FrameMode = kFrameModeI;
	int FrameDrop = -1;
	bool SwapFfmpeg = false;
} sSoftwarePipSetup;
extern sSoftwarePipSetup SoftwarePipSetup;






#endif // setup_h__
