#include <iostream>
#include "zedin.hpp"
using namespace std;

#ifdef ZED_SUPPORT
#include <boost/filesystem.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "cvMatSerialize.hpp"

using namespace cv;
using namespace boost::filesystem;

void zedBrightnessCallback(int value, void *data);
void zedContrastCallback(int value, void *data);
void zedHueCallback(int value, void *data);
void zedSaturationCallback(int value, void *data);
void zedGainCallback(int value, void *data);
void zedWhiteBalanceCallback(int value, void *data);

ZedIn::ZedIn(const char *inFileName, bool gui) :
	zed_(NULL),
	width_(0),
	height_(0),
	frameNumber_(0),
	serializeIn_(NULL),
	filtSBIn_(NULL),
	archiveIn_(NULL)
{
	if (inFileName)
	{
		// Might be svo, might be zms
		string fnExt = path(inFileName).extension().string();
		semValue_ = 1;
		if ((fnExt == ".svo") || (fnExt == ".SVO"))
			zed_ = new sl::zed::Camera(inFileName);
		else if ((fnExt == ".zms") || (fnExt == ".ZMS"))
		{
			// ZMS file is home-brewed serialization format
			// which just dumps raw a image and depth Mat data to a file.
			// Apply a light bit of compression because
			// the files will get out of hand quickly otherwise
			cerr << "Loading " << inFileName << " for reading" << endl;
			if (!openSerializeInput(inFileName))
				cerr << "Zed init : Could not open " << inFileName << " for reading" << endl;
		}
		else
			cerr << "Zed failed to start : unknown file extension " << fnExt << endl;
	}
	else
	{  // Open an actual camera for input
		zed_ = new sl::zed::Camera(sl::zed::HD720,15);
		semValue_ = 2;
	}

	if (zed_)
	{
		// init computation mode of the zed
		sl::zed::ERRCODE err = zed_->init(sl::zed::MODE::QUALITY, -1, true);
		// Quit if an error occurred
		if (err != sl::zed::SUCCESS)
		{
			cout << sl::zed::errcode2str(err) << endl;
			delete zed_;
			zed_ = NULL;
		}
		else
		{
			width_  = zed_->getImageSize().width;
			height_ = zed_->getImageSize().height;

#if 0
			brightness_ = zed_->getCameraSettingsValue(sl::zed::ZED_BRIGHTNESS);
			contrast_ = zed_->getCameraSettingsValue(sl::zed::ZED_CONTRAST);
			hue_ = zed_->getCameraSettingsValue(sl::zed::ZED_HUE);
			saturation_ = zed_->getCameraSettingsValue(sl::zed::ZED_SATURATION);
			gain_ = zed_->getCameraSettingsValue(sl::zed::ZED_GAIN);
			whiteBalance_ = zed_->getCameraSettingsValue(sl::zed::ZED_WHITEBALANCE);
#endif
			zedBrightnessCallback(3, this);
			zedContrastCallback(6, this);
			zedHueCallback(7, this);
			zedSaturationCallback(4, this);
			zedGainCallback(1, this);
			zedWhiteBalanceCallback(3101, this);

			cout << "brightness_ = " << zed_->getCameraSettingsValue(sl::zed::ZED_BRIGHTNESS) << endl;
			cout << "contrast_ = " << zed_->getCameraSettingsValue(sl::zed::ZED_CONTRAST) << endl;
			cout << "hue_ = " << zed_->getCameraSettingsValue(sl::zed::ZED_HUE) << endl;
			cout << "saturation_ = " << zed_->getCameraSettingsValue(sl::zed::ZED_SATURATION) << endl;
			cout << "gain_ = " << zed_->getCameraSettingsValue(sl::zed::ZED_GAIN) << endl;
			cout << "whiteBalance_ = " << zed_->getCameraSettingsValue(sl::zed::ZED_WHITEBALANCE) << endl;
			if (gui)
			{
				cv::namedWindow("Adjustments", CV_WINDOW_NORMAL);
				cv::createTrackbar("Brightness", "Adjustments", &brightness_, 9, zedBrightnessCallback, this);
				cv::createTrackbar("Contrast", "Adjustments", &contrast_, 9, zedContrastCallback, this);
				cv::createTrackbar("Hue", "Adjustments", &hue_, 12, zedHueCallback, this);
				cv::createTrackbar("Saturation", "Adjustments", &saturation_, 9, zedSaturationCallback, this);
				cv::createTrackbar("Gain", "Adjustments", &gain_, 9, zedGainCallback, this);
				cv::createTrackbar("White Balance", "Adjustments", &whiteBalance_, 6501, zedWhiteBalanceCallback, this);
			}
		}
	}
	else if (archiveIn_)
	{
		// Zed == NULL and serializeStream_ means reading from
		// a serialized file. Grab height_ and width_
		*archiveIn_ >> _frame >> depthMat_;
		if (!openSerializeInput(inFileName))
			cerr << "Zed init : Could not reopen " << inFileName << " for reading" << endl;
		width_  = _frame.cols;
		height_ = _frame.rows;
	}

	while (height_ > 700)
	{
		width_  /= 2;
		height_ /= 2;
	}
}

