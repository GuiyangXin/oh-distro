/*
  TLD's work well w/ features (keypts) low-level features

  - DoT works well with textureless objects (contoures)
  - Use both to bootstrap a contour and features based reconstruction 
  - Does going to 3D from contour make tracking possible ??

*/
#define WINDOW_NAME "TLD Segmenter"

#include <set>
#include <bot_core/bot_core.h>
#include <bot_frames/bot_frames.h>
#include <bot_param/param_client.h>

#include <opencv2/opencv.hpp>

#include <image_io_utils/image_io_utils.hpp> // to simplify jpeg/zlib compression and decompression
#include <lcmtypes/perception_image_roi_t.h>
#include <ConciseArgs>

const int WINDOW_WIDTH = 640/2; 
const int WINDOW_HEIGHT = 480/2; 

int MAX_IMAGE_WIDTH = 0;
int MAX_IMAGE_HEIGHT = 0;

const int MAX_THUMB_SIZE = 100;

int32_t OBJECT_ID = 1; 
int32_t FEATURE_ID = 1; // 1 for object (reference), -1 for virtual heading object
using namespace cv;

struct state_t {
    lcm_t *lcm;
    pthread_t lcm_thread;
    pthread_mutex_t img_mutex;

    GMainLoop *mainloop;
    BotParam *param;
    BotFrames *frames;
    int counter;

    // Img
    cv::Mat img; 

    cv::Mat img_selection;

    // utimes for image
    int64_t  img_utime;

    // UI selection of desired object
    Rect selection, selection_virtual, tld_result;
    Point origin, origin_virtual;
    bool selectObject, selectObject_virtual;

    perception_image_roi_t *tracking_result;

    state_t () {
        // LCM, BotFrames, BotParam inits
        lcm =  bot_lcm_get_global(NULL);
        param = bot_param_new_from_server(lcm, 1);
        frames = bot_frames_get_global (lcm, param);

        img_mutex = PTHREAD_MUTEX_INITIALIZER;
        // Counter for debug prints
        counter = 0; 
        tracking_result = NULL;
        selectObject = false;
        selectObject_virtual = false;
    }
    ~state_t () { 
    }

};
state_t * state = NULL;

void* lcm_thread_handler(void *l) {
    state_t* state = (state_t*)l;
    while(1)
        lcm_handle(state->lcm);

}
struct MouseEvent {
    MouseEvent() { event = -1; buttonState = 0; }
    Point pt;
    int event;
    int buttonState;
};
MouseEvent mouse;

