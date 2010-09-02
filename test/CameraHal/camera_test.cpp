#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>

#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/ISurfaceFlingerClient.h>
#include <ui/Overlay.h>
#include <surfaceflinger/SurfaceComposerClient.h>

#include <CameraHardwareInterface.h>
#include <camera/Camera.h>
#include <camera/ICamera.h>
#include <media/mediarecorder.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>

#define PRINTOVER(arg...)     LOGD(#arg)
#define LOG_FUNCTION_NAME         LOGD("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    LOGD("%d: %s() EXIT", __LINE__, __FUNCTION__);
#define KEY_POST_PROC   "ipp"
#define KEY_CAMERA          "camera-index"
#define KEY_SATURATION      "saturation"
#define KEY_BRIGHTNESS      "brightness"
#define KEY_BURST           "burst-capture"
#define KEY_EXPOSURE        "exposure"
#define KEY_CONTRAST        "contrast"
#define KEY_SHARPNESS       "sharpness"
#define KEY_ISO             "iso"
#define KEY_CAF             "caf"
#define KEY_MODE            "mode"
#define KEY_VNF             "vnf"
#define KEY_VSTAB           "vstab"
#define KEY_COMPENSATION    "exposure-compensation"
#define KEY_IPP             "ippMode"
#define KEY_BUFF_STARV      "buff-starvation"
#define KEY_METERING_MODE   "meter-mode"

#define SDCARD_PATH "/sdcard/"

#define MAX_BURST   15
#define BURST_INC     5

#define MEMORY_DUMP "procrank -u"
#define KEY_METERING_MODE   "meter-mode"

#define COMPENSATION_OFFSET 20
#define DELIMITER           "|"

#define MAX_PREVIEW_SURFACE_WIDTH   800
#define MAX_PREVIEW_SURFACE_HEIGHT  480

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

using namespace android;

int camera_index = 0;
int print_menu;
sp<Camera> camera;
sp<MediaRecorder> recorder;
sp<SurfaceComposerClient> client;
sp<SurfaceControl> overlayControl;
sp<ISurface> overlaySurface;
CameraParameters params;
float compensation = 0.0;
double latitude = 0.0;
double longitude = 0.0;
double degree_by_step = 17.5609756;//..0975609756097;
double altitude = 0.0;
int awb_mode = 0;
int effects_mode = 0;
int scene_mode = 0;
int caf_mode = 0;
int vnf_mode = 0;
int vstab_mode = 0;

int postProcIDX = 0;
int rotation = 0;
bool reSizePreview = true;
bool hardwareActive = false;
bool recordingMode = false;
bool previewRunning = false;
int saturation = 0;
int zoomIDX = 0;
int videoCodecIDX = 0;
int audioCodecIDX = 0;
int outputFormatIDX = 0;
int contrast = 0;
int brightness = 0;
unsigned int burst = 0;
int sharpness = 0;
int iso_mode = 0;
int capture_mode = 0;
int exposure_mode = 0;
int ippIDX = 0;
int previewFormat = 0;
int jpegQuality = 85;
int thumbQuality = 85;
timeval autofocus_start, picture_start;
char script_name[25];
bool nullOverlay = false;
int prevcnt = 0;

char dir_path[40] = SDCARD_PATH;

const char *cameras[] = {"Primary Camera", "Secondary Camera 1", "Secondary Camera 2", "Stereo Camera"};

const char *ipp_mode[] = { "off", "Chroma Suppression", "Edge Enhancement" };
const char *iso [] = { "auto", "100", "200", "400", "800", "1200", "1600"};
const char *effects [] = {
    "none",
    "mono",
    "negative",
    "solarize",
    "sepia",
    "whiteboard",
    "blackboard",
    "cool",
    "emboss",
};
const char *caf [] = { "Off", "On" };
const char *vnf [] = { "Off", "On" };
const char *vstab [] = { "Off", "On" };


const char *scene [] = {
    "auto",
    "portrait",
    "landscape",
    "night",
    "night-portrait",
    "fireworks",

};
const char *strawb_mode[] = {
    "auto",
    "incandescent",
    "fluorescent",
    "daylight",
    "horizon",
    "shadow",
};
const char *antibanding[] = {
    "off",
    "auto",
    "50hz",
    "60hz",
};
int antibanding_mode = 0;
const char *focus[] = {
    "auto",
    "infinity",
    "macro",
};
int focus_mode = 0;
const char *pixelformat[] = {"yuv422i-yuyv", "yuv420sp", "rgb565", "jpeg", "raw"};
const char *post_proc[] = {"off", "nsf", "ldc", "ldc-nsf"};
int pictureFormat = ARRAY_SIZE(pixelformat) - 2;
const char *exposure[] = {"auto", "macro", "portrait", "landscape", "sports", "night", "night-portrait", "backlighting", "manual"};
const char *capture[] = { "high-performance", "high-quality", "video-mode" };
const struct {
    int idx;
    const char *zoom_description;
} zoom [] = {
    { 0,  "1x"},
    { 12, "1.5x"},
    { 20, "2x"},
    { 27, "2.5x"},
    { 32, "3x"},
    { 36, "3.5x"},
    { 40, "4x"},
    { 60, "8x"},
};

const struct {
    video_encoder type;
    const char *desc;
} videoCodecs[] = {
    { VIDEO_ENCODER_H263, "H263" },
    { VIDEO_ENCODER_H264, "H264" },
    { VIDEO_ENCODER_MPEG_4_SP, "MPEG4"}
};

const struct { audio_encoder type; const char *desc; } audioCodecs[] = {
    { AUDIO_ENCODER_AMR_NB, "AMR_NB" },
    { AUDIO_ENCODER_AMR_WB, "AMR_WB" },
    { AUDIO_ENCODER_AAC, "AAC" },
    { AUDIO_ENCODER_AAC_PLUS, "AAC+" },
    { AUDIO_ENCODER_EAAC_PLUS, "EAAC+" },
};

const struct { output_format type; const char *desc; } outputFormat[] = {
    { OUTPUT_FORMAT_THREE_GPP, "3gp" },
    { OUTPUT_FORMAT_MPEG_4, "mp4" },
};

const struct {
    int width, height;
    const char *desc;
} previewSize[] = {
    { 128, 96, "SQCIF" },
    { 176, 144, "QCIF" },
    { 352, 288, "CIF" },
    { 320, 240, "QVGA" },
    { 352, 288, "CIF" },
    { 640, 480, "VGA" },
    { 720, 480, "NTSC" },
    { 720, 576, "PAL" },
    { 800, 480, "WVGA" },
    { 848, 480, "WVGA2"},
    { 864, 480, "WVGA3"},
    { 992, 560, "WVGA4"},
    { 1280, 720, "HD" },
    { 1920, 1080, "FULLHD"},
};

const struct {
    int width, height;
    const char *desc;
} VcaptureSize[] = {
    { 128, 96, "SQCIF" },
    { 176, 144, "QCIF" },
    { 352, 288, "CIF" },
    { 320, 240, "QVGA" },
    { 640, 480, "VGA" },
    { 704, 480, "TVNTSC" },
    { 704, 576, "TVPAL" },
    { 720, 480, "D1NTSC" },
    { 720, 576, "D1PAL" },
    { 800, 480, "WVGA" },
    { 1280, 720, "HD" },
    { 1920, 1080, "FULLHD"},
};

const struct {
    int width, height;
    const char *name;
} captureSize[] = {
    {  320, 240,  "QVGA" },
    {  640, 480,  "VGA" },
    {  800, 600,  "SVGA" },
    { 1280, 960,  "1MP" },
    { 1280, 1024, "1.3MP" },
    { 1600, 1200,  "2MP" },
    { 2048, 1536,  "3MP" },
    { 2560, 2048,  "5MP" },
    { 3264, 2448,  "8MP" },
    { 3648, 2736, "10MP"},
    { 4000, 3000, "12MP"},
};

const struct {
    int fps;
} frameRate[] = {
    {0},
    {5},
    {10},
    {15},
    {20},
    {25},
    {30}
};

const struct {
    uint32_t bit_rate;
    const char *desc;
} VbitRate[] = {
    {    64000, "64K"  },
    {   128000, "128K" },
    {   192000, "192K" },
    {   240000, "240K" },
    {   320000, "320K" },
    {   360000, "360K" },
    {   384000, "384K" },
    {   420000, "420K" },
    {   768000, "768K" },
    {  1000000, "1M"   },
    {  1500000, "1.5M" },
    {  2000000, "2M"   },
    {  4000000, "4M"   },
    {  6000000, "6M"   },
    {  8000000, "8M"   },
    { 10000000, "10M"  },

};

int thumbSizeIDX =  ARRAY_SIZE(previewSize) - 1;
int previewSizeIDX = ARRAY_SIZE(previewSize) - 1;
int captureSizeIDX = ARRAY_SIZE(captureSize) - 1;
int frameRateIDX = ARRAY_SIZE(frameRate) - 1;
int VcaptureSizeIDX = ARRAY_SIZE(VcaptureSize) - 1;
int VbitRateIDX = ARRAY_SIZE(VbitRate) - 1;

