#ifndef INC__ARGS_HPP__
#define INC__ARGS_HPP__

#include <string>

class Args //class for processing arguments
{
	public :
		bool captureAll;       // capture all found targets to image files?
		bool tracking;         // display tracking info?
		bool rects;            // display frame by frame hit info
		bool batchMode;        // non-interactive mode - no display, run through
						       // as quickly as possible. Combine with --all?
		bool pause;            // start paused
		int  skip;             // skip over frames (video file input only)
		bool writeVideo;       // write captured video to output
		int  writeVideoSkip;   // only write 1 of every N frames
		bool saveVideo;        // write processed video to output
		bool saveVideoSkip;    // only write 1 of every N processed frames
		int  frameStart;       // frame number to start from
		bool calibrate;        // crosshair to calibrate camera
		bool detection;         // enable object detection?
		std::string d12BaseDir; // base directory for d12 net info
		int  d12DirNum;         // d12 directory and
		int  d12StageNum;       // stage to use
		std::string d24BaseDir; // base directory for d24 net info
		int  d24DirNum;         // d24 directory and
		int  d24StageNum;       // stage to use
		std::string inputName; // input file name or camera number
		bool groundTruth;      // only test frames with ground truth data
		std::string xmlFile;   // XML settings file

		Args(void);
		bool processArgs(int argc, const char **argv);
};

#endif
