// rgb_commands.c - Command line functionality implementation
#include "qs-rgb.h"
#include "drivers/rgb.h"
#include "inc/hw_types.h"
#include "utils/ustdlib.h"
#include "utils/uartstdio.h"
#include "utils/cmdline.h"
#include "rgb_commands.h"

// Table of valid command strings, callback functions and help messages.

tCmdLineEntry g_sCmdTable[] =
{
    {"help",     CMD_help,      " : Display list of commands" },
    {"hib",      CMD_hib,       " : Place system into hibernate mode"},
    {"rand",     CMD_rand,      " : Start automatic color sequencing"},
    {"intensity",CMD_intensity, " : Adjust brightness 0 to 100 percent"},
    {"rgb",      CMD_rgb,       " : Adjust color 000000-FFFFFF HTML notation"},
    {"getframe", CMD_getFrame, " : Flashes a pixel buffer to the tri-LED" },
    { 0, 0, 0 }
};

const int NUM_CMD = sizeof(g_sCmdTable)/sizeof(tCmdLineEntry);

//*****************************************************************************
//
// Command: help
//
// Print the help strings for all commands.
//
//*****************************************************************************
int
CMD_help (int argc, char **argv)
{
    int index;
    
    (void)argc;
    (void)argv;
    
    UARTprintf("\n");
    for (index = 0; index < NUM_CMD; index++)
    {
      UARTprintf("%17s %s\n\n",
        g_sCmdTable[index].pcCmd,
        g_sCmdTable[index].pcHelp);
    }
    UARTprintf("\n"); 
    
    return (0);
}

//*****************************************************************************
//
// Command: hib
//
// Force the device into hibernate mode now.
//
//*****************************************************************************
int
CMD_hib (int argc, char **argv)
{
    (void) argc;
    (void) argv;
    AppHibernateEnter();

    return (0);
}

//*****************************************************************************
//
// Command: rand
//
// Starts the automatic light sequence immediately.
//
//*****************************************************************************
int
CMD_rand (int argc, char **argv)
{
    (void) argc;
    (void) argv;
    g_sAppState.ulMode = APP_MODE_AUTO;

    return (0);
}

//*****************************************************************************
//
// Command: intensity
//
// Takes a single argument that is between zero and one hundred. The argument 
// must be an integer.  This is interpreted as the percentage of maximum
// brightness with which to display the current color.  
//
//*****************************************************************************
int
CMD_intensity (int argc, char **argv)
{
    unsigned long ulIntensity;


    if(argc == 2)
    {
        ulIntensity = ustrtoul(argv[1], 0, 10);
        g_sAppState.fIntensity = ((float) ulIntensity) / 100.0f;
        RGBIntensitySet(g_sAppState.fIntensity);
    }

    return(0);

}

//*****************************************************************************
//
// Command: rgb
//
// Takes a single argument that is a string between 000000 and FFFFFF.
// This is the HTML color code that should be used to set the RGB LED color.
// 
// http://www.w3schools.com/html/html_colors.asp
//
//*****************************************************************************
int
CMD_rgb (int argc, char **argv)
{
    unsigned long ulHTMLColor;
    
    if(argc == 2)
    {
        ulHTMLColor = ustrtoul(argv[1], 0, 16);
        g_sAppState.ulColors[RED] = (ulHTMLColor & 0xFF0000) >> 8;
        g_sAppState.ulColors[GREEN] = (ulHTMLColor & 0x00FF00);
        g_sAppState.ulColors[BLUE] = (ulHTMLColor & 0x0000FF) << 8;
        g_sAppState.ulMode = APP_MODE_REMOTE;
        g_sAppState.ulModeTimer = 0;
        RGBColorSet(g_sAppState.ulColors);
    }
    
    return (0);

}


//*****************************************************************************
//
// Command: getframe
//
// Takes a single argument that is a string between 0 and 3.
// This is the pixel buffer that will be displayed
//*****************************************************************************
int
CMD_getFrame (int argc, char **argv)
{
    unsigned long ulHTMLColor;

    if(argc == 2)
    {
        ulHTMLColor = ustrtoul(argv[1], 0, 16);
        g_sAppState.ulColors[RED] = (ulHTMLColor & 0xFF0000) >> 8;
        g_sAppState.ulColors[GREEN] = (ulHTMLColor & 0x00FF00);
        g_sAppState.ulColors[BLUE] = (ulHTMLColor & 0x0000FF) << 8;
        g_sAppState.ulMode = APP_MODE_REMOTE;
        g_sAppState.ulModeTimer = 0;
        RGBColorSet(g_sAppState.ulColors);
    }

    return (0);

}