static unsigned int recording_counter = 1;

int dump_preview = 0;
int bufferStarvationTest = 0;
bool showfps = false;

const char *metering[] = {
    "center",
    "average",
};
int meter_mode = 0;

//forward declarations
int closeCamera();

namespace android {

    class Test {
        public:
            static const sp<ISurface>& getISurface(const sp<SurfaceControl>& s) {
                return s->getISurface();
            }
    };

};

class CameraHandler: public CameraListener {
    public:
        virtual void notify(int32_t msgType, int32_t ext1, int32_t ext2);
        virtual void postData(int32_t msgType, const sp<IMemory>& dataPtr);
#ifdef OMAP_ENHANCEMENT
        virtual void postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr
                , uint32_t offset, uint32_t stride);
#else
        virtual void postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr);
#endif
};

/** Calculate delay from a reference time */
unsigned long long timeval_delay(const timeval *ref) {
    unsigned long long st, end, delay;
    timeval current_time;

    gettimeofday(&current_time, 0);

    st = ref->tv_sec * 1000000 + ref->tv_usec;
    end = current_time.tv_sec * 1000000 + current_time.tv_usec;
    delay = end - st;

    return delay;
}

/** Callback for takePicture() */
void my_raw_callback(const sp<IMemory>& mem) {

    static int      counter = 1;
    unsigned char   *buff = NULL;
    int             size;
    int             fd = -1;
    char            fn[256];

    LOG_FUNCTION_NAME

    if (mem == NULL)
        goto out;

    fn[0] = 0;
    sprintf(fn, "/sdcard/img%03d.raw", counter);
    fd = open(fn, O_CREAT | O_WRONLY | O_SYNC | O_TRUNC, 0777);

    if (fd < 0)
        goto out;

    size = mem->size();

    if (size <= 0)
        goto out;

    buff = (unsigned char *)mem->pointer();

    if (!buff)
        goto out;

    if (size != write(fd, buff, size))
        printf("Bad Write int a %s error (%d)%s\n", fn, errno, strerror(errno));

    counter++;
    printf("%s: buffer=%08X, size=%d stored at %s\n",
           __FUNCTION__, (int)buff, size, fn);

out:

    if (fd >= 0)
        close(fd);

    LOG_FUNCTION_NAME_EXIT
}

void saveFile(const sp<IMemory>& mem) {
    static int      counter = 1;
    unsigned char   *buff = NULL;
    int             size;
    int             fd = -1;
    char            fn[256];

    LOG_FUNCTION_NAME

    if (mem == NULL)
        goto out;

    fn[0] = 0;
    sprintf(fn, "/preview%03d.yuv", counter);
    fd = open(fn, O_CREAT | O_WRONLY | O_SYNC | O_TRUNC, 0777);
    if(fd < 0) {
        LOGE("Unable to open file %s: %s", fn, strerror(fd));
        goto out;
    }

    size = mem->size();
    if (size <= 0) {
        LOGE("IMemory object is of zero size");
        goto out;
    }

    buff = (unsigned char *)mem->pointer();
    if (!buff) {
        LOGE("Buffer pointer is invalid");
        goto out;
    }

    if (size != write(fd, buff, size))
        printf("Bad Write int a %s error (%d)%s\n", fn, errno, strerror(errno));

    counter++;
    printf("%s: buffer=%08X, size=%d\n",
           __FUNCTION__, (int)buff, size);

out:

    if (fd >= 0)
        close(fd);

    LOG_FUNCTION_NAME_EXIT
}

void printSupportedParams()
{
    printf("\n\r\tSupported Cameras: %s", params.get("camera-indexes"));
    printf("\n\r\tSupported Picture Sizes: %s", params.get(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES));
    printf("\n\r\tSupported Picture Formats: %s", params.get(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS));
    printf("\n\r\tSupported Preview Sizes: %s", params.get(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES));
    printf("\n\r\tSupported Preview Formats: %s", params.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS));
    printf("\n\r\tSupported Preview Frame Rates: %s", params.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES));
    printf("\n\r\tSupported Thumbnail Sizes: %s", params.get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES));
    printf("\n\r\tSupported Whitebalance Modes: %s", params.get(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE));
    printf("\n\r\tSupported Effects: %s", params.get(CameraParameters::KEY_SUPPORTED_EFFECTS));
    printf("\n\r\tSupported Scene Modes: %s", params.get(CameraParameters::KEY_SUPPORTED_SCENE_MODES));
    printf("\n\r\tSupported Focus Modes: %s", params.get(CameraParameters::KEY_SUPPORTED_FOCUS_MODES));
    printf("\n\r\tSupported Antibanding Options: %s", params.get(CameraParameters::KEY_SUPPORTED_ANTIBANDING));
    printf("\n\r\tSupported Flash Modes: %s", params.get(CameraParameters::KEY_SUPPORTED_FLASH_MODES));

    return;
}

void debugShowFPS()
{
    static int mFrameCount = 0;
    static int mLastFrameCount = 0;
    static nsecs_t mLastFpsTime = 0;
    static float mFps = 0;
    mFrameCount++;
    if ( ( mFrameCount % 30 ) == 0 ) {
        nsecs_t now = systemTime();
        nsecs_t diff = now - mLastFpsTime;
        mFps =  ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
        printf("####### [%d] Frames, %f FPS", mFrameCount, mFps);
    }
}

/** Callback for startPreview() */
void my_preview_callback(const sp<IMemory>& mem) {

    printf("PREVIEW Callback 0x%x", ( unsigned int ) mem->pointer());
    if (dump_preview) {

        if(prevcnt==50)
        saveFile(mem);

        prevcnt++;

        uint8_t *ptr = (uint8_t*) mem->pointer();

        printf("PRV_CB: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8], ptr[9]);

    }

    debugShowFPS();
}

/** Callback for takePicture() */
void my_jpeg_callback(const sp<IMemory>& mem) {
    static int  counter = 1;
    unsigned char   *buff = NULL;
    int     size;
    int     fd = -1;
    char        fn[256];

    LOG_FUNCTION_NAME

    if (mem == NULL)
        goto out;

    fn[0] = 0;
    sprintf(fn, "%s/img%03d.jpg", dir_path,counter);
    fd = open(fn, O_CREAT | O_WRONLY | O_SYNC | O_TRUNC, 0777);

    if(fd < 0) {
        LOGE("Unable to open file %s: %s", fn, strerror(fd));
        goto out;
    }

    size = mem->size();
    if (size <= 0) {
        LOGE("IMemory object is of zero size");
        goto out;
    }

    buff = (unsigned char *)mem->pointer();
    if (!buff) {
        LOGE("Buffer pointer is invalid");
        goto out;
    }

    if (size != write(fd, buff, size))
        printf("Bad Write int a %s error (%d)%s\n", fn, errno, strerror(errno));

    counter++;
    printf("%s: buffer=%08X, size=%d stored at %s\n",
           __FUNCTION__, (int)buff, size, fn);

out:

    if (fd >= 0)
        close(fd);

    LOG_FUNCTION_NAME_EXIT
}

void CameraHandler::notify(int32_t msgType, int32_t ext1, int32_t ext2) {

    printf("Notify cb: %d %d %d\n", msgType, ext1, ext2);

    if ( msgType & CAMERA_MSG_FOCUS )
        printf("AutoFocus %s in %llu us\n", (ext1) ? "OK" : "FAIL", timeval_delay(&autofocus_start));

    if ( msgType & CAMERA_MSG_SHUTTER )
        printf("Shutter done in %llu us\n", timeval_delay(&picture_start));

}

void CameraHandler::postData(int32_t msgType, const sp<IMemory>& dataPtr) {
    printf("Data cb: %d\n", msgType);

    if ( msgType & CAMERA_MSG_PREVIEW_FRAME )
        my_preview_callback(dataPtr);

    if ( msgType & CAMERA_MSG_RAW_IMAGE ) {
        printf("RAW done in %llu us\n", timeval_delay(&picture_start));
        my_raw_callback(dataPtr);
    }

    if (msgType & CAMERA_MSG_POSTVIEW_FRAME) {
        printf("Postview frame %llu us\n", timeval_delay(&picture_start));
    }

    if (msgType & CAMERA_MSG_COMPRESSED_IMAGE ) {
        printf("JPEG done in %llu us\n", timeval_delay(&picture_start));
        my_jpeg_callback(dataPtr);
    }
}

#ifdef OMAP_ENHANCEMENT
void CameraHandler::postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr
                                    , uint32_t offset, uint32_t stride)
