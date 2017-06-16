#define APP_SYSTICKS_PER_SEC            32
#define APP_BUTTON_POLL_DIVIDER          8
#define APP_NUM_MANUAL_COLORS            7
#define APP_PI                           3.1415926535897932384626433832795f
#define APP_AUTO_COLOR_STEP              (APP_PI/ 48.0f)
#define APP_INTENSITY_DEFAULT            0.3f
#define APP_AUTO_MODE_TIMEOUT            (APP_SYSTICKS_PER_SEC * 3)
#define APP_HIB_BUTTON_DEBOUNCE          (APP_SYSTICKS_PER_SEC * 3)
#define APP_HIB_FLASH_DURATION           2

#define APP_MODE_NORMAL                  0
#define APP_MODE_HIB                     1
#define APP_MODE_HIB_FLASH               2
#define APP_MODE_AUTO                    3
#define APP_MODE_REMOTE                  4

#define APP_INPUT_BUF_SIZE               128

// Represents a single pixel of color data
typedef struct
{
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
} sPixel_t;

typedef struct
{
	unsigned long ulFrames; // Number of frames in this buffer
	unsigned long ulFrameWidth; // Frame width in pixels
	unsigned long ulFrameHeight; // Frame width in pixels
	sPixel_t *pPixelData; // The pixel buffer
} sPixelBuffer_t;

// Structure typedef to make storing application state data to and from the 
// hibernate battery backed memory simpler.
//      ulColors:       [R, G, B] range is 0 to 0xFFFF per color.
//      ulMode:         The current application mode, system state variable.
//      ulButtons:      bit map representation of buttons being pressed
//      ulManualIndex:  Control variable for manual color increment/decrement
//      fColorWheelPos: Control variable to govern color mixing 
//      fIntensity:     Control variable to govern overall brightness of LED
typedef struct
{
    unsigned long ulColors[3];
    unsigned long ulMode;
    unsigned long ulButtons;
    unsigned long ulManualIndex;
    unsigned long ulModeTimer;
    float fColorWheelPos;
    float fIntensity;
}sAppState_t;


extern volatile sAppState_t g_sAppState;
extern void AppButtonHandler(void);
extern void AppRainbow(unsigned long ulForceUpdate);
extern void AppHibernateEnter(void);