static void onMouse(int event, int x, int y, int flags, void* userdata) {
    MouseEvent* data = (MouseEvent*)userdata;

    float sx = 1.f / WINDOW_WIDTH ; 
    float sy = 1.f / WINDOW_HEIGHT;

    if (state->selectObject) {
        state->selection.x = MIN(x, state->origin.x);
        state->selection.y = MIN(y, state->origin.y);
        state->selection.width = std::abs(x - state->origin.x);
        state->selection.height = std::abs(y - state->origin.y);
        state->selection &= Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);


                   
    }

    if (state->selectObject_virtual) {
        state->selection_virtual.x = MIN(x, state->origin_virtual.x);
        state->selection_virtual.y = MIN(y, state->origin_virtual.y);
        state->selection_virtual.width = std::abs(x - state->origin_virtual.x);
        state->selection_virtual.height = std::abs(y - state->origin_virtual.y);
        state->selection &= Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    switch (event) {
    case CV_EVENT_RBUTTONDOWN:
        state->origin_virtual = Point(x, y);
        state->selection_virtual = Rect(x, y, 0, 0);
        state->selectObject_virtual = true;
        break;
    case CV_EVENT_RBUTTONUP:
        state->selectObject_virtual = false;
        std::cerr << "SEND virtual selection: " << state->img_utime << " - " << 
            state->selection_virtual.x << " " << state->selection_virtual.y << " " << 
            state->selection_virtual.width << " " << state->selection_virtual.height << std::endl;

        perception_image_roi_t img_vselection;
        img_vselection.utime = state->img_utime;
        img_vselection.object_id = OBJECT_ID; 
        img_vselection.feature_id = -1; // FEATURE_ID; 
        img_vselection.roi.x = state->selection_virtual.x * sx;
        img_vselection.roi.y = state->selection_virtual.y * sy;
        img_vselection.roi.width = state->selection_virtual.width * sx;
        img_vselection.roi.height = state->selection_virtual.height * sy;
        perception_image_roi_t_publish(state->lcm, "TLD_OBJECT_ROI", &img_vselection);

        // destroyWindow(WINDOW_NAME);
        // state->img = cv::Mat();

        break;
    case CV_EVENT_LBUTTONDOWN:
        state->origin = Point(x, y);
        state->selection = Rect(x, y, 0, 0);
        state->selectObject = true;

        // reset virtual feature
        state->selection_virtual = Rect(0,0,0,0);

        break;
    case CV_EVENT_LBUTTONUP:
        state->selectObject = false;
        std::cerr << "SEND selection: " << state->img_utime << " - " << 
            state->selection.x << " " << state->selection.y << " " << 
            state->selection.width << " " << state->selection.height << std::endl;

        perception_image_roi_t img_selection;
        img_selection.utime = state->img_utime;
        img_selection.object_id = OBJECT_ID; 
        img_selection.feature_id = 1; // FEATURE_ID; 
        img_selection.roi.x = state->selection.x * sx;
        img_selection.roi.y = state->selection.y * sy;
        img_selection.roi.width = state->selection.width * sx;
        img_selection.roi.height = state->selection.height * sy;
        perception_image_roi_t_publish(state->lcm, "TLD_OBJECT_ROI", &img_selection);

        float sc_x = sx * state->img.cols;
        float sc_y = sy * state->img.rows;
        
        int x_s = state->selection.x * sc_x;
        int y_s = state->selection.y * sc_y;
        int width_s = state->selection.width * sc_x;
        int height_s = state->selection.height * sc_y;

        if(width_s > 0 && height_s >0){
            state->img_selection = state->img(cv::Rect(x_s, y_s, width_s, height_s)).clone();                       
         
        } 

        // don't display the selection after publish
        state->selection.width = 0;  state->selection.height = 0;
        state->selection.x = 0;  state->selection.y = 0;

        // destroyWindow(WINDOW_NAME);
        // state->img = cv::Mat();

        break;
    }
    return;
}

void  INThandler(int sig)
{
    printf("Exiting\n");
    if (state) delete state; 
    exit(0);
}


void
decode_image(const bot_core_image_t * msg, cv::Mat& img)
{
    int ch = msg->row_stride / (msg->width); 
    if (img.empty() || img.rows != msg->height || img.cols != msg->width)
        if (ch == 3) 
            img.create(msg->height, msg->width, CV_8UC3);
        else 
            img.create(msg->height, msg->width, CV_8UC1);
    //std::cerr << "msg: " << ch << " " << msg->row_stride << " " << msg->width << "x" << msg->height << std::endl;
        
    // extract image data
    switch (msg->pixelformat) {
    case BOT_CORE_IMAGE_T_PIXEL_FORMAT_RGB:
        memcpy(img.data, msg->data, sizeof(uint8_t) * msg->width * msg->height * 3);
        cv::cvtColor(img, img, CV_RGB2BGR);
        break;
    case BOT_CORE_IMAGE_T_PIXEL_FORMAT_MJPEG:
        // for some reason msg->row_stride is 0, so we use msg->width instead.
        if (ch == 3 || ch ==0) { 
            jpeg_decompress_8u_rgb(msg->data,
                                   msg->size,
                                   img.data,
                                   msg->width,
                                   msg->height,
                                   msg->width * 3);
            cv::cvtColor(img, img, CV_RGB2BGR);
        } else {  
            jpeg_decompress_8u_gray(msg->data,
                                    msg->size,
                                    img.data,
                                    msg->width,
                                    msg->height,
                                    msg->width);
        }
        break;
    case BOT_CORE_IMAGE_T_PIXEL_FORMAT_GRAY:
        memcpy(img.data, msg->data, sizeof(uint8_t) * msg->width * msg->height);
        break;
    default:
        fprintf(stderr, "Unrecognized image format\n");
        break;
    }
    return;
}