#else
void CameraHandler::postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr)
#endif
{
#ifdef OMAP_ENHANCEMENT
    printf("Recording cb: %d %lld %p Offset:%d Stride:%d\n", msgType, timestamp, dataPtr.get(), offset, stride);
#else
    printf("Recording cb: %d %lld %p Offset:%d Stride:%d\n", msgType, timestamp, dataPtr.get());
#endif

    static uint32_t count = 0;

    //if(count==100)
    //saveFile(dataPtr);

    count++;

    uint8_t *ptr = (uint8_t*) dataPtr->pointer();

    printf("VID_CB: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8], ptr[9]);

    camera->releaseRecordingFrame(dataPtr);
}

int createPreviewSurface(unsigned int width, unsigned int height) {
    unsigned int previewWidth, previewHeight;

    if ( MAX_PREVIEW_SURFACE_WIDTH < width ) {
        previewWidth = MAX_PREVIEW_SURFACE_WIDTH;
    } else {
        previewWidth = width;
    }

    if ( MAX_PREVIEW_SURFACE_HEIGHT < height ) {
        previewHeight = MAX_PREVIEW_SURFACE_HEIGHT;
    } else {
        previewHeight = height;
    }

    client = new SurfaceComposerClient();

    if ( NULL == client.get() ) {
        printf("Unable to establish connection to Surface Composer \n");

        return -1;
    }

    overlayControl = client->createSurface(getpid(), 0, previewWidth, previewHeight,
                                           PIXEL_FORMAT_UNKNOWN, ISurfaceComposer::ePushBuffers);

    if ( NULL == overlayControl.get() ) {
        printf("Unable to create Overlay control surface\n");

        return -1;
    }

    overlaySurface = Test::getISurface(overlayControl);

    if ( NULL == overlaySurface.get() ) {
        printf("Unable to get overlay ISurface interface\n");

        return -1;
    }

    client->openTransaction();
    overlayControl->setLayer(100000);
    client->closeTransaction();

    client->openTransaction();
    overlayControl->setPosition(0, 0);
    overlayControl->setSize(previewWidth, previewHeight);
    client->closeTransaction();

    return 0;
}

int destroyPreviewSurface() {

    if ( NULL != overlaySurface.get() ) {
        overlaySurface.clear();
    }

    if ( NULL != overlayControl.get() ) {
        overlayControl->clear();
        overlayControl.clear();
    }

    if ( NULL != client.get() ) {
        client->dispose();
        client.clear();
    }

    return 0;
}

int openRecorder() {
    recorder = new MediaRecorder();

    if ( NULL == recorder.get() ) {
        printf("Error while creating MediaRecorder\n");

        return -1;
    }

    return 0;
}

int closeRecorder() {
    if ( NULL == recorder.get() ) {
        printf("invalid recorder reference\n");

        return -1;
    }

    if ( recorder->init() < 0 ) {
        printf("recorder failed to initialize\n");

        return -1;
    }

    if ( recorder->close() < 0 ) {
        printf("recorder failed to close\n");

        return -1;
    }

    if ( recorder->release() < 0 ) {
        printf("error while releasing recorder\n");

        return -1;
    }

    recorder.clear();

    return 0;
}

int configureRecorder() {

    char videoFile[256],vbit_string[50];
    int fd = -1;

    if ( ( NULL == recorder.get() ) || ( NULL == camera.get() ) ) {
        printf("invalid recorder and/or camera references\n");

        return -1;
    }

    camera->unlock();

    sprintf(vbit_string,"video-param-encoding-bitrate=%u", VbitRate[VbitRateIDX].bit_rate);
    String8 bit_rate(vbit_string);
    if ( recorder->setParameters(bit_rate) < 0 ) {
        printf("error while configuring bit rate\n");

        return -1;
    }

    if ( recorder->setCamera(camera->remote()) < 0 ) {
        printf("error while setting the camera\n");

        return -1;
    }

    if ( recorder->setVideoSource(VIDEO_SOURCE_CAMERA) < 0 ) {
        printf("error while configuring camera video source\n");

        return -1;
    }


    if ( recorder->setAudioSource(AUDIO_SOURCE_MIC) < 0 ) {
        printf("error while configuring camera audio source\n");

        return -1;
    }

    if ( recorder->setOutputFormat(outputFormat[outputFormatIDX].type) < 0 ) {
        printf("error while configuring output format\n");

        return -1;
    }

    if(mkdir("/data/videos",0777) == -1)
         printf("\n Directory --videos-- was not created \n");
    sprintf(videoFile, "/data/videos/video%d.%s", recording_counter,outputFormat[outputFormatIDX].desc);

    fd = open(videoFile, O_CREAT | O_WRONLY | O_SYNC | O_TRUNC, 0777);

    if(fd < 0){
        printf("Error while creating video filename\n");

        return -1;
    }

    if ( recorder->setOutputFile(videoFile) < 0 ) {
        printf("error while configuring video filename\n");

        return -1;
    }

    recording_counter++;

    if ( recorder->setVideoFrameRate(frameRate[frameRateIDX].fps) < 0 ) {
        printf("error while configuring video framerate\n");

        return -1;
    }

    if ( recorder->setVideoSize(VcaptureSize[VcaptureSizeIDX].width, VcaptureSize[VcaptureSizeIDX].height) < 0 ) {
        printf("error while configuring video size\n");

        return -1;
    }

    if ( recorder->setVideoEncoder(videoCodecs[videoCodecIDX].type) < 0 ) {
        printf("error while configuring video codec\n");

        return -1;
    }

    if ( recorder->setAudioEncoder(audioCodecs[audioCodecIDX].type) < 0 ) {
        printf("error while configuring audio codec\n");

        return -1;
    }

    if ( recorder->setPreviewSurface( overlayControl->getSurface() ) < 0 ) {
        printf("error while configuring preview surface\n");

        return -1;
    }

    return 0;
}

int startRecording() {
    if ( ( NULL == recorder.get() ) || ( NULL == camera.get() ) ) {
        printf("invalid recorder and/or camera references\n");

        return -1;
    }

    camera->unlock();

    if ( recorder->prepare() < 0 ) {
        printf("recorder prepare failed\n");

        return -1;
    }

    if ( recorder->start() < 0 ) {
        printf("recorder start failed\n");

        return -1;
    }

    return 0;
}

int stopRecording() {
    if ( NULL == recorder.get() ) {
        printf("invalid recorder reference\n");

        return -1;
    }

    if ( recorder->stop() < 0 ) {
        printf("recorder failed to stop\n");

        return -1;
    }

    return 0;
}

int openCamera() {

    camera = Camera::connect();

    if ( NULL == camera.get() ) {
        printf("Unable to connect to CameraService\n");

        return -1;
    }

    camera->setListener(new CameraHandler());

    return 0;
}

int closeCamera() {
    if ( NULL == camera.get() ) {
        printf("invalid camera reference\n");

        return -1;
    }

    camera->disconnect();
    camera.clear();

    return 0;
}

int startPreview() {
    int previewWidth, previewHeight;
    if (reSizePreview) {

        if(recordingMode)
        {
            previewWidth = VcaptureSize[VcaptureSizeIDX].width;
            previewHeight = VcaptureSize[VcaptureSizeIDX].height;
        }else
        {
            previewWidth = previewSize[previewSizeIDX].width;
            previewHeight = previewSize[previewSizeIDX].height;
        }

        if(!nullOverlay) {
            if ( createPreviewSurface(previewWidth, previewHeight ) < 0 ) {
                printf("Error while creating preview surface\n");
                return -1;
            }
        } else {
            overlaySurface = NULL;
        }

        if(!hardwareActive)
        {
            if ( openCamera() < 0 ) {
                printf("Camera initialization failed\n");

                return -1;
            }

        }

        params.setPreviewSize(previewWidth, previewHeight);
        params.setPictureSize(captureSize[captureSizeIDX].width, captureSize[captureSizeIDX].height);

        camera->setParameters(params.flatten());
        camera->setPreviewDisplay(overlaySurface);

        if(!hardwareActive) prevcnt = 0;

        camera->startPreview();

        previewRunning = true;
        reSizePreview = false;
        hardwareActive = true;
    }

    return 0;
}

void stopPreview() {
    if ( hardwareActive ) {
        camera->stopPreview();
        closeCamera();
        if(!nullOverlay)
        destroyPreviewSurface();

        previewRunning  = false;
        reSizePreview = true;
        hardwareActive = false;
    }
}

void initDefaults() {
    camera_index = 0;
    antibanding_mode = 0;
    focus_mode = 0;
    previewSizeIDX = ARRAY_SIZE(previewSize) - 6;  /* Default resolution set to WVGA */
    captureSizeIDX = ARRAY_SIZE(captureSize) - 3;  /* Default capture resolution is 8MP */
    frameRateIDX = ARRAY_SIZE(frameRate) - 1;      /* Default frame rate is 30 FPS */
    VcaptureSizeIDX = ARRAY_SIZE(VcaptureSize) - 2;/* Default video record is WVGA */
    VbitRateIDX = ARRAY_SIZE(VbitRate) - 4;        /*Default video bit rate is 4M */
    compensation = 0.0;
    awb_mode = 0;
    effects_mode = 0;
    scene_mode = 0;
    caf_mode = 0;
    vnf_mode = 0;
    vstab_mode = 0;
    rotation = 0;
    zoomIDX = 0;
    videoCodecIDX = 0;
    postProcIDX = 0;
#ifdef TARGET_OMAP4
    ///Temporary fix until OMAP3 and OMAP4 3A values are synced
    contrast = 90;
    brightness = 50;
    sharpness = 0;
    saturation = 50;
#else
    contrast = 0;
    brightness = 100;
    sharpness = 0;
    saturation = 0;
#endif
    iso_mode = 0;
    capture_mode = 1;
    exposure_mode = 0;
    ippIDX = 0;
    jpegQuality = 85;
    bufferStarvationTest = 0;
    meter_mode = 0;
    previewFormat = 0;
    pictureFormat = ARRAY_SIZE(pixelformat) - 2;
    params.setPreviewSize(previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height);
    params.setPictureSize(captureSize[captureSizeIDX].width, captureSize[captureSizeIDX].height);
    params.set(CameraParameters::KEY_ROTATION, rotation);
    params.set(KEY_COMPENSATION, (int) (compensation * 10));
    params.set(params.KEY_WHITE_BALANCE, strawb_mode[awb_mode]);
    params.set(KEY_MODE, (capture[capture_mode]));
    params.set(params.KEY_SCENE_MODE, scene[scene_mode]);
    params.set(KEY_CAF, caf_mode);
    params.set(KEY_ISO, iso_mode);
    params.set(KEY_POST_PROC, post_proc[postProcIDX]);
    params.set(KEY_SHARPNESS, sharpness);
    params.set(KEY_CONTRAST, contrast);
    params.set(CameraParameters::KEY_ZOOM, zoom[zoomIDX].idx);
    params.set(KEY_EXPOSURE, exposure[exposure_mode]);
    params.set(KEY_BRIGHTNESS, brightness);
    params.set(KEY_SATURATION, saturation);
    params.set(params.KEY_EFFECT, effects[effects_mode]);
    params.setPreviewFrameRate(frameRate[frameRateIDX].fps);
    params.set(params.KEY_ANTIBANDING, antibanding[antibanding_mode]);
    params.set(params.KEY_FOCUS_MODE, focus[focus_mode]);
    params.set(KEY_IPP, ippIDX);
    params.set(CameraParameters::KEY_JPEG_QUALITY, jpegQuality);
    params.setPreviewFormat(pixelformat[previewFormat]);
    params.setPictureFormat(pixelformat[pictureFormat]);
    params.set(KEY_BUFF_STARV, bufferStarvationTest); //enable buffer starvation
    params.set(KEY_METERING_MODE, metering[meter_mode]);
}

int menu_gps() {
    char ch;
    char coord_str[100];

    if (print_menu) {
        printf("\n\n== GPS MENU ============================\n\n");
        printf("   e. Latitude:       %.7lf\n", latitude);
        printf("   d. Longitude:      %.7lf\n", longitude);
        printf("   c. Altitude:       %.7lf\n", altitude);
        printf("\n");
        printf("   q. Return to main menu\n");
        printf("\n");
        printf("   Choice: ");
    }

    ch = getchar();
    printf("%c", ch);

    print_menu = 1;

    switch (ch) {

        case 'e':
            latitude += degree_by_step;

            if (latitude > 90.0) {
                latitude -= 180.0;
            }

            snprintf(coord_str, 7, "%.7lf", latitude);
            params.set(params.KEY_GPS_LATITUDE, coord_str);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'd':
            longitude += degree_by_step;

            if (longitude > 180.0) {
                longitude -= 360.0;
            }

            snprintf(coord_str, 7, "%.7lf", longitude);
            params.set(params.KEY_GPS_LONGITUDE, coord_str);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'c':
            altitude += 12345.67890123456789;

            if (altitude > 100000.0) {
                altitude -= 200000.0;
            }

            snprintf(coord_str, 100, "%.20lf", altitude);
            params.set(params.KEY_GPS_ALTITUDE, coord_str);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'Q':
        case 'q':
            return -1;

        default:
            print_menu = 0;
            break;
    }

    return 0;
}

int functional_menu() {
    char ch;

    if (print_menu) {

        printf("\n\n=========== FUNCTIONAL TEST MENU ===================\n\n");

        printf(" \n\nSTART / STOP / GENERAL SERVICES \n");
        printf(" -----------------------------\n");
        printf("   A  Select Camera %s\n", cameras[camera_index]);
        printf("   [. Resume Preview after capture\n");
        printf("   0. Reset to defaults\n");
        printf("   q. Quit\n");
        printf("   @. Disconnect and Reconnect to CameraService \n");
        printf("   /. Enable/Disable showfps: %s\n", ((showfps)? "Enabled":"Disabled"));
        printf("   a. GEO tagging settings menu\n");
        printf("   E.  Camera Capability Dump");


        printf(" \n\n PREVIEW SUB MENU \n");
        printf(" -----------------------------\n");
        printf("   1. Start Preview\n");
        printf("   2. Stop Preview\n");
        printf("   ~. Preview format %s\n", pixelformat[previewFormat]);
        printf("   4. Preview size:   %4d x %4d - %s\n",previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height, previewSize[previewSizeIDX].desc);
        printf("   &. Dump a preview frame\n");

        printf(" \n\n IMAGE CAPTURE SUB MENU \n");
        printf(" -----------------------------\n");
        printf("   p. Take picture\n");
        printf("   $. Picture Format: %s\n", pixelformat[pictureFormat]);
        printf("   3. Picture Rotation:       %3d degree\n", rotation );
        printf("   5. Picture size:   %4d x %4d - %s\n",captureSize[captureSizeIDX].width, captureSize[captureSizeIDX].height,              captureSize[captureSizeIDX].name);
        printf("   i. ISO mode:       %s\n", iso[iso_mode]);
        printf("   u. Capture Mode:   %s\n", capture[capture_mode]);
        printf("   k. IPP Mode:       %s\n", ipp_mode[ippIDX]);
        printf("   K. Post-Processing: %s\n", post_proc[postProcIDX]);
        printf("   o. Jpeg Quality:   %d\n", jpegQuality);
        printf("   #. Burst Images:  %3d\n", burst);
        printf("   :. Thumbnail Size:  %4d x %4d - %s\n",previewSize[thumbSizeIDX].width, previewSize[thumbSizeIDX].height, previewSize[thumbSizeIDX].desc);
        printf("   ': Thumbnail Quality %d\n", thumbQuality);

        printf(" \n\n VIDEO CAPTURE SUB MENU \n");
        printf(" -----------------------------\n");

        printf("   6. Start Video Recording\n");
        printf("   2. Stop Recording\n");
        printf("   l. Video Capture resolution:   %4d x %4d - %s\n",VcaptureSize[VcaptureSizeIDX].width,VcaptureSize[VcaptureSizeIDX].height, VcaptureSize[VcaptureSizeIDX].desc);
        printf("   ]. Video Bit rate :  %s\n", VbitRate[VbitRateIDX].desc);
        printf("   9. Video Codec:    %s\n", videoCodecs[videoCodecIDX].desc);
        printf("   d. Audio Codec:    %s\n", audioCodecs[audioCodecIDX].desc);
        printf("   v. Output Format:  %s\n", outputFormat[outputFormatIDX].desc);
        printf("   r. Framerate:     %3d\n", frameRate[frameRateIDX].fps);
        printf("   *. Start Video Recording dump ( 1 raw frame ) \n");
        printf("   B  VNF              %s \n", vnf[vnf_mode]);
        printf("   C  VSTAB              %s", vstab[vstab_mode]);

        printf(" \n\n 3A SETTING SUB MENU \n");
        printf(" -----------------------------\n");

        printf("   f. Auto Focus\n");
        printf("   7. EV offset:      %4.1f\n", compensation);
        printf("   8. AWB mode:       %s\n", strawb_mode[awb_mode]);
        printf("   z. Zoom            %s\n", zoom[zoomIDX].zoom_description);
        printf("   j. Exposure        %s\n", exposure[exposure_mode]);
        printf("   e. Effect:         %s\n", effects[effects_mode]);
        printf("   w. Scene:          %s\n", scene[scene_mode]);
        printf("   s. Saturation:     %d\n", saturation);
        printf("   c. Contrast:       %d\n", contrast);
        printf("   h. Sharpness:      %d\n", sharpness);
        printf("   b. Brightness:     %d\n", brightness);
        printf("   y. Continuous AF:  %s\n", caf[caf_mode]);
        printf("   x. Antibanding:    %s\n", antibanding[antibanding_mode]);
        printf("   g. Focus mode:     %s\n", focus[focus_mode]);
        printf("   m. Metering mode:     %s\n", metering[meter_mode]);
        printf("\n");
        printf("   Choice: ");
    }

    ch = getchar();
    printf("%c", ch);

    print_menu = 1;

    switch (ch) {

    case 'A':
        camera_index++;
        camera_index %= ARRAY_SIZE(cameras);
        params.set(KEY_CAMERA, camera_index);

        if ( hardwareActive )
            camera->setParameters(params.flatten());
        break;
    case '[':
        if ( hardwareActive ) {
            camera->setParameters(params.flatten());
            camera->startPreview();
        }
        break;

    case '0':
        initDefaults();
        break;

        case '1':

            if ( startPreview() < 0 ) {
                printf("Error while starting preview\n");

                return -1;
            }

            break;

        case '2':
            stopPreview();

            if ( recordingMode ) {
                stopRecording();
                closeRecorder();

                recordingMode = false;
            }

            break;

        case '3':
            rotation += 90;
            rotation %= 360;
            params.set(CameraParameters::KEY_ROTATION, rotation);
            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case '4':
            previewSizeIDX += 1;
            previewSizeIDX %= ARRAY_SIZE(previewSize);
            params.setPreviewSize(previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height);
            reSizePreview = true;

            if ( hardwareActive && previewRunning ) {
                camera->stopPreview();
                camera->setParameters(params.flatten());
                camera->startPreview();
            } else if ( hardwareActive ) {
                camera->setParameters(params.flatten());
            }

            break;

        case '5':
            captureSizeIDX += 1;
            captureSizeIDX %= ARRAY_SIZE(captureSize);
            params.setPictureSize(captureSize[captureSizeIDX].width, captureSize[captureSizeIDX].height);

            if ( hardwareActive )
                camera->setParameters(params.flatten());
            break;

        case 'l':
        case 'L':
            VcaptureSizeIDX++;
            VcaptureSizeIDX %= ARRAY_SIZE(VcaptureSize);
            break;

        case ']':
            VbitRateIDX++;
            VbitRateIDX %= ARRAY_SIZE(VbitRate);
            break;


        case '6':

            if ( !recordingMode ) {

                recordingMode = true;

                if ( startPreview() < 0 ) {
                    printf("Error while starting preview\n");

                    return -1;
                }

                if ( openRecorder() < 0 ) {
                    printf("Error while openning video recorder\n");

                    return -1;
                }

                if ( configureRecorder() < 0 ) {
                    printf("Error while configuring video recorder\n");

                    return -1;
                }

                if ( startRecording() < 0 ) {
                    printf("Error while starting video recording\n");

                    return -1;
                }
            }

            break;

        case '7':

            if ( compensation > 2.0) {
                compensation = -2.0;
            } else {
                compensation += 0.1;
            }

            params.set(KEY_COMPENSATION, (int) (compensation * 10));

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case '8':
            awb_mode++;
            awb_mode %= ARRAY_SIZE(strawb_mode);
            params.set(params.KEY_WHITE_BALANCE, strawb_mode[awb_mode]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case '9':
            videoCodecIDX++;
            videoCodecIDX %= ARRAY_SIZE(videoCodecs);
            break;
        case '~':
            previewFormat += 1;
            previewFormat %= ARRAY_SIZE(pixelformat) - 1;
            params.setPreviewFormat(pixelformat[previewFormat]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;
        case '$':
            pictureFormat += 1;
            pictureFormat %= ARRAY_SIZE(pixelformat);
            params.setPictureFormat(pixelformat[pictureFormat]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case '?' :
            ///Set mode=3 to select video mode
            params.set(KEY_MODE, 3);
            params.set(KEY_VNF, 1);
            params.set(KEY_VSTAB, 1);
            break;

        case ':':
            thumbSizeIDX += 1;
            thumbSizeIDX %= ARRAY_SIZE(previewSize);
            params.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, previewSize[thumbSizeIDX].width);
            params.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, previewSize[thumbSizeIDX].height);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case '\'':
            if ( thumbQuality >= 100) {
                thumbQuality = 0;
            } else {
                thumbQuality += 5;
            }

            params.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, thumbQuality);
            if ( hardwareActive )
                camera->setParameters(params.flatten());
            break;

        case 'B' :
            vnf_mode++;
            vnf_mode %= ARRAY_SIZE(vnf);
            params.set(KEY_VNF, vnf_mode);

            if ( hardwareActive )
                camera->setParameters(params.flatten());
            break;

        case 'C' :
            vstab_mode++;
            vstab_mode %= ARRAY_SIZE(vstab);
            params.set(KEY_VSTAB, vstab_mode);

            if ( hardwareActive )
                camera->setParameters(params.flatten());
            break;

        case 'E':
            if(hardwareActive)
                params.unflatten(camera->getParameters());
            printSupportedParams();
            break;

        case '*':
            if ( hardwareActive )
                camera->startRecording();
            break;

        case 'o':
            if ( jpegQuality >= 100) {
                jpegQuality = 0;
            } else {
                jpegQuality += 5;
            }

            params.set(CameraParameters::KEY_JPEG_QUALITY, jpegQuality);
            if ( hardwareActive )
                camera->setParameters(params.flatten());
            break;

        case 'm':
        {
            meter_mode = (meter_mode + 1)%ARRAY_SIZE(metering);
            params.set(KEY_METERING_MODE, metering[meter_mode]);
            break;
        }

        case 'n':
        {
            nullOverlay = true;
            break;
        }

        case 'k':
            ippIDX += 1;
            ippIDX %= ARRAY_SIZE(ipp_mode);
            params.set(KEY_IPP, ippIDX);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'K':
            postProcIDX+= 1;
            postProcIDX %= ARRAY_SIZE(post_proc);
            params.set(KEY_POST_PROC, post_proc[postProcIDX]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case '@':
            if ( hardwareActive ) {

                closeCamera();

                hardwareActive = false;

                if ( 0 >= openCamera() ) {
                    hardwareActive = true;
                    printf( "Reconnected to CameraService \n");
                }
            }

            break;

        case '#':

            if ( burst >= MAX_BURST ) {
                burst = 0;
            } else {
                burst += BURST_INC;
            }
            params.set(KEY_BURST, burst);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'u':
            capture_mode++;
            capture_mode %= ARRAY_SIZE(capture);
            params.set(KEY_MODE, (capture[capture_mode]));

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'w':
            scene_mode++;
            scene_mode %= ARRAY_SIZE(scene);
            params.set(params.KEY_SCENE_MODE, scene[scene_mode]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'y':
            caf_mode++;
            caf_mode %= ARRAY_SIZE(caf);
            params.set(KEY_CAF, caf_mode);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'i':
            iso_mode++;
            iso_mode %= ARRAY_SIZE(iso);
            params.set(KEY_ISO, iso[iso_mode]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());
            break;

        case 'h':
            if ( sharpness >= 100) {
                sharpness = 0;
            } else {
                sharpness += 10;
            }
            params.set(KEY_SHARPNESS, sharpness);
            if ( hardwareActive )
                camera->setParameters(params.flatten());
            break;

        case 'D':
        {
            audioCodecIDX++;
            audioCodecIDX %= ARRAY_SIZE(audioCodecs);
            break;
        }

        case 'v':
        {
            outputFormatIDX++;
            outputFormatIDX %= ARRAY_SIZE(outputFormat);
            break;
        }

        case 'z':
            zoomIDX++;
            zoomIDX %= ARRAY_SIZE(zoom);
            params.set(CameraParameters::KEY_ZOOM, zoom[zoomIDX].idx);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'j':
            exposure_mode++;
            exposure_mode %= ARRAY_SIZE(exposure);
            params.set(KEY_EXPOSURE, exposure[exposure_mode]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'c':
            if( contrast >= 200){
                contrast = 0;
            } else {
                contrast += 10;
            }
            params.set(KEY_CONTRAST, contrast);
            if ( hardwareActive )
                camera->setParameters(params.flatten());
            break;
        case 'b':
            if ( brightness >= 200) {
                brightness = 0;
            } else {
                brightness += 10;
            }

            params.set(KEY_BRIGHTNESS, brightness);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 's':
        case 'S':
            if ( saturation >= 100) {
                saturation = 0;
            } else {
                saturation += 10;
            }

            params.set(KEY_SATURATION, saturation);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'e':
            effects_mode++;
            effects_mode %= ARRAY_SIZE(effects);
            params.set(params.KEY_EFFECT, effects[effects_mode]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'r':
            frameRateIDX += 1;
            frameRateIDX %= ARRAY_SIZE(frameRate);
            params.setPreviewFrameRate(frameRate[frameRateIDX].fps);

            if ( hardwareActive && previewRunning ) {
                camera->stopPreview();
                camera->setParameters(params.flatten());
                camera->startPreview();
            } else if ( hardwareActive ) {
                camera->setParameters(params.flatten());
            }

            break;

        case 'x':
            antibanding_mode++;
            antibanding_mode %= ARRAY_SIZE(antibanding);
            params.set(params.KEY_ANTIBANDING, antibanding[antibanding_mode]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'g':
            focus_mode++;
            focus_mode %= ARRAY_SIZE(focus);
            params.set(params.KEY_FOCUS_MODE, focus[focus_mode]);

            if ( hardwareActive )
                camera->setParameters(params.flatten());

            break;

        case 'f':
            gettimeofday(&autofocus_start, 0);

            if ( hardwareActive )
                camera->autoFocus();

            break;

        case 'p':

            gettimeofday(&picture_start, 0);

            if ( hardwareActive )
                camera->takePicture();

            break;

        case '&':
            printf("Enabling Preview Callback");
            dump_preview = 1;
            if ( hardwareActive )
            camera->setPreviewCallbackFlags(FRAME_CALLBACK_FLAG_ENABLE_MASK);
            break;

        case 'a':

            while (1) {
                if ( menu_gps() < 0)
                    break;
            };

            break;

        case 'q':

            stopPreview();

            return -1;

        case '/':
        {
            if (showfps)
            {
                property_set("debug.image.showfps", "0");
                showfps = false;
            }
            else
            {
                property_set("debug.image.showfps", "1");
                showfps = true;
            }
            break;
        }

        default:
            print_menu = 0;

            break;
    }

    return 0;
}

status_t dump_mem_status() {
  return system(MEMORY_DUMP);
}

char *load_script(char *config) {
    FILE *infile;
    size_t fileSize;
    char *script;
    size_t nRead = 0;
    char dir_name[20];
    char log_cmd[60];

    infile = fopen(config, "r");

    strcpy(script_name,config);
    printf("\n SCRIPT : <%s> is currently being executed \n",script_name);
    if(strncpy(dir_name,config,16) == NULL)
        printf("Strcpy error");
    dir_name[16]=NULL;
    if(strcat(dir_path,dir_name) == NULL)
        printf("Strcat error");
    printf("\n COMPLETE FOLDER PATH : %s \n",dir_path);
    if(mkdir(dir_path,0777) == -1) {
        printf("\n Directory %s was not created \n",dir_path);
    } else {
        printf("\n Directory %s was created \n",dir_path);
    }
    printf("\n DIRECTORY CREATED FOR TEST RESULT IMAGES IN MMC CARD : %s \n",dir_name);

    if( (NULL == infile)){
        printf("Error while opening script file %s!\n", config);
        return NULL;
    }

    fseek(infile, 0, SEEK_END);
    fileSize = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    script = (char *) malloc(fileSize);

    if ( NULL == script ) {
        printf("Unable to allocate buffer for the script\n");

        return NULL;
    }

    if ((nRead = fread(script, 1, fileSize, infile)) != fileSize) {
        printf("Error while reading script file!\n");

        free(script);
        fclose(infile);
        return NULL;
    }

    fclose(infile);

    /*Start logcat before executing script*/

    if(!sprintf(log_cmd,"logcat > /sdcard/%s/log.txt &",dir_name))
          printf(" Sprintf Error");
    if(system(log_cmd))
          printf("\nCould not execute %s command\n",log_cmd);

    return script;
}

char * get_cycle_cmd(const char *aSrc) {
    unsigned ind = 0;
    char *cycle_cmd = new char[256];

    while ((*aSrc != '+') && (*aSrc != '\0')) {
        cycle_cmd[ind++] = *aSrc++;
    }
    cycle_cmd[ind] = '\0';

    return cycle_cmd;
}

int execute_functional_script(char *script) {
    char *cmd, *ctx, *cycle_cmd, *temp_cmd;
    char id;
    unsigned int i;
    int dly;
    int cycleCounter = 1;
    int tLen = 0;
    unsigned int iteration = 0;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    dump_mem_status();

    cmd = strtok_r((char *) script, DELIMITER, &ctx);

    while ( NULL != cmd ) {
        id = cmd[0];
        printf("Full Command: %s \n", cmd);
        printf("Command: %c \n", cmd[0]);

        switch (id) {
            case '[':
                if ( hardwareActive )
                    {

                    camera->setParameters(params.flatten());

                    printf("starting camera preview..");
                    status_t ret = camera->startPreview();
                    if(ret !=NO_ERROR)
                        {
                        printf("startPreview failed %d..", ret);
                        }
                    }
                break;
            case '+': {
                cycleCounter = atoi(cmd + 1);
                cycle_cmd = get_cycle_cmd(ctx);
                tLen = strlen(cycle_cmd);
                temp_cmd = new char[tLen+1];

                for (int ind = 0; ind < cycleCounter; ind++) {
                    strcpy(temp_cmd, cycle_cmd);
                    execute_functional_script(temp_cmd);
                    temp_cmd[0] = '\0';

                    //patch for image capture
                    //[
                    if (ind < cycleCounter - 1) {
                        if (hardwareActive == false) {
                            hardwareActive = true;

                            if ( openCamera() < 0 ) {
                                printf("Camera initialization failed\n");

                                return -1;
                            }

                            params.unflatten(camera->getParameters());
                            initDefaults();
                        }
                    }

                    //]
                }

                ctx += tLen + 1;

                if (temp_cmd) {
                    delete temp_cmd;
                    temp_cmd = NULL;
                }

                if (cycle_cmd) {
                    delete cycle_cmd;
                    cycle_cmd = NULL;
                }

                break;
            }

            case '0':
            {
                initDefaults();
                break;
            }

            case '1':

                if ( startPreview() < 0 ) {
                    printf("Error while starting preview\n");

                    return -1;
                }

                break;

            case '2':
                stopPreview();

                if ( recordingMode ) {
                    stopRecording();
                    closeRecorder();

                    recordingMode = false;
                }

                break;

            case '3':
                rotation = atoi(cmd + 1);
                params.set(CameraParameters::KEY_ROTATION, rotation);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case '4':
            {
                printf("Setting resolution...");
                int width, height;
                for(i = 0; i < ARRAY_SIZE(previewSize); i++)
                {
                    if( strcmp((cmd + 1), previewSize[i].desc) == 0)
                    {
                        width = previewSize[i].width;
                        height = previewSize[i].height;
                        previewSizeIDX = i;
                        break;
                    }
                }

                if (i == ARRAY_SIZE(previewSize))   //if the resolution is not in the supported ones
                {
                    char *res = NULL;
                    res = strtok(cmd + 1, "x");
                    width = atoi(res);
                    res = strtok(NULL, "x");
                    height = atoi(res);
                }

                printf("Resolution: %d x %d\n", width, height);

                params.setPreviewSize(width, height);
                reSizePreview = true;

                if ( hardwareActive && previewRunning ) {
                    camera->stopPreview();
                    camera->setParameters(params.flatten());
                    camera->startPreview();
                } else if ( hardwareActive ) {
                    camera->setParameters(params.flatten());
                }

                break;
            }
            case '5':

                for (i = 0; i < ARRAY_SIZE(captureSize); i++) {
                    if ( strcmp((cmd + 1), captureSize[i].name) == 0)
                        break;
                }

                if (  i < ARRAY_SIZE(captureSize) )
                    params.setPictureSize(captureSize[i].width, captureSize[i].height);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case '6':

                if ( !recordingMode ) {

                    recordingMode = true;

                    if ( startPreview() < 0 ) {
                        printf("Error while starting preview\n");

                        return -1;
                    }

                    if ( openRecorder() < 0 ) {
                        printf("Error while openning video recorder\n");

                        return -1;
                    }

                    if ( configureRecorder() < 0 ) {
                        printf("Error while configuring video recorder\n");

                        return -1;
                    }

                    if ( startRecording() < 0 ) {
                        printf("Error while starting video recording\n");

                        return -1;
                    }

                }

                break;

            case '7':
                compensation = atof(cmd + 1);
                params.set(KEY_COMPENSATION, (int) (compensation * 10));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case '8':
                params.set(params.KEY_WHITE_BALANCE, (cmd + 1));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case '9':
                for(i = 0; i < ARRAY_SIZE(videoCodecs); i++)
                {
                    if( strcmp((cmd + 1), videoCodecs[i].desc) == 0)
                    {
                        videoCodecIDX = i;
                        printf("Video Codec Selected: %s\n",
                                videoCodecs[i].desc);
                        break;
                    }
                }
                break;

            case 'v':
                for(i = 0; i < ARRAY_SIZE(outputFormat); i++)
                {
                    if( strcmp((cmd + 1), outputFormat[i].desc) == 0)
                    {
                        outputFormatIDX = i;
                        printf("Video Codec Selected: %s\n",
                                videoCodecs[i].desc);
                        break;
                    }
                }
            break;

            case '~':
                params.setPreviewFormat(cmd + 1);
                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case '$':
                params.setPictureFormat(cmd + 1);
                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;
            case '-':
                for(i = 0; i < ARRAY_SIZE(audioCodecs); i++)
                {
                    if( strcmp((cmd + 1), audioCodecs[i].desc) == 0)
                    {
                        audioCodecIDX = i;
                        printf("Selected Audio: %s\n", audioCodecs[i].desc);
                        break;
                    }
                }
                break;

            case 'A':
                camera_index=atoi(cmd+1);
                camera_index %= ARRAY_SIZE(cameras);
                params.set(KEY_CAMERA, camera_index);
                printf("%s selected.\n", cameras[camera_index]);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());
                break;

            case 'l':
            case 'L':
                for(i = 0; i < ARRAY_SIZE(VcaptureSize); i++)
                {
                    if( strcmp((cmd + 1), VcaptureSize[i].desc) == 0)
                    {
                        VcaptureSizeIDX = i;
                        printf("Video Capture Size: %s\n", VcaptureSize[i].desc);
                        break;
                    }
                }
                break;
            case ']':
                for(i = 0; i < ARRAY_SIZE(VbitRate); i++)
                {
                    if( strcmp((cmd + 1), VbitRate[i].desc) == 0)
                    {
                        VbitRateIDX = i;
                        printf("Video Bit Rate: %s\n", VbitRate[i].desc);
                        break;
                    }
                }
                break;
            case ':':
                int width, height;
                for(i = 0; i < ARRAY_SIZE(previewSize); i++)
                {
                    if( strcmp((cmd + 1), previewSize[i].desc) == 0)
                    {
                        width = previewSize[i].width;
                        height = previewSize[i].height;
                        thumbSizeIDX = i;
                        break;
                    }
                }

                if (i == ARRAY_SIZE(previewSize))   //if the resolution is not in the supported ones
                {
                    char *res = NULL;
                    res = strtok(cmd + 1, "x");
                    width = atoi(res);
                    res = strtok(NULL, "x");
                    height = atoi(res);
                }

                params.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, width);
                params.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, height);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case '\'':
                thumbQuality = atoi(cmd + 1);

                params.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, thumbQuality);
                if ( hardwareActive )
                    camera->setParameters(params.flatten());
                break;

            case '*':
                if ( hardwareActive )
                    camera->startRecording();
                break;

            case 't':
                params.setPreviewFormat((cmd + 1));
                if ( hardwareActive )
                    camera->setParameters(params.flatten());
                break;

            case 'o':
                jpegQuality = atoi(cmd + 1);
                params.set(CameraParameters::KEY_JPEG_QUALITY, jpegQuality);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;


            case '&':
                printf("Enabling Preview Callback");
                dump_preview = 1;
                camera->setPreviewCallbackFlags(FRAME_CALLBACK_FLAG_ENABLE_MASK);
                break;


            case 'k':
                params.set(KEY_IPP, atoi(cmd + 1));
                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'K':
                params.set(KEY_POST_PROC, (cmd+1));
                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'u':
                params.set(KEY_MODE, (cmd + 1));
                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case '#':

                params.set(KEY_BURST, atoi(cmd + 1));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;


            case 'w':
                params.set(params.KEY_SCENE_MODE, (cmd + 1));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'B' :
                params.set(KEY_VNF, (cmd + 1));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());


            case 'C' :
                params.set(KEY_VSTAB, (cmd + 1));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());
                break;

            case 'D':
                if ( hardwareActive )
                    camera->stopRecording();
                break;

            case 'E':
                if(hardwareActive)
                    params.unflatten(camera->getParameters());
                printSupportedParams();
                break;

            case 'y':
                caf_mode = atoi(cmd + 1);
                params.set(KEY_CAF, caf_mode);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'i':
                iso_mode = atoi(cmd + 1);
                params.set(KEY_ISO, iso_mode);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'h':
                sharpness = atoi(cmd + 1);
                params.set(KEY_SHARPNESS, sharpness);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case '@':
                if ( hardwareActive ) {

                    closeCamera();

                    hardwareActive = false;

                    if ( 0 >= openCamera() ) {
                        hardwareActive = true;
                        printf( "Reconnected to CameraService \n");
                    }
                }

                break;

            case 'c':
                contrast = atoi(cmd + 1);
                params.set(KEY_CONTRAST, contrast);

                if ( hardwareActive ) {
                    camera->setParameters(params.flatten());
                }

                break;

            case 'z':
            case 'Z':
                params.set(CameraParameters::KEY_ZOOM, (atoi(cmd + 1) - 1));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'j':
                params.set(KEY_EXPOSURE, atoi(cmd + 1));
                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'b':
                brightness = atoi(cmd + 1);
                params.set(KEY_BRIGHTNESS, brightness);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 's':
                saturation = atoi(cmd + 1);
                params.set(KEY_SATURATION, saturation);

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'e':
                params.set(params.KEY_EFFECT, (cmd + 1));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'r':
                params.setPreviewFrameRate(atoi(cmd + 1));

                if ( hardwareActive && previewRunning ) {
                    camera->stopPreview();
                    camera->setParameters(params.flatten());
                    camera->startPreview();
                } else if ( hardwareActive ) {
                    camera->setParameters(params.flatten());
                }

                break;

            case 'x':
                params.set(params.KEY_ANTIBANDING, (cmd + 1));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'g':
                params.set(params.KEY_FOCUS_MODE, (cmd + 1));

                if ( hardwareActive )
                    camera->setParameters(params.flatten());

                break;

            case 'f':
                gettimeofday(&autofocus_start, 0);

                if ( hardwareActive )
                    camera->autoFocus();

                break;

            case 'p':
                gettimeofday(&picture_start, 0);

                if ( hardwareActive )
                    ret = camera->takePicture();

                if ( ret != NO_ERROR )
                    printf("Error returned while taking a picture");
                break;

            case 'd':
                dly = atoi(cmd + 1);
                sleep(dly);
                break;

            case 'q':
                stopPreview();

                if ( recordingMode ) {
                    stopRecording();
                    closeRecorder();

                    recordingMode = false;
                }

                if(system("exit"))
                      printf("Exit command failed");
                else
                      printf("\nlogcat for script %s is complete\n Saved @ location: %s\n",script_name,dir_path);
                goto exit;

            case '\n':
                printf("Iteration: %d \n", iteration);
                iteration++;
                break;

            case 'M':
            case 'm':
            {
                params.set(KEY_METERING_MODE, (cmd + 1));
                if ( hardwareActive )
                {
                    camera->setParameters(params.flatten());
                }
                break;
            }
            case '<':
            {
                char coord_str[8];
                latitude += degree_by_step;
                if (latitude > 90.0)
                {
                    latitude -= 180.0;
                }
                snprintf(coord_str, 7, "%.7lf", latitude);
                params.set(params.KEY_GPS_LATITUDE, coord_str);
                if ( hardwareActive )
                {
                    camera->setParameters(params.flatten());
                }
                break;
            }

            case '=':
            {
                char coord_str[8];
                longitude += degree_by_step;
                if (longitude > 180.0)
                {
                    longitude -= 360.0;
                }
                snprintf(coord_str, 7, "%.7lf", longitude);
                params.set(params.KEY_GPS_LONGITUDE, coord_str);
                if ( hardwareActive )
                {
                    camera->setParameters(params.flatten());
                }
                break;
            }

            case '>':
            {
                char coord_str[8];
                altitude += 12345.67890123456789;
                if (altitude > 100000.0)
                {
                    altitude -= 200000.0;
                }
                snprintf(coord_str, 7, "%.7lf", altitude);
                params.set(params.KEY_GPS_ALTITUDE, coord_str);
                if ( hardwareActive )
                {
                    camera->setParameters(params.flatten());
                }
                break;
            }

            case 'X':
            {
                char rem_str[50];
                printf("Deleting images from %s \n", dir_path);
                if(!sprintf(rem_str,"rm %s/*.jpg",dir_path))
                    printf("Sprintf Error");
                if(system(rem_str))
                    printf("Images were not deleted\n");
                break;
            }

            case 'n':
            {
                nullOverlay = true;
                break;
            }

            default:
                printf("Unrecognized command!\n");
                break;
        }

        cmd = strtok_r(NULL, DELIMITER, &ctx);
    }

exit:

    return 0;
}