// Input needs 3 things. First is a standard ifstream to read from
// Next is an (optional) filtered stream buffer. This is used to
// uncompress on the fly - uncompressed files take up way too
// much space. Last item is the actual boost binary archive template
// If all three are opened, return true. If not, delete and set to
// NULL all pointers related to serialized Input
bool ZedIn::openSerializeInput(const char *inFileName)
{
	deleteInputPointers();
	serializeIn_ = new ifstream(inFileName, ios::in | ios::binary);
	if (!serializeIn_ || !serializeIn_->is_open())
	{
		cerr << "Could not open ifstream(" << inFileName << ")" << endl;
		deleteInputPointers();
		return false;
	}

	filtSBIn_= new boost::iostreams::filtering_streambuf<boost::iostreams::input>;
	if (!filtSBIn_)
	{
		cerr << "Could not create filtering_streambuf<input>" << endl;
		deleteInputPointers();
		return false;
	}
	filtSBIn_->push(boost::iostreams::zlib_decompressor());
	filtSBIn_->push(*serializeIn_);
	archiveIn_ = new boost::archive::binary_iarchive(*filtSBIn_);
	if (!archiveIn_)
	{
		cerr << "Could not create new binary_iarchive" << endl;
		deleteInputPointers();
		return false;
	}
	return true;
}

// Helper to easily delete and NULL out input file pointers
void ZedIn::deleteInputPointers(void)
{
	if (archiveIn_)
	{
		delete archiveIn_;
		archiveIn_ = NULL;
	}
	if (filtSBIn_)
	{
		delete filtSBIn_;
		filtSBIn_ = NULL;
	}
	if (serializeIn_)
	{
		delete serializeIn_;
		serializeIn_ = NULL;
	}
}



ZedIn::~ZedIn()
{
	deleteInputPointers();
	if (zed_)
		delete zed_;
}

bool ZedIn::update(bool left)
{
	// Read from either the zed camera or from
	// a previously-serialized ZMS file
	if (zed_)
	{
		if (zed_->grab(sl::zed::SENSING_MODE::RAW))
			return false;

		slDepth_ = zed_->retrieveMeasure(sl::zed::MEASURE::DEPTH);
		slFrame_ = zed_->retrieveImage(left ? sl::zed::SIDE::LEFT : sl::zed::SIDE::RIGHT);
		boost::lock_guard<boost::mutex> guard(_mtx);
		cvtColor(slMat2cvMat(slFrame_), _frame, CV_RGBA2RGB);
		slMat2cvMat(slDepth_).copyTo(depthMat_);

		while (_frame.rows > 700)
		{
			pyrDown(_frame, _frame);
			pyrDown(depthMat_, depthMat_);
		}
		frameNumber_ += 1;
	}
	else if (archiveIn_)
	{
		// Ugly try-catch to detect EOF
		try
		{
			*archiveIn_ >> localFrame_ >> localDepth_;
		}
		catch (const boost::archive::archive_exception &e)
		{
			return false;
		}
		boost::lock_guard<boost::mutex> guard(_mtx);
		localFrame_.copyTo(_frame);
		localDepth_.copyTo(depthMat_);

		while (_frame.rows > 700)
		{
			pyrDown(_frame, _frame);
			pyrDown(depthMat_, depthMat_);
		}
		frameNumber_ += 1;
	}
	else
		return false;
	return true;
}