static void on_image_frame (const lcm_recv_buf_t *rbuf, const char *channel,
                            const bot_core_image_t *msg, 
                            void *user_data ) {

    if (!msg->width || !msg->height) return;
    
    if (!MAX_IMAGE_WIDTH || !MAX_IMAGE_HEIGHT) { 
        MAX_IMAGE_WIDTH = msg->width;
        MAX_IMAGE_HEIGHT = msg->height;
    }

    pthread_mutex_lock(&state->img_mutex);
    state_t* state = (state_t*) user_data; 
    if (state->img.empty() || state->img.rows != msg->height || state->img.cols != msg->width) { 
        if (msg->pixelformat == BOT_CORE_IMAGE_T_PIXEL_FORMAT_GRAY) { 
            std::cerr << "ERROR: Incoming image is grayscale!! Cannot perform color tracking!!!" << std::endl;
            assert(0);
        } else { 
            std::cerr << "One time creation of image" << std::endl;
            state->img.create(msg->height, msg->width, CV_8UC3);
        }
    }
    decode_image(msg, state->img);    
    state->img_utime = msg->utime; 
    pthread_mutex_unlock(&state->img_mutex);
    return;
}

static void on_result_frame (const lcm_recv_buf_t *rbuf, const char *channel,
                             const perception_image_roi_t *msg, 
                             void *user_data ) {
    pthread_mutex_lock(&state->img_mutex);
    state_t* state = (state_t*) user_data; 

    if(state->tracking_result != NULL){
        perception_image_roi_t_destroy(state->tracking_result);
    }
    state->tracking_result = perception_image_roi_t_copy(msg);

    state->tld_result.x = msg->roi.x * WINDOW_WIDTH; //MIN(msg->roi.x, state->origin.x);
    state->tld_result.y = msg->roi.y * WINDOW_HEIGHT;//MIN(msg->roi.y, state->origin.y);

    state->tld_result.width = msg->roi.width * WINDOW_WIDTH;
    state->tld_result.height = msg->roi.height * WINDOW_HEIGHT;
    state->tld_result &= Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    pthread_mutex_unlock(&state->img_mutex);
    return;
}

struct TLDSegmenterOptions { 
    bool vDEBUG;
    std::string vCHANNEL;

    TLDSegmenterOptions () : 
        vCHANNEL(std::string("CAMERALEFT")), vDEBUG(false) {}
};

TLDSegmenterOptions options;

void cross(Mat& disp, Rect rec, const Scalar& color, int thickness=1){
  Point p1((rec.x+rec.width/2) - 10, rec.y+rec.height/2), p2((rec.x+rec.width/2) + 10, rec.y+rec.height/2);
  Point p3(rec.x+rec.width/2, (rec.y+rec.height/2) -10), p4(rec.x+rec.width/2, (rec.y+rec.height/2)+10);
  line(disp, p1, p2, color, thickness);
  line(disp, p3, p4, color, thickness);

}