int execute_error_script(char *script) {
    char *cmd, *ctx;
    char id;
    status_t stat = NO_ERROR;

    LOG_FUNCTION_NAME

    cmd = strtok_r((char *) script, DELIMITER, &ctx);

    while ( NULL != cmd ) {
        id = cmd[0];

        switch (id) {

            case '0': {
                bufferStarvationTest = 1;
                params.set(KEY_BUFF_STARV, bufferStarvationTest); //enable buffer starvation

                if ( !recordingMode ) {

                    recordingMode = true;

                    if ( startPreview() < 0 ) {
                        printf("Error while starting preview\n");

                        return -1;
                    }

                    if ( openRecorder() < 0 ) {
                        printf("Error while openning video recorder\n");

                        return -1;
                    }

                    if ( configureRecorder() < 0 ) {
                        printf("Error while configuring video recorder\n");

                        return -1;
                    }

                    if ( startRecording() < 0 ) {
                        printf("Error while starting video recording\n");

                        return -1;
                    }

                }

                usleep(1000000);//1s

                stopPreview();

                if ( recordingMode ) {
                    stopRecording();
                    closeRecorder();

                    recordingMode = false;
                }

                break;
            }

            case '1': {
                int* tMemoryEater = new int[999999999];

                if (!tMemoryEater) {
                    printf("Not enough memory\n");
                    return -1;
                } else {
                    delete tMemoryEater;
                }

                break;
            }

            case '2': {
                //camera = Camera::connect();

                if ( NULL == camera.get() ) {
                    printf("Unable to connect to CameraService\n");
                    return -1;
                }

                break;
            }

            case '3': {
                int err = 0;

                err = open("/dev/video5", O_RDWR);

                if (err < 0) {
                    printf("Could not open the camera device5: %d\n",  err );
                    return err;
                }

                if ( startPreview() < 0 ) {
                    printf("Error while starting preview\n");
                    return -1;
                }

                usleep(1000000);//1s

                stopPreview();

                close(err);
                break;
            }

            case '4': {

                if ( hardwareActive ) {

                    params.setPictureFormat("invalid-format");
                    params.setPreviewFormat("invalid-format");

                    stat = camera->setParameters(params.flatten());

                    if ( NO_ERROR != stat ) {
                        printf("Test passed!\n");
                    } else {
                        printf("Test failed!\n");
                    }

                    initDefaults();
                }

                break;
            }

            case '5': {

                if ( hardwareActive ) {

                    params.setPictureSize(-1, -1);
                    params.setPreviewSize(-1, -1);

                    stat = camera->setParameters(params.flatten());

                    if ( NO_ERROR != stat ) {
                        printf("Test passed!\n");
                    } else {
                        printf("Test failed!\n");
                    }

                    initDefaults();
                }

                break;
            }

            case '6': {

                if ( hardwareActive ) {

                    params.setPreviewFrameRate(-1);

                    stat = camera->setParameters(params.flatten());

                    if ( NO_ERROR != stat ) {
                        printf("Test passed!\n");
                    } else {
                        printf("Test failed!\n");
                    }

                    initDefaults();
                }


                break;
            }

            case 'q': {
                goto exit;

                break;
            }

            default: {
                printf("Unrecognized command!\n");

                break;
            }
        }

        cmd = strtok_r(NULL, DELIMITER, &ctx);
    }

exit:

    return 0;
}