bool ZedIn::getFrame(cv::Mat &frame, cv::Mat &depth)
{
	boost::lock_guard<boost::mutex> guard(_mtx);
	lockedFrameNumber_ = frameNumber_;
	_frame.copyTo(frame);
	depthMat_.copyTo(depth);
	return true;
}


bool ZedIn::update(void)
{
	return update(true);
}


int ZedIn::frameCount(void) const
{
	// Luckily getSVONumberOfFrames() returns -1 if we're
	// capturing from a camera, which is also what the rest
	// of our code expects in that case
	if (zed_)
		return zed_->getSVONumberOfFrames();

	// If using a video, there's no way to tell
	return -1;
}


int ZedIn::frameNumber(void) const
{
	return lockedFrameNumber_;
}


// Seek to a given frame number. This is possible if the
// input is a video. If reading live camera data it will
// fail, but nothing we can do about that so fail silently
void ZedIn::frameNumber(int frameNumber)
{
	if (zed_ && zed_->setSVOPosition(frameNumber))
		frameNumber_ = frameNumber;
}


int ZedIn::width(void) const
{
	return width_;
}


int ZedIn::height(void) const
{
	return height_;
}


CameraParams ZedIn::getCameraParams(bool left) const
{
	stringstream camera_id;
	camera_id << "ZED";
	int actual_w = zed_ ? zed_->getImageSize().width : width_;
	int actual_h = zed_ ? zed_->getImageSize().height : height_;
	camera_id << actual_w << "x" << actual_h;

	if(left)
		camera_id << "left";
	else
		camera_id << "right";

	if(zed_)
		camera_id << zed_->getZEDSerial();
	else
		camera_id << "2151"; //assume 2151 because currently we don't store which camera it was with the archive

	CameraParams params;
	cout << endl << "Reading parameter file params.xml: " << endl;
	FileStorage fs;
	fs.open("params.xml", FileStorage::READ);
	fs[camera_id.str()] >> params;
	return params;
}


void zedBrightnessCallback(int value, void *data)
{
    ZedIn *zedPtr = (ZedIn *)data;
	zedPtr->brightness_ = value;
	if (zedPtr->zed_)
	{
		zedPtr->zed_->setCameraSettingsValue(sl::zed::ZED_BRIGHTNESS, value - 1, value == 0);
	}
}


void zedContrastCallback(int value, void *data)
{
    ZedIn *zedPtr = (ZedIn *)data;
	zedPtr->contrast_ = value;
	if (zedPtr->zed_)
	{
		zedPtr->zed_->setCameraSettingsValue(sl::zed::ZED_CONTRAST, value - 1, value == 0);
	}
}


void zedHueCallback(int value, void *data)
{
    ZedIn *zedPtr = (ZedIn *)data;
	zedPtr->hue_ = value;
	if (zedPtr->zed_)
	{
		zedPtr->zed_->setCameraSettingsValue(sl::zed::ZED_HUE, value - 1, value == 0);
	}
}


void zedSaturationCallback(int value, void *data)
{
    ZedIn *zedPtr = (ZedIn *)data;
	zedPtr->saturation_ = value;
	if (zedPtr->zed_)
	{
		zedPtr->zed_->setCameraSettingsValue(sl::zed::ZED_SATURATION, value - 1, value == 0);
	}
}


void zedGainCallback(int value, void *data)
{
    ZedIn *zedPtr = (ZedIn *)data;
	zedPtr->gain_ = value;
	if (zedPtr->zed_)
	{
		zedPtr->zed_->setCameraSettingsValue(sl::zed::ZED_GAIN, value - 1, value == 0);
	}
}


void zedWhiteBalanceCallback(int value, void *data)
{
    ZedIn *zedPtr = (ZedIn *)data;
	zedPtr->whiteBalance_ = value;
	if (zedPtr->zed_)
	{
		zedPtr->zed_->setCameraSettingsValue(sl::zed::ZED_WHITEBALANCE, value - 1, value == 0);
	}
}


#else

ZedIn::~ZedIn()
{
}


int ZedIn::width(void) const
{
	return 0;
}


int ZedIn::height(void) const
{
	return 0;
}


ZedIn::ZedIn(const char *filename, const char *outputName)
{
	(void)filename;
	cerr << "Zed support not compiled in" << endl;
}


#endif