int main(int argc, char** argv)
{
    std::cout << "============= QUICK MODES ===================\n";
    std::cout << "drc-tld-segmenter -c CAMERALEFT\n";
    std::cout << "=============================================\n";

    ConciseArgs opt(argc, (char**)argv);
    opt.add(options.vCHANNEL, "c", "camera-channel","Camera Channel [CAMERALEFT]");
    opt.add(options.vDEBUG, "d", "debug","Debug mode");
    opt.parse();

    std::cerr << "===========  TLD Tracker ============" << std::endl;
    std::cerr << "=> CAMERA CHANNEL : " << options.vCHANNEL << std::endl;
    std::cerr << "=> DEBUG : " << options.vDEBUG << std::endl;
    std::cerr << "===============================================" << std::endl;

    // Install signal handler to free data.
    signal(SIGINT, INThandler);

    // Param server, botframes
    state = new state_t();

    printf("starting lcm thread\n");
    pthread_create(&(state->lcm_thread), NULL, lcm_thread_handler, state);

    cv::namedWindow( WINDOW_NAME );
    cv::setMouseCallback( WINDOW_NAME, onMouse, &mouse);

    // Subscriptions
    bot_core_image_t_subscribe(state->lcm, options.vCHANNEL.c_str(), on_image_frame, (void*)state);
    perception_image_roi_t_subscribe(state->lcm, "TLD_OBJECT_ROI_RESULT", on_result_frame, (void*)state);

    // Install signal handler to free data.
    signal(SIGINT, INThandler);

    while(1) { 
        unsigned char c = cv::waitKey(1) & 0xff;
	// lcm_handle(state->lcm);
        if (c == 'q') { 
            break;
            // } else if (c == 'd') { 
            //     FEATURE_ID++;
            // } else if (c == 'a') { 
            //     FEATURE_ID--;
        } else if (c == 'w') { 
            OBJECT_ID++;
        } else if (c == 's') { 
            OBJECT_ID--;
        }

        pthread_mutex_lock(&state->img_mutex);
        // UI handling 
        if (!state->img.empty()) { 
            cv::Mat display;
            cv::resize(state->img.clone(), display, cv::Size(WINDOW_WIDTH,WINDOW_HEIGHT)); 
            if (state->selection.width > 0 && state->selection.height > 0) {

                cv::Mat roi(display, state->selection);
                rectangle(display, state->selection, cv::Scalar(0,255,255), 2);
                cross(display, state->selection, cv::Scalar(0,255,255), 1);

                // bitwise_not(roi, roi);
            }
            if (state->selection_virtual.width > 0 && state->selection_virtual.height > 0) {
                
                cv::Mat roi(display, state->selection_virtual);
                rectangle(display, state->selection_virtual, cv::Scalar(0,255,0), 2);
                // bitwise_not(roi, roi);
            }

            if (state->tld_result.width > 0 && state->tld_result.height > 0) {
                cv::Mat roi(display, state->tld_result);
                rectangle(display, state->tld_result, cv::Scalar(0,0,255), 2);
                cross(display, state->tld_result, cv::Scalar(0,0,255), 1);
                // bitwise_not(roi, roi);
            }

            if(!state->img_selection.empty()){

                int border_width = 2;
                
                float sx = 1.f / WINDOW_WIDTH ; 
                float sy = 1.f / WINDOW_HEIGHT;
                
                int width = state->img_selection.cols / (sx * state->img.cols);
                int height = state->img_selection.rows / (sy * state->img.rows);

                int max_dim = fmax(width, height);
                
                double scale = 1.0;
                if(max_dim > MAX_THUMB_SIZE){
                    scale =  MAX_THUMB_SIZE / (double) max_dim;
                    width = width * scale;
                    height = height * scale;
                }

                cv::Mat img_sel;
                cv::resize(state->img_selection.clone(), img_sel, cv::Size(width, height)); 

                cv::Rect border(WINDOW_WIDTH - width - border_width , WINDOW_HEIGHT - height - border_width , width + border_width/2.0, height + border_width / 2.0);

                cv::Mat obj_roi = cv::Mat(display, cv::Rect(WINDOW_WIDTH - width - border_width , WINDOW_HEIGHT - height - border_width , width, height));
                
                img_sel.copyTo(obj_roi);

                cv::rectangle(display, border, cv::Scalar(100 ,0,255), border_width);

                //cv.
                //show this at the bottom somewhere
                
                //cv::imshow("Img Selection" , img_sel);
            }


            // Show OBJECT_ID, FEATURE_ID
            cv::putText(display, cv::format("OBJ: %ld", OBJECT_ID),
                        Point(20,20), 0, .5, cv::Scalar(0,200,0), 2);

            imshow(WINDOW_NAME, display);
        }
        if(0 && !state->img_selection.empty()){
            float sx = 1.f / WINDOW_WIDTH ; 
            float sy = 1.f / WINDOW_HEIGHT;

            int width = state->img_selection.cols / (sx * state->img.cols);
            int height = state->img_selection.rows / (sx * state->img.rows);
            
            cv::Mat img_sel;
            cv::resize(state->img_selection.clone(), img_sel, cv::Size(width, height)); 
            cv::imshow("Img Selection" , img_sel);
        }

        pthread_mutex_unlock(&state->img_mutex);
    }

    // if (state) delete state; 
    return 0;
}