void print_usage() {
    printf(" USAGE: camera_test  <param>  <script>\n");
    printf(" <param>\n-----------\n\n");
    printf(" F or f -> Functional tests \n");
    printf(" A or a -> API tests \n");
    printf(" E or e -> Error scenario tests \n");
    printf(" S or s -> Stress tests \n\n");
    printf(" <script>\n----------\n");
    printf("Script name (Only for stress tests)\n\n");
    return;
}

int error_scenario() {
    char ch;
    status_t stat = NO_ERROR;

    if (print_menu) {
        printf("   0. Buffer need\n");
        printf("   1. Not enough memory\n");
        printf("   2. Media server crash\n");
        printf("   3. Overlay object request\n");
        printf("   4. Pass unsupported preview&picture format\n");
        printf("   5. Pass unsupported preview&picture resolution\n");
        printf("   6. Pass unsupported preview framerate\n");

        printf("   q. Quit\n");
        printf("   Choice: ");
    }

    print_menu = 1;
    ch = getchar();
    printf("%c\n", ch);

    switch (ch) {
        case '0': {
            printf("Case0:Buffer need\n");
            bufferStarvationTest = 1;
            params.set(KEY_BUFF_STARV, bufferStarvationTest); //enable buffer starvation

            if ( !recordingMode ) {
                recordingMode = true;
                if ( startPreview() < 0 ) {
                    printf("Error while starting preview\n");

                    return -1;
                }

                if ( openRecorder() < 0 ) {
                    printf("Error while openning video recorder\n");

                    return -1;
                }

                if ( configureRecorder() < 0 ) {
                    printf("Error while configuring video recorder\n");

                    return -1;
                }

                if ( startRecording() < 0 ) {
                    printf("Error while starting video recording\n");

                    return -1;
                }

            }

            usleep(1000000);//1s

            stopPreview();

            if ( recordingMode ) {
                stopRecording();
                closeRecorder();

                recordingMode = false;
            }

            break;
        }

        case '1': {
            printf("Case1:Not enough memory\n");
            int* tMemoryEater = new int[999999999];

            if (!tMemoryEater) {
                printf("Not enough memory\n");
                return -1;
            } else {
                delete tMemoryEater;
            }

            break;
        }

        case '2': {
            printf("Case2:Media server crash\n");
            //camera = Camera::connect();

            if ( NULL == camera.get() ) {
                printf("Unable to connect to CameraService\n");
                return -1;
            }

            break;
        }

        case '3': {
            printf("Case3:Overlay object request\n");
            int err = 0;

            err = open("/dev/video5", O_RDWR);

            if (err < 0) {
                printf("Could not open the camera device5: %d\n",  err );
                return err;
            }

            if ( startPreview() < 0 ) {
                printf("Error while starting preview\n");
                return -1;
            }

            usleep(1000000);//1s

            stopPreview();

            close(err);
            break;
        }

        case '4': {

            if ( hardwareActive ) {

                params.setPictureFormat("invalid-format");
                params.setPreviewFormat("invalid-format");

                stat = camera->setParameters(params.flatten());

                if ( NO_ERROR != stat ) {
                    printf("Test passed!\n");
                } else {
                    printf("Test failed!\n");
                }

                initDefaults();
            }

            break;
        }

        case '5': {

            if ( hardwareActive ) {

                params.setPictureSize(-1, -1);
                params.setPreviewSize(-1, -1);

                stat = camera->setParameters(params.flatten());

                if ( NO_ERROR != stat ) {
                    printf("Test passed!\n");
                } else {
                    printf("Test failed!\n");
                }

                initDefaults();
            }

            break;
        }

        case '6': {

            if ( hardwareActive ) {

                params.setPreviewFrameRate(-1);

                stat = camera->setParameters(params.flatten());

                if ( NO_ERROR != stat ) {
                    printf("Test passed!\n");
                } else {
                    printf("Test failed!\n");
                }

                initDefaults();
            }


            break;
        }

        case 'q': {
            return -1;
        }

        default: {
            print_menu = 0;
            break;
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    char *cmd;
    sp<ProcessState> proc(ProcessState::self());

    unsigned long long st, end, delay;
    timeval current_time;

    gettimeofday(&current_time, 0);

    st = current_time.tv_sec * 1000000 + current_time.tv_usec;

    cmd = NULL;

    if ( argc < 2 ) {
        printf(" Please enter atleast 1 argument\n");
        print_usage();

        return 0;
    }

    if ( argc < 3 ) {
        switch (*argv[1]) {
            case 'S':
            case 's':
                printf("This is stress / regression tests \n");
                printf("Provide script file as 2nd argument\n");

                break;

            case 'F':
            case 'f':
                ProcessState::self()->startThreadPool();

                if ( openCamera() < 0 ) {
                    printf("Camera initialization failed\n");

                    return -1;
                }

                hardwareActive = true;
                params.unflatten(camera->getParameters());
                initDefaults();
                print_menu = 1;

                while ( 1 ) {
                    if ( functional_menu() < 0 )
                        break;
                };

                break;

            case 'A':
            case 'a':
                printf("API level test cases coming soon ... \n");

                break;

            case 'E':
            case 'e': {
                ProcessState::self()->startThreadPool();

                if ( openCamera() < 0 ) {
                    printf("Camera initialization failed\n");

                    return -1;
                }

                hardwareActive = true;
                params.unflatten(camera->getParameters());
                initDefaults();
                print_menu = 1;

                while (1) {
                    if (error_scenario() < 0) {
                        break;
                    }
                }

                break;
            }

            default:
                printf("INVALID OPTION USED\n");
                print_usage();

                break;
        }
    } else if ( ( argc == 3) && ( ( *argv[1] == 'S' ) || ( *argv[1] == 's') ) ) {

        ProcessState::self()->startThreadPool();

        if ( openCamera() < 0 ) {
            printf("Camera initialization failed\n");

            return -1;
        }

        hardwareActive = true;

        params.unflatten(camera->getParameters());
        initDefaults();

        cmd = load_script(argv[2]);

        if ( cmd != NULL) {
            execute_functional_script(cmd);
            free(cmd);
        }
    } else if ( ( argc == 3) && ( ( *argv[1] == 'E' ) || ( *argv[1] == 'e') ) ) {

        ProcessState::self()->startThreadPool();

        if ( openCamera() < 0 ) {
            printf("Camera initialization failed\n");

            return -1;
        }

        hardwareActive = true;

        params.unflatten(camera->getParameters());
        initDefaults();

        cmd = load_script(argv[2]);

        if ( cmd != NULL) {
            execute_error_script(cmd);
            free(cmd);
        }

    } else {
        printf("INVALID OPTION USED\n");
        print_usage();
    }

    gettimeofday(&current_time, 0);
    end = current_time.tv_sec * 1000000 + current_time.tv_usec;
    delay = end - st;
    printf("Application clossed after: %llu ms\n", delay);

    return 0;
}
