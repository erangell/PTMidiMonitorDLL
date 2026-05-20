/****************************************/
/* MIDI MONITOR DLL version 2.02        */
/* for PowerTracks Pro Audio            */
/* by PG Music Inc.                     */
/*                                      */
/* Copyright 1998 Eric Rangell          */
/*                                      */
/* This program is in the public domain,*/
/* so it can be used without charge, and*/ 
/* you may create derivative works from */
/* it, as long as you acknowledge the   */
/* original copyright with the following*/
/* statement:                           */
/*                                      */
/* Midi Monitor technology adapted from:*/
/*"Midi Monitor DLL for PowerTracks Pro */
/* Audio" Copyright 1998, Eric Rangell. */
/*                                      */
/* This program comes with ABSOLUTELY   */
/* NO WARRANTY, without even the implied*/
/* warranty of MERCHANTABILITY or       */
/* FITNESS FOR ANY PARTICULAR PURPOSE.  */
/*                                      */
/* This DLL is used to graphically      */
/* display MIDI note, velocity, patch,  */
/* and pitch bend data as it plays in   */
/* real time through Powertracks Pro.   */
/*                                      */
/* This DLL should be copied to your    */
/* Powertracks directory (C:\PT) and    */
/* named $MIDIMON.DLL.  To start up the */
/* DLL, open up the mixer window of     */
/* PowerTracks Pro, click the DLL       */
/* button, select $MIDIMON.DLL, then    */
/* click the MIDIMON button to start    */
/* it.  You can then load a song and    */
/* start it playing, then press ALT-TAB */
/* to flip to the MIDIMON window where  */
/* it will be displayed graphically.    */
/*                                      */
/* Note data is shown as a bar in one   */
/* of the 16 midi channels, and the     */
/* length of the bar represents the     */
/* velocity.  The color of the bar      */
/* represents the patch bank.  Pitch    */
/* bends will display at the bottom     */
/* of each channel, above the channel   */
/* number.                              */
/*                                      */
/* This program uses the "Simplified    */
/* Windows Platform" code from "Windows */
/* Programming - an Introduction"       */
/* Copyright 1989 William H. Murray and */
/* Chris H. Pappas, published by Osborne*/
/* McGraw Hill.                         */
/*                                      */
/****************************************/

#include <windows.h>  
#include <string.h>
#include <stdio.h>
#include <stdlib.h>     
#include <ctype.h>
#include <mmsystem.h>                  
#include <commdlg.h>		// added 12/28/97 for open file dialog
#include "resource.h"		// added 12/28/97 for open file dialog

#define GRAPHICDISPLAY 1
#define TEXTDISPLAY 2
#define SHARPMODE 1
#define FLATMODE -1
#define MAXFADEEVENTS 500     
#define MAXTEMPOEVENTS 200
#define DEFAULTFADERATE 5   //default number of timer clicks for noteoff fades
#define TIMERINTERVAL 50        //milliseconds between timer events
              
#define DEFWNDLEFTX 0           // default window size and position
#define DEFWNDLEFTY 75
#define DEFWNDWIDTH 642         //number of X pixels available in window
#define DEFWNDHEIGHT 375        //number of Y pixels available in window
#define BORDERHEIGHT 25         //amount of space used for numbers, pitchbend
#define CTLBOXWIDTH 77          //horizontal allowance for ctl boxes
#define CTLBOXLABEL 12          //horizontal allowance for ctl box labels
#define DEFNUMCOLS 16           //number of columns to display


int MsgInProgress = 0;
int MsgPhase = 0; 
int MsgWMUSER = 0;
WPARAM MsgWParam = 0;
LPARAM MsgLParam = 0; 
LRESULT MsgResult = 0;
char MsgText[80];

RECT newrect;
                 
// Prototypes needed for MakeProcInstance

WORD FAR PASCAL __export ProcessEvent 
    (HDC hdc, int sts, int data1, int data2);
    
WORD FAR PASCAL __export TimerCallbackProc
    (HWND hwnd, WORD msg, int idevent, DWORD dwtime);        
        
WORD FAR PASCAL __export ProcessChar
    (HWND hwnd, WORD msg, int wParam, DWORD lParam);
   
WORD FAR PASCAL __export ChgKeySig
    (HWND hwnd, WORD msg, int WParam, DWORD lParam);

WORD FAR PASCAL __export ChgMode
    (HWND hwnd, WORD msg, int wParam, DWORD lParam);

WORD FAR PASCAL __export DrawCols
    (HWND hwnd, WORD msg, int wParam, DWORD lParam);
   
WORD FAR PASCAL __export DrawCtrlBoxes
    (HWND hwnd, WORD msg, int wParam, DWORD lParam);
                                                     
WORD FAR PASCAL __export OptimizeDisplay
    (HWND hwnd, WORD msg, int wParam, DWORD lParam);                                                     
                                                     
WORD FAR PASCAL __export ChangeTempo
   	(HWND hwnd, WORD msg, int wParam, DWORD lParam
   	, float tempochg, char multdiv);        
   
WORD FAR PASCAL __export ClearMainDisplay
	(HWND hwnd, WORD msg, int wParam, DWORD lParam);
   
WORD FAR PASCAL __export ResizeWindow 
	(int wndleftx, int wndlefty, int wndwidth, int wndheight);

WORD FAR PASCAL __export ProcessCtrlChg 
	(HDC myhdc, int sts, int data1, int data2);

// Prototype needed because of forward reference                                    

long FAR PASCAL WindowProc (HWND, unsigned, WORD, LONG);

static HWND hPowerTracks;

static struct TMidiInfo * MidiBuf;

HWND         hwnd;
MSG          msg;
WNDCLASS     wcSwp;   // Declare a window class for us

char szProgName[] = "PTMIDMON";


HINSTANCE       hOurInst;                     

PAINTSTRUCT     ps; //needed to create a device context for drawing
HDC             hdc;         

FARPROC  fpTimerCallback;   // callback function will be used for Timer event
       
int Launched = 0;      // used to check if the DLL has already been launched
double ProgColor[128];  // maps patches (program changes) to colors
double ChnlColor[16];    // holds color for each midi channel
                         // (set by program change messages)
            
RECT  ctrlval[16][6];   //used to save rectangles for controller settings
COLORREF ctrlcolr[16][6];

// Code for reading INI file:

/* MMONINI.C - code to read and decode INI file into ProgColor array */

char ColorArr[200][40]; // used to store data from the [Color Names] section
int  RedArr[200];
int  GreenArr[200];
int  BlueArr[200];    
int  ColorIndx = 0;

char PatchArr[200][40]; // used to store data from the [Patch Names] section
int  PatchLo[200];
int  PatchHi[200];
int  PatchIndx = 0;

int sfmode = FLATMODE;  // 1 = sharps -1 = flats
int mode = GRAPHICDISPLAY;

// Define event transform flags here.  These values will be set to 1
// if their keys appear in the INI file [Transforms] section.  These
// flags are used to determine whether to remap specific midi events.
int transform_softthru = 0;     //use this if you turn off PT midi thru mode
int transform_nodisplay = 0;    //use this to turn off midi monitor displays
int transform_at2vol = 0;       //whether to change aftertouch to volume
int transform_pr2vol = 0;       //whether to change chnl pressure to volume

// Arrays for note off history fadeout bars
int notewidth[16][128];
int noteinuse[16][128];
int NumFadeEvents = 0;
int FadeChannel[MAXFADEEVENTS];
int FadeNoteNum[MAXFADEEVENTS];       
int FadeCounter[MAXFADEEVENTS];
int FadeLeft[MAXFADEEVENTS];
int FadeRight[MAXFADEEVENTS];
int FadeYcoord[MAXFADEEVENTS];
int FadePhase[MAXFADEEVENTS];
int FadeRate[16];
double TempoTimes[MAXTEMPOEVENTS];
double TempoValues[MAXTEMPOEVENTS];

//Fader Phases: color and line widths to use for each phase.
double  p1color = 0xC0C0C0;
int     p1width = 2;
double  p2color = 0xA0A0A4;
int     p2width = 2;
double  p3color = 0xA0A0A4;
int     p3width = 1;
double  p4color = 0xA0A0A4;
int     p4width = 1;
double  p5color = 0x808080;
int     p5width = 1;

HMIDIOUT hMidiOut;
DWORD midimessage;

//Variables for window resizeability
int wndleftx = DEFWNDLEFTX;
int wndlefty = DEFWNDLEFTY;
int wndwidth = DEFWNDWIDTH;
int wndheight = DEFWNDHEIGHT;
int defnumcols = DEFNUMCOLS;
int deflownote = 21;    //low A on piano
int defhighnote = 108;  //high C on piano
int defcolwidth = 33;
int defcolheight = 358;
int chcolumn[17];
     
//Variables for optimize routine   
long midistat[256];
int panvalue[17];
int pansort[17];
int lownote[17];
int highnote[17];
int statcol[17];
int statcolct;                       

// 12/28/97 - Variables for text note coloring
int defTextSingleColor = 0;
double defTextColor = 0x00FF00;
int MouseInvisible = 0;

typedef struct TIndexedMIDIData
{
    long Index;
    long MIDIData;
    long Time;
    int  Duration;
    char * LyricText;
    long dw1;
    long dw2;
};

typedef struct TTrackInfo
{ 
    unsigned char PlayMuteFrozenStatus;
    char * Name;
    unsigned char Channel;
    unsigned char Key;
    unsigned char Vel;
    unsigned char Port;
    unsigned char Patch;
    unsigned char Bank;
    int Loop;
    long dw1;
    long dw2;
    int w1;
    int i1;
};
                            


//the ProgColor array from the main program will be populated with 
//color assignments from the INI file.

void GetColorName(char * s_line)
{                                                                
    char ch;
    char * cptr;
    char colrname[40];
    char redpart[10];
    char greenpart[10];
    char bluepart[10];
    int i, red, green, blue;
    
    //read non blank chars before equal sign for color name
    cptr = s_line;
    
    i=0;
    ch = *cptr;
    while ((ch != '=') && (ch != '\0') && (ch != '\n'))
    {              
    if ((ch != ' ') && (i < 40))
    {
        colrname[i]=ch;
        i++;       
    }
    cptr++;
    ch = *cptr;
    }
    colrname[i]='\0';
    
    if ((ch == '\0') || (ch == '\n'))
    {
        return;
    }
    cptr++;
    
    //read numbers between equal sign and comma for RED component
    
    i=0;
    ch = *cptr;
    while ((ch != ',') && (ch != '\0') && (ch != '\n'))
    {
    if ((ch != ' ') && (i < 10))
    {
        redpart[i]=ch;
        i++;       
    }
    cptr++;
    ch = *cptr;
    }
    redpart[i]='\0';
    
    if ((ch == '\0') || (ch == '\n'))
    {
        return;
    }   
    cptr++;
    
    red=atoi(redpart);
          
    //read numbers between commas for GREEN component
    i=0;
    ch = *cptr;
    while ((ch != ',') && (ch != '\0') && (ch != '\n'))
    {
        if ((ch != ' ') && (i < 10))
        {
            greenpart[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    greenpart[i]='\0';
    
    if (ch == '\0')
    {
        return;
    }
    cptr++;
    
    green=atoi(greenpart);
    
    //read numbers after last comma for BLUE component
    //stop reading at null character

    i=0;
    ch = *cptr;
    while ((ch != ',') && (ch != '\0') && (ch != '\n'))
    {
        if ((ch != ' ') && (i < 10))
        {
            bluepart[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    bluepart[i]='\0';
    
    blue=atoi(bluepart);

    //validate numbers - must be between 0 and 255
    red = red % 256;
    green = green % 256;
    blue = blue % 256;
    
    //store data in array

    if (ColorIndx < 200)
    {
        strcpy (ColorArr[ColorIndx],colrname);
        RedArr[ColorIndx]=red;
        GreenArr[ColorIndx]=green;
        BlueArr[ColorIndx]=blue;
    
        ColorIndx++;
    }
}

void GetPatchName(char * s_line)
{   
    //read non blank chars before equal sign for patch name
    //read number after equal sign for patch number
    //validate patch number - must be between 1 and 128
    //store data in array      
    
    char ch;
    char * cptr;
    char patchname[40];
    char patchnum [40];
    int i, patchlo, patchhi;
    
    //read non blank chars before equal sign for patch name
    cptr = s_line;
    
    i=0;
    ch = *cptr;
    while ((ch != '=') && (ch != '\0') && (ch != '\n'))
    {              
        if ((ch != ' ') && (i < 40))
        {
            patchname[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    patchname[i]='\0';
    
    if ((ch == '\0') || (ch == '\n'))
    {
        return;
    }
    cptr++;
    
    //read numbers after equal sign for patch number
    //stop reading at null character or '-' to indicate a range

    i=0;
    ch = *cptr;
    while ((ch != '-') && (ch != '\0')  && (ch != '\n'))
    {
        if ((ch != ' ') && (i < 40))
        {
            patchnum[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    patchnum[i]='\0';
    
    patchlo=atoi(patchnum);

    //validate numbers - must be between 1 and 128
    if (patchlo > 128)
    {
        patchlo = 128;
    }
    if (patchlo == 0)
    {   
        patchlo = 1;
    }
           
    if (ch == '-')// user wants patch name to apply to a range
    {
        cptr++;
        i=0;
        ch = *cptr;
        while ((ch != '\0')  && (ch != '\n'))
        {
            if ((ch != ' ') && (i < 40))
            {
                patchnum[i]=ch;
                i++;       
            }
            cptr++;
            ch = *cptr;
        }
        patchnum[i]='\0';
    
        patchhi=atoi(patchnum);

        //validate numbers - must be between 1 and 128
        if (patchhi > 128)
        {
            patchhi = 128;
        }
        if (patchhi == 0)
        {   
            patchhi = 1;
        }
    }
    else    // user doesn't want a range
    {
        patchhi = patchlo;
    }
           
           
           
    //store data in array
    
    if (PatchIndx < 200)
    {
        strcpy (PatchArr[PatchIndx],patchname);
        PatchLo[PatchIndx]=patchlo;
        PatchHi[PatchIndx]=patchhi;
        PatchIndx++;
    }
}

void GetPatchColor(char * s_line)
{
    char ch;
    char * cptr;
    char patchname[40];
    char colorname[40];
    int i, ixpatch, ixcolor, i_test;
    int iPatchNumLo, iPatchNumHi, ired, igreen, iblue, ixpat;

    //read non blank chars before equal sign for patch name
    
    cptr = s_line;
    
    i=0;
    ch = *cptr;
    while ((ch != '=') && (ch != '\0') && (ch != '\n'))
    {              
        if ((ch != ' ') && (i < 40))
        {
            patchname[i]=ch;
             i++;       
        }
        cptr++;
        ch = *cptr;
    }
    patchname[i]='\0';
    
    if ((ch == '\0') || (ch == '\n'))
    {
        return;
    }
    cptr++;
    
    //read non blank chars after equal sign for patch color
    //stop reading at null character

    i=0;
    ch = *cptr;
    while ((ch != ',') && (ch != '\0')  && (ch != '\n'))
    {
        if ((ch != ' ') && (i < 40))
        {
            colorname[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    colorname[i]='\0';
    
    //look up patch name to get patch number

    iPatchNumLo = 0;
    iPatchNumHi = 0;
    
    for (ixpatch = 0; ixpatch < PatchIndx; ixpatch++)
    {
        i_test = strcmp (patchname, PatchArr[ixpatch]);
    
        if (i_test == 0)
        {
            iPatchNumLo = PatchLo[ixpatch];
            iPatchNumHi = PatchHi[ixpatch];
            ixpatch = PatchIndx;    // break out of loop
        }
    }

    
    //look up color name to get it's RGB values

    ired     = 0;
    igreen   = 0;
    iblue    = 0;

    for (ixcolor = 0; ixcolor < ColorIndx; ixcolor++)
    {
        i_test = strcmp (colorname, ColorArr[ixcolor]);
    
        if (i_test == 0)
        {
            ired= RedArr [ixcolor];
            igreen   = GreenArr    [ixcolor];
            iblue    = BlueArr[ixcolor];
            ixcolor = ColorIndx;    // break out of loop
        }
    }
    
    //store RGB values for specified patch number range
    //subtract 1 to convert user's patch number to system patch number
    
    for (ixpat = iPatchNumLo ; ixpat <= iPatchNumHi ; ixpat++)
    {
        ProgColor [ ixpat - 1 ] = RGB (ired, igreen, iblue);
    }   

	//12/28/97 - new option to set text color
    if (strcmp(patchname,"text") == 0)
    {
		defTextColor = RGB (ired, igreen, iblue);
    }

    
}

void GetTransform(char * s_line)
{
    //read non blank chars before equal sign for transform name
    //read number after equal sign for flag value (0=off,nonzero=on)
    
    char ch;
    char * cptr;
    char transname[40];
    char transnum [40];
    int i, transfid;
    
    //read non blank chars before equal sign for patch name
    cptr = s_line;
    
    i=0;
    ch = *cptr;
    while ((ch != '=') && (ch != '\0') && (ch != '\n'))
    {              
        if ((ch != ' ') && (i < 40))
        {
            transname[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    transname[i]='\0';
    
    if ((ch == '\0') || (ch == '\n'))
    {
        return;
    }
    cptr++;
    
    //read numbers after equal sign for patch number
    //stop reading at null character or '-' to indicate a range

    i=0;
    ch = *cptr;
    while ((ch != '-') && (ch != '\0')  && (ch != '\n'))
    {
        if ((ch != ' ') && (i < 40))
        {
            transnum[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    transnum[i]='\0';
    
    transfid=atoi(transnum);

    if (transfid != 0)
    {   
        transfid = 1;
    }
           
    if (strcmp(transname,"SoftThru") == 0)
    {
        transform_softthru = transfid;
    }
    if (strcmp(transname,"NoDisplay") == 0)
    {
        transform_nodisplay = transfid;
    }
    if (strcmp(transname,"AfterTouchToVolume") == 0)
    {
        transform_at2vol = transfid;
    }
    if (strcmp(transname,"ChannelPressureToVolume") == 0)
    {
        transform_pr2vol = transfid;
    }
}

void GetFaders(char * s_line)
{
    char ch;
    char * cptr;
    char fadenum[40];
    char fadeval[40];
    int i, fadechnl, fadedata;
    
    //read non blank chars before equal sign for patch name
    
    cptr = s_line;
    
    i=0;
    ch = *cptr;
    while ((ch != '=') && (ch != '\0') && (ch != '\n'))
    {              
        if ((ch != ' ') && (i < 40))
        {
            fadenum[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    fadenum[i]='\0';
    
    if ((ch == '\0') || (ch == '\n'))
    {
        return;
    }
    cptr++;
    
    //read non blank chars after equal sign for patch color
    //stop reading at null character

    i=0;
    ch = *cptr;
    while ((ch != ',') && (ch != '\0')  && (ch != '\n'))
    {
        if ((ch != ' ') && (i < 40))
        {
            fadeval[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    fadeval[i]='\0';
    
    fadechnl = atoi(fadenum)-1;
    fadedata = atoi(fadeval);
    
    // Set fade rate only if reasonable numbers entered
    if ((fadechnl >= 0) && (fadechnl <=15) 
    && (fadedata >= 0) && (fadedata <= 100))
    {   
        FadeRate[fadechnl]=fadedata;
    }
}

void GetFaderPhaseColors(char * s_line)
{

    char ch;
    char * cptr;
    char phasenum[40];
    char phasecolor[40];
    int i, ixcolor, i_test;
    int ired, igreen, iblue;
    int phnum;
    
    //read non blank chars before equal sign for patch name
    
    cptr = s_line;
    
    i=0;
    ch = *cptr;
    while ((ch != '=') && (ch != '\0') && (ch != '\n'))
    {              
        if ((ch != ' ') && (i < 40))
        {
            phasenum[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    phasenum[i]='\0';
    
    if ((ch == '\0') || (ch == '\n'))
    {
        return;
    }
    cptr++;
    
    //read non blank chars after equal sign for patch color
    //stop reading at null character

    i=0;
    ch = *cptr;
    while ((ch != ',') && (ch != '\0')  && (ch != '\n'))
    {
        if ((ch != ' ') && (i < 40))
        {
            phasecolor[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    phasecolor[i]='\0';
    
    //look up color name to get it's RGB values

    ired     = 0;
    igreen   = 0;
    iblue    = 0;

    for (ixcolor = 0; ixcolor < ColorIndx; ixcolor++)
    {
        i_test = strcmp (phasecolor, ColorArr[ixcolor]);
    
        if (i_test == 0)
        {
            ired= RedArr [ixcolor];
            igreen   = GreenArr    [ixcolor];
            iblue    = BlueArr[ixcolor];
            ixcolor = ColorIndx;    // break out of loop
        }
    }
    
    //store RGB values for specified patch number range
    
    phnum=atoi(phasenum);
    switch (phnum)
    {
        case 1:
        {       
            p1color = RGB(ired,igreen,iblue);
            break;
        }
        case 2:
        {       
            p2color = RGB(ired,igreen,iblue);
            break;
        }
        case 3:
        {       
            p3color = RGB(ired,igreen,iblue);
            break;
        }
        case 4:
        {       
            p4color = RGB(ired,igreen,iblue);
            break;
        }
        case 5:
        {       
            p5color = RGB(ired,igreen,iblue);
            break;
        }
    }
}

void GetFaderPhaseWidths(char * s_line)
{
    char ch;
    char * cptr;
    char fadenum[40];
    char fadeval[40];
    int i, fadephase, fadewidth;
    
    //read non blank chars before equal sign for patch name
    
    cptr = s_line;
    
    i=0;
    ch = *cptr;
    while ((ch != '=') && (ch != '\0') && (ch != '\n'))
    {              
        if ((ch != ' ') && (i < 40))
        {
            fadenum[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    fadenum[i]='\0';
    
    if ((ch == '\0') || (ch == '\n'))
    {
        return;
    }
    cptr++;
    
    //read non blank chars after equal sign for patch color
    //stop reading at null character

    i=0;
    ch = *cptr;
    while ((ch != ',') && (ch != '\0')  && (ch != '\n'))
    {
        if ((ch != ' ') && (i < 40))
        {
            fadeval[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    fadeval[i]='\0';
    
    fadephase = atoi(fadenum);
    fadewidth = atoi(fadeval);
    
    // Set fade width only if reasonable numbers entered
    if ((fadephase >= 1) && (fadephase <= 5) 
    && (fadewidth >= 1) && (fadewidth <= 4))
    {   
        switch (fadephase)
        {
            case 1:
            {       
                p1width = fadewidth;
                break;
            }
            case 2:
            {       
                p2width = fadewidth;
                break;
            }
            case 3:
            {       
                p3width = fadewidth;
                break;
            }
            case 4:
            {       
                p4width = fadewidth;
                break;
            }
            case 5:
            {       
                p5width = fadewidth;
                break;
            }
        }    
    }
}


void GetColumnChannel(char * s_line)
{
    char ch;
    char * cptr;
    char colnum[40];
    char colchnl[40];
    int i, icolnum, icolchnl;
    
    //read non blank chars before equal sign for patch name
    
    cptr = s_line;
    
    i=0;
    ch = *cptr;
    while ((ch != '=') && (ch != '\0') && (ch != '\n'))
    {              
        if ((ch != ' ') && (i < 40))
        {
            colnum[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    colnum[i]='\0';
    
    if ((ch == '\0') || (ch == '\n'))
    {
        return;
    }
    cptr++;
    
    //read non blank chars after equal sign for patch color
    //stop reading at null character

    i=0;
    ch = *cptr;
    while ((ch != ',') && (ch != '\0')  && (ch != '\n'))
    {
        if ((ch != ' ') && (i < 40))
        {
            colchnl[i]=ch;
            i++;       
        }
        cptr++;
        ch = *cptr;
    }
    colchnl[i]='\0';
    
    icolnum = atoi(colnum);
    icolchnl = atoi(colchnl);
    
    if (strcmp(colnum,"NumCols") == 0)
    {
        defnumcols = icolchnl;
    }
    if (strcmp(colnum,"LowNote") == 0)
    {
        deflownote = icolchnl;
    }
    if (strcmp(colnum,"HighNote") == 0)
    {
        defhighnote = icolchnl;
    }

    //Store channel to be displayed for specified column
    if ((icolnum >= 1) && (icolnum <= 16)
    &&  (icolchnl >= 1) && (icolchnl <= 16))
    {
        chcolumn [icolchnl] = icolnum;
    }
}


void ProcessINIfile (char * lpsz_inifile)
{          
    int i_test, phase, i;

    FILE * ini_file;
    char s_line[80];
     
    ini_file = fopen (lpsz_inifile,"r");
    if (ini_file == NULL)
    {
        /* Use default colors */    
        MessageBox(hwnd,"Unable to open INI file.  You may load it manually by pressing the M key and entering the full pathname of the $MIDMON.INI file.  (Note: You may have to re-invoke the DLL first.) ",lpsz_inifile,MB_ICONEXCLAMATION);
        return;
    }
    else
    {
        /* Read colors from file */
        fgets (s_line, 79, ini_file);
    
        phase = 0;
        while (!feof(ini_file))
        {
            if ((phase == 1) && (*s_line != '['))
            {
                GetColorName (s_line);
            }
            if ((phase == 2) && (*s_line != '['))                                                
            {                         
                GetPatchName (s_line);
            }
            if ((phase == 3) && (*s_line != '['))
            {                         
                GetPatchColor (s_line);
            }
            if ((phase == 4) && (*s_line != '['))
            {                         
                GetTransform (s_line);
            }
            if ((phase == 5) && (*s_line != '['))
            {                         
                 GetFaders (s_line);
            }
            if ((phase == 6) && (*s_line != '['))
            {                         
                 GetFaderPhaseColors (s_line);
            }
            if ((phase == 7) && (*s_line != '['))
            {                         
                 GetFaderPhaseWidths (s_line);
            }
            if ((phase == 8) && (*s_line != '['))
            {
                 GetColumnChannel (s_line);
            }

    
            i_test = strcmp (s_line, "[Color Names]\n");   
            if (i_test == 0)
            {
                 phase = 1; //ColorNames
            }        
            i_test = strcmp (s_line, "[Patch Names]\n");
            if (i_test == 0)
            {
                 phase = 2; //PatchNames
            }                                         
            i_test = strcmp (s_line, "[Patch Colors]\n");
            if (i_test == 0)
            {
                 phase = 3; //PatchColors          

				 //12/28/97 - initialize default text color and mode
            	 defTextSingleColor = 0;
				 defTextColor = 0x00FF00;
            }                                         
            i_test = strcmp (s_line, "[Transforms]\n");
            if (i_test == 0)
            {
                phase = 4; //Transforms
            }                                         
            i_test = strcmp (s_line, "[Faders]\n");
            if (i_test == 0)
            {
                phase = 5; //Faders
            }                                         
            i_test = strcmp (s_line, "[FaderPhaseColors]\n");
            if (i_test == 0)
            {
                phase = 6; //Fader Phases
            }   
            i_test = strcmp (s_line, "[FaderPhaseWidths]\n");
            if (i_test == 0)
            {
                phase = 7; //Fader Phases
            }   
            i_test = strcmp (s_line, "[ColumnChannels]\n");
            if (i_test == 0)
            {
                phase = 8; //Column Channels      
                //Initialize any previous column channel assignments
                for (i=1;i<=16;i++) 
                {
                    chcolumn[i]=0;  
                }
                defnumcols = 16;
                deflownote = 21;    //low A on piano
                defhighnote = 108;  //high C on piano      
            }   
            fgets (s_line, 79, ini_file);
        }   
        fclose (ini_file);
    }                                  
    return;
}                       

WORD FAR PASCAL __export ResizeWindow (int wndleftx, int wndlefty, int wndwidth, int wndheight)
{    
    if (defnumcols > 0)	// prevent division by 0
    {
    	defcolwidth = (wndwidth - CTLBOXWIDTH - CTLBOXLABEL) / defnumcols;
    	defcolheight = wndheight - BORDERHEIGHT;
    }
    return 0;
}

int FAR PASCAL LibMain (HINSTANCE hInst, WORD wDataSeg
, WORD cbHeapSize, LPSTR lpszCmdLine)
{    
    int i, bank, red, green, blue, box, chnl;

    if (cbHeapSize != 0)
    UnlockData(0);          

    hOurInst = hInst;                             
                      
    // The MakeProcInstance statement allows future instances of the
    // ProcessEvent function to access global variables                                    
    // This is needed to update and read the Color Array
    MakeProcInstance (ProcessEvent, hInst);

    // This one is needed so the Timer has access to the MidiBuf
    // structure so it can determine whether or not to repaint the
    // window based on whether a new event was received.
    fpTimerCallback = MakeProcInstance (TimerCallbackProc, hInst);
    
    // This one is needed to process character messages from the keyboard
    MakeProcInstance (ProcessChar, hInst);
    
    // This one is used for the text monitor.
    MakeProcInstance (ChgKeySig, hInst);    
    
    MakeProcInstance (DrawCols, hInst);    
    MakeProcInstance (DrawCtrlBoxes, hInst);    
    
    MakeProcInstance (OptimizeDisplay, hInst); 
    
    MakeProcInstance (ChangeTempo, hInst); 

    MakeProcInstance (ClearMainDisplay, hInst); 
    MakeProcInstance (ResizeWindow, hInst); 
    MakeProcInstance (ProcessCtrlChg, hInst);

    // Initialize color array with default color (green).     
    for (i=0;i<16;i++)
    {    
        ChnlColor[i]=RGB(0,255,0);
    }

    // Initialize control value array
    for (box=0;box<=5;box++)
    {
        for (chnl=0;chnl<=15;chnl++)
        {
            SetRect(&ctrlval[chnl][box],0,0,0,0); 
            ctrlcolr[chnl][box]=RGB(0,0,0);
        }
    }


    //Initialize notewidth arrays
    for (i=0;i<128;i++)
    {       
        for (chnl=0;chnl<=15;chnl++)
        {       
            notewidth[chnl][i]=0;
            noteinuse[chnl][i]=0;
        }
    }
    for (chnl=0;chnl<=15;chnl++)
    {       
        FadeRate[chnl]=DEFAULTFADERATE;
    }
     
     
    // Initialize ProgColor array with defaults
    // (in case INI file is not found)
    for (i=0;i<128;i++)
    {
        bank = i / 8;
        switch (bank)
        {
            case 0:                      //red:piano
            {
                red = 255;
                green = 0;
                blue = 0;
                break;
            }
            case 1:                      //peach: chromatic percussion
            {   
                red = 255;
                green = 127;
                blue = 127;
                break;
            }
            case 2:                      //orange: organ
            {   
                red = 255;
                green = 127;
                blue = 0;
                break;
            }
            case 3:                      //green:guitar
            {   
                red = 0;
                green=255;
                blue=0;
                break;
            }
            case 4:                      //purple:bass      
            {   
                red=127;
                green=0;
                blue=127;
                break;
            }
            case 5:                      //pink:strings
            {   
                red=255;
                green=127;
                blue=255;
                break;
            }
            case 6:                      //dk pink:ensemble
            {     
                red=255;
                green=0;
                blue=127;
                break;
            }
            case 7:                      //yellow:brass
            {   
                red=255;
                green=255;
                blue=0;
                break;
            }
            case 8:                      //lt yellow:reeds
            {     
                red=255;
                green=255;
                blue=127;
                break;
            }
            case 9:                      //lt blue:pipes
            {     
                red=0;
                green=255;
                blue=255;
                break;
            }
            case 10:                     //dk green:synth lead
            {     
                red=0;
                green=127;
                blue=0;
                break;
            }
            case 11:                     //lt purple:synth pad
            {     
                red=255;
                green=0;
                blue=255;
                break;
            }
            case 12:                     //lt green:synth fx
            {     
                red=0;
                green=255;
                blue=127;
                break;
            }
            case 13:                     //lt blue:ethnic
            {     
                red=127;
                green=255;
                blue=255;
                break;
            }
            case 14:                  
            {                            //gray:percussive
                red=127;
                green=127;
                blue=127;
                break;
            }                 
            case 15:                     //white:sfx
            {     
                red=255;
                green=255;
                blue=255;
                break;
            }
        }
        ProgColor[i]= RGB(red,green,blue);
    }
    wndleftx = DEFWNDLEFTX;
    wndlefty = DEFWNDLEFTY;
    wndwidth = DEFWNDWIDTH;
    wndheight = DEFWNDHEIGHT;

    for (i=1;i<=16;i++) 
    {
        chcolumn[i]=0;
    }
            
    // Check if user set up an INI file to override default colors

    ProcessINIfile ("$MIDMON.INI");//function is in MMONINI.C
    
    ResizeWindow (wndleftx, wndlefty, wndwidth, wndheight);
                    
    //sprintf(debugmsg,"LeftX=%d lefty=%d width=%d height=%d defw=%d",
    //  wndleftx, wndlefty, wndwidth, wndheight, defcolwidth) ;
    //MessageBox(hwnd,debugmsg,"After Resize:"
    //      ,MB_OK | MB_ICONINFORMATION );
        
    return 1;
}
                              
int FAR PASCAL WEP(int nParameter)
{                    
    if (MouseInvisible == 1)
	{
       	ShowCursor(TRUE);
    	MouseInvisible = 0;
    }
    if (nParameter == WEP_SYSTEM_EXIT)
    {   
        return 1;
    }            
    else if (nParameter == WEP_FREE_DLL)
    {   
        return 1;
    }
    else
    {   
        return 1;
    }
}

// Powertracks DLL Structures

typedef struct TMidiEvent
{
    WORD Source;
    LONG Event;
};

typedef struct TMIDIEventArray 
{
    struct TMidiEvent MEvent[1001];
};

typedef struct TSysExArray
{
    BYTE SxArray[64001];
};

typedef struct TMidiInfo
{   
    struct TMIDIEventArray * TheMidiEventArray;
    WORD MIDIEventHead, MIDIEventTail;
    struct TSysExArray * SysExArray;
};
    

//This is called by PowerTracks when the user launches us

void FAR PASCAL _export LaunchTheDialog
    (HWND HWindow, int TheCurrentPart, HANDLE MidiPortHand,
    struct TMidiInfo * TheMidiInfo, BYTE ProgNum, BYTE TheLang)
{                          
    if (Launched > 0)
    {   
        MessageBeep(0);
        
        (HWindow, "You have already connected to the Midi Monitor.\n\nIf you exited it and want to reload it, click the DLL button on the Mixer Window and pick it again.", 
        "Already Launched", MB_OK);
        return;
    }
    
    Launched = 1;
    
    hPowerTracks = HWindow;
    MidiBuf = TheMidiInfo;  
    hMidiOut = MidiPortHand;
                    
    // To Test midioutshortmsg
    //DWORD midimessage;
    //midimessage = 0x90L + (0x40L * 0x100L) + (0x7FL * 0x10000L);
    //midiOutShortMsg ( hMidiOut, midimessage );
                       
    // Register window class
    wcSwp.lpszClassName    = szProgName;
    wcSwp.hInstance            = hOurInst;
    wcSwp.lpfnWndProc      = WindowProc;               
    wcSwp.hCursor         = LoadCursor (NULL, IDC_ARROW);
    wcSwp.hIcon               = LoadIcon     (hOurInst, szProgName);
    wcSwp.lpszMenuName     = NULL;
    wcSwp.hbrBackground    = GetStockObject(BLACK_BRUSH);
    wcSwp.style               = CS_HREDRAW | CS_VREDRAW;
    wcSwp.cbClsExtra       = 0;
    wcSwp.cbWndExtra       = 0;
    if (!RegisterClass (&wcSwp))
    {    
        return;
    }
    hwnd = CreateWindow (
          szProgName              //lpClassName window's class
        , "MIDI Monitor for PowerTracks Pro Audio" //lpWindowName   window's name
        , WS_OVERLAPPEDWINDOW       //dwStyle    window style (p81)
        , wndleftx                  //x             upper left X
        , wndlefty                  //y             upper left Y
        , wndwidth                  //nWidth     window width
        , wndheight                 //nHeight    window height
        , NULL                       //hWndParent  window's parent
        , NULL                       //hMenu          window's menu
        , hOurInst                    //hInstance       module for window
        , NULL                       //lpParam    data for WM_CREATE
        );
    
             
    GetClientRect(hwnd,&newrect);
    wndleftx = newrect.left;
    wndlefty = newrect.top;
    wndwidth = newrect.right - newrect.left;
    wndheight = newrect.bottom - newrect.top;
	ResizeWindow(wndleftx, wndlefty, wndwidth, wndheight);        
	
        
    ShowWindow (hwnd, SW_SHOWNORMAL); 
                    // Display the window, erase client area
                    // Usually SW_SHOWNORMAL
    
    UpdateWindow (hwnd);            // Force client area to be repainted
                    // with WM_PAINT

    if (SetTimer (hwnd, 1, TIMERINTERVAL, fpTimerCallback) == NULL)
    {   
        MessageBox (hwnd, "Couldn't create timer - exiting dll."
            ,"Notice!", MB_OK);
        return ;
    }
    MessageBeep(0);
    MessageBox(HWindow,
"Copyright 1998, Eric Rangell\nPress Alt+Tab to view monitor.\n\nThis program comes with ABSOLUTELY NO WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\nDO NOT USE IF SENSITIVE TO FLASHING PATTERNS"
,"PTPA Midi Monitor DLL ver 2.02", MB_OK);     
}


HWND FAR PASCAL _export GetPTHandle()
{
    return hPowerTracks;
}


int FAR PASCAL _export GetEvent (int * sts, int * data1, int * data2) 
/* Return 1 if got data, 0 if no data */
{   
    LONG ourevent;                         
    WORD ourtail;
    int done, rc;
            
    ourtail = MidiBuf->MIDIEventTail - 1 ;
    
    if (MidiBuf->MIDIEventHead == MidiBuf->MIDIEventTail)
    {    
        return 0;    /* head = tail - nothing in queue */
    }
           
    done = 0;
    while (done==0)
    {
        ourevent = MidiBuf->TheMidiEventArray->MEvent[ourtail].Event;
    
        *sts   = (int)(ourevent & 0x000000FF) ;
        *data1 = (int)((ourevent & 0x0000FF00) >> 8);
        *data2 = (int)((ourevent & 0x00FF0000) >> 16);
    
        ourtail++;
        if (ourtail == 1000)
        {    
            ourtail = 0;
        }

        MidiBuf->MIDIEventTail = ourtail + 1 ;
             
        if (MidiBuf->MIDIEventHead == MidiBuf->MIDIEventTail)
        {    
            done = 1;
            rc = 0;
        }
    
        /* Filter */
        if ((*sts >= 0x80) && (*sts < 0xA0))    //notes
        {   
            done = 1;
            rc = 1;
        }          
        if ((*sts >= 0xA0) && (*sts < 0xB0))    //aftertouch
        {   
            done = 1;
            rc = 1;
        }          
        if ((*sts >= 0xB0) && (*sts < 0xC0))    //controllers
        {   
            done = 1;
            rc = 1;
        }          
        if ((*sts >= 0xC0) && (*sts < 0xD0))    //patch changes
        {   
            done = 1;
            rc = 1;
        }          
        if ((*sts >= 0xD0) && (*sts < 0xE0))    //channel pressure
        {   
            done = 1;
            rc = 1;
        }          
        if ((*sts >= 0xE0) && (*sts < 0xF0))    //pitch bend
        {
            done = 1;
            rc = 1;          
        }
    }   
    return rc;
}
                               

void DrawChar (HDC hdc, char ch, int x, int y)
{       // x,y is bottom left corner of 5x7 character pixel matrix
    RECT myrect;        
    RECT FAR * pmyrect;

    switch (ch)
    {                             
        case 'C':               
        {                       
            MoveTo (hdc,x+4,y-1);
            LineTo (hdc,x+3,y);
            LineTo (hdc,x+1,y);
            LineTo (hdc,x,y-1);
            LineTo (hdc,x,y-5);
            LineTo (hdc,x+1,y-6);
            LineTo (hdc,x+3,y-6);
            LineTo (hdc,x+4,y-5);
            LineTo (hdc,x+4,y-4);   //for last pixel
            MoveTo (hdc,x,y);
            break;
        }
        case 'D':       
        {
            MoveTo (hdc,x+3,y);
            LineTo (hdc,x,y);
            LineTo (hdc,x,y-6);
            LineTo (hdc,x+3,y-6);
            LineTo (hdc,x+4,y-5);
            LineTo (hdc,x+4,y-1);      
            LineTo (hdc,x+3,y);
            MoveTo (hdc,x,y);
            break;
        }
        case 'E':
        {
            MoveTo (hdc,x+4,y);
            LineTo (hdc,x,y);
            LineTo (hdc,x,y-6);
            LineTo (hdc,x+4,y-6);
            LineTo (hdc,x+4,y-5);   // for last pixel
            MoveTo (hdc,x,y-3);             
            LineTo (hdc,x+4,y-3);
            MoveTo (hdc,x,y);
            break;
        }                       
        case 'F':
        {
            MoveTo (hdc,x,y);
            LineTo (hdc,x,y-6);
            LineTo (hdc,x+4,y-6);
            LineTo (hdc,x+4,y-5);   // for last pixel
            MoveTo (hdc,x,y-3);             
            LineTo (hdc,x+4,y-3);
            MoveTo (hdc,x,y);
            break;
        }
        case 'G':
        {
            MoveTo (hdc,x+4,y-5);
            LineTo (hdc,x+3,y-6);
            LineTo (hdc,x+1,y-6);
            LineTo (hdc,x,y-5);
            LineTo (hdc,x,y-1);
            LineTo (hdc,x+1,y);
            LineTo (hdc,x+3,y);
            LineTo (hdc,x+4,y-1);
            LineTo (hdc,x+4,y-3);
            LineTo (hdc,x+3,y-3);                   
            LineTo (hdc,x+2,y-3);   //for last pixel
            MoveTo (hdc,x,y);
            break;
        }
        case 'A':
        {
            MoveTo (hdc,x,y);               
            LineTo (hdc,x,y-4);
            LineTo (hdc,x+1,y-5);
            LineTo (hdc,x+2,y-6);
            LineTo (hdc,x+3,y-5);
            LineTo (hdc,x+4,y-4);
            LineTo (hdc,x+4,y);                      
            LineTo (hdc,x+3,y);     // for last pixel
            MoveTo (hdc,x,y-2);             
            LineTo (hdc,x+4,y-2);
            MoveTo (hdc,x,y);
            break;
        }
        case 'B':
        {
            MoveTo (hdc,x,y);
            LineTo (hdc,x,y-6);
            LineTo (hdc,x+3,y-6);
            LineTo (hdc,x+4,y-5);   
            LineTo (hdc,x+4,y-4);
            LineTo (hdc,x+3,y-3);   
            LineTo (hdc,x,y-3);             
            LineTo (hdc,x+3,y-3);
            LineTo (hdc,x+4,y-2);
            LineTo (hdc,x+4,y-1);
            LineTo (hdc,x+3,y);             
            LineTo (hdc,x,y);
            break;
        }                       
        case '#':
        {
            MoveTo (hdc,x+1,y-2);   LineTo (hdc,x+1,y-7); //note:adjusts
            MoveTo (hdc,x+3,y-2);   LineTo (hdc,x+3,y-7); // for last
            MoveTo (hdc,x,y-5);     LineTo (hdc,x+5,y-5); // pixel
            MoveTo (hdc,x,y-3);     LineTo (hdc,x+5,y-3);
            break;
        }
        case 'b':
        {
            MoveTo (hdc,x,y-5);
            LineTo (hdc,x,y);       
            LineTo (hdc,x+3,y);
            LineTo (hdc,x+4,y-1);
            LineTo (hdc,x+4,y-2);
            LineTo (hdc,x+3,y-3);   
            LineTo (hdc,x,y-3);
            break;
        }       
        case ' ':
        {
            pmyrect = &myrect;         
            SetRect(pmyrect, x, y-6, x+5, y+1);
            FillRect (hdc, pmyrect, GetStockObject(BLACK_BRUSH));
            break;
        }               
    }       
}                                                       


void DrawNote (HDC hdc, int ch, int n, int v)
{   
    // ch = channel (1-16)
    // n = note number.  
    // If v > 0 will draw note, if v=0 will erase note

    // Note: global variable sfmode determines sharps or flats
    // that variable is toggled by the left mouse button.

    int octave, x,y, sf, nnum, npos;
    char nchr;
    int column;   
    int maxl,maxr,midpt;

    if ((ch < 1) || (ch > 16))
    {
        return;
    }
    column = chcolumn[ch];
    if (column == 0) return;
    
    if ((n < 24) || (n > 105))
    {
        return;
    }
    
    octave = (n-24) / 12;    
    
    nnum = (n-24) % 12;

          
    switch (nnum)
    {
        case 0:
        {
            nchr = 'C';
            sf = 0;
            npos = 0;
            break;
        }
        case 1:
        {
            if (sfmode >= 0)
            {   
                nchr = 'C';
                sf = 1;
                npos = 0;
            }
            else
            {       
                nchr = 'D';
                sf = -1;
                npos = 0;
            }
            break;
        }
        case 2:
        {
            nchr = 'D';
            sf = 0;  
            npos = 1;
            break;
        }
        case 3:
        {
            if (sfmode >= 0)
            {       
                nchr = 'D';
                sf = 1;  
                npos = 1;
            }
            else
            {       
                nchr = 'E';
                sf = -1;
                npos = 1;
            }
            break;
        }
        case 4:
        {
            nchr = 'E';
            sf = 0;  
            npos = 2;
            break;
        }
        case 5:
        {
            nchr = 'F';
            sf = 0;  
            npos = 3;
            break;
        }
        case 6:
        {
            if (sfmode >= 0)
            {       
                nchr = 'F';
                sf = 1;  
                npos = 3;
            }
            else
            {       
                nchr = 'G';
                sf = -1;
                npos = 3;
            }
            break;
        }
        case 7:
        {
            nchr = 'G';
            sf = 0;  
            npos = 4;
            break;
        }
        case 8:
        {
            if (sfmode >= 0)
            {       
                nchr = 'G';
                sf = 1;  
                npos = 4;
            }
            else
            {       
                nchr = 'A';
                sf = -1;
                npos = 4;
            }
            break;
        }
        case 9:
        {
            nchr = 'A';
            sf = 0;  
            npos = 5;
            break;
        }
        case 10:
        {
            if (sfmode >= 0)
            {       
                nchr = 'A';
                sf = 1;
                npos = 5;
            }
            else
            {       
                nchr = 'B';
                sf = -1;
                npos = 5;
            }
            break;
        }
        case 11:
        {
            nchr = 'B';
            sf = 0;
            npos = 6;
            break;
        }   
    }
    
    //12/28/97 - try to put text in middle of column
    maxl=4;
    maxr=defcolwidth-1;
    midpt=(maxr-maxl)/2+maxl;
	x = (column-1) * defcolwidth + midpt - 8;
    //x = (column-1) * defcolwidth + 12; 
    
    y = 345 - (octave * 8 * 7) - (8 * npos);
          
    if (sf != 0)
    {
        y=y-4;       
        x=x+6;
    }
    
    if (v > 0)
    {       
        DrawChar (hdc, nchr, x, y);
        if (sf == 1)
        {
            x=x+6;
            DrawChar (hdc, '#', x, y);
        }    
        if (sf == -1)
        {       
            x=x+6;
            DrawChar (hdc, 'b', x, y);
        }    
    }
    else
    {
        DrawChar (hdc, ' ', x, y);
        if (sf != 0)
        {       
            x=x+6;
            DrawChar (hdc, ' ', x, y);
        }   
    }
}    

void ProcessTextNoteEvent (HDC myhdc, int sts, int data1, int data2)
{
    int chnl;
    int vel;
    double colr;

    HPEN        hpen, hpenold;
        
    if (transform_nodisplay == 1)
    {
        return;
    }

    chnl = sts % 16;          
    
    //12/28/97 - allow override of single color from INI file [Patch Colors]
    if (defTextSingleColor == 1)
    {
    	colr = defTextColor;
    }
	else
    {
    	colr = ChnlColor[chnl];
    }
     
    vel = data2;
    
    if ((sts >= 0x80) && (sts < 0x90))
    {
        vel = 0;
    }
    if ((sts >= 0x90) && (sts < 0xA0) && (data2 ==0))
    {
        vel = 0;
    }                                                
    
    if ((data1 >= 24) && (data1 <= 96))   //RESTRICT TO PIANO RANGE
    {
        SetMapMode (myhdc,MM_TEXT);
        hpen = CreatePen ( PS_SOLID, 1, (COLORREF)colr );
        hpenold = SelectObject(myhdc,hpen);

        DrawNote (myhdc, chnl+1, data1, vel);   //chgd from chnl+1
        
        SelectObject(myhdc,hpenold);
        DeleteObject(hpen);                                 
    }
}

                               
void MakeNoteFadeEvent(int chnl, int data1, int left, int right, int ycoord)
{
    int i, FreeFound;
    
    //find first free array element in notefade
    i=0;
    FreeFound = 0;
    while ((i<=NumFadeEvents) && (FreeFound == 0))
    {       
        if (FadeNoteNum[i] == 0)
        {       
            FreeFound = 1;
        }
        else
        {       
            i++;
        }
    }
    if (FreeFound == 0)
    {
        NumFadeEvents++;
        if (NumFadeEvents < MAXFADEEVENTS)
        {       
            i=NumFadeEvents;
        }
        else
        {       
            i=MAXFADEEVENTS;
        }
    }
    // store channel and note number
    FadeChannel[i]  = chnl;
    FadeNoteNum[i]  = data1;
    FadeCounter[i]  = FadeRate[chnl];
    FadeLeft[i]     = left;
    FadeRight[i]    = right;
    FadeYcoord[i]   = ycoord;          
    FadePhase[i]    = 5;
}


void ProcessNoteEvent (HDC myhdc, int sts, int data1, int data2)
{
    int chnl, left, right, ycoord1, ycoord2;
    int maxl, maxr, midpt, width, noteoffwidth;
    double factor;
    double colr;
    double noteoffcolor;
    int thisisnoteoff;
    int column;
    RECT    myrect;
    HBRUSH       hbrush, hbrushold;              
    HPEN        hpen, hpenold;
    int noterange, noteheight, notetop;
    
    if (transform_nodisplay == 1)
    {
        return;       
    }
    
    noterange = defhighnote - deflownote + 1 ;
    noteheight = (int)( (double)defcolheight / (double)noterange );
    notetop = (defcolheight - (noterange * noteheight)) / 2;
                   
    noteoffcolor = p1color;
    noteoffwidth = p1width;
    
    maxl=4;
    maxr=defcolwidth-1;
    midpt=(maxr-maxl)/2+maxl;
    factor = (double)((double)127/((double)defcolwidth-(double)maxl));      
        
    chnl = sts % 16;              
    column = chcolumn[chnl+1] - 1;
    if (chcolumn[chnl+1] == 0) return;

    width = (int)((double)data2 / factor);
    
    colr = ChnlColor[chnl];
    
    thisisnoteoff=0;
    // Check for Note Offs
    if (((sts >= 0x80) && (sts < 0x90))
    ||  ((sts >= 0x90) && (sts < 0xA0) && (data2 ==0)))
    {
        colr = noteoffcolor;
        width = notewidth[chnl][data1];
        noteinuse[chnl][data1] = 0;             
                     
        if ((data1 >= deflownote) && (data1 <= defhighnote))   //RESTRICT TO PIANO RANGE
        {   
            ycoord1= (defhighnote-data1)*noteheight+5+notetop;
            left=    defcolwidth*(column)+midpt-(width/2);
            right=   defcolwidth*(column)+midpt+(width/2);
            MakeNoteFadeEvent(chnl,data1,left+1,right-1,
                ycoord1+((int)(noteheight/2)) );
        }
        thisisnoteoff = 1;
    }
    else
    {
        notewidth[chnl][data1] = width;
        noteinuse[chnl][data1] = 1;
    }
             
    left=    defcolwidth*(column)+midpt-(width/2);
    right=   defcolwidth*(column)+midpt+(width/2);      

    if ((data1 >= deflownote) && (data1 <= defhighnote))   //RESTRICT TO PIANO RANGE
    {
        ycoord1= (defhighnote-data1)*noteheight+5+notetop;
        ycoord2= ycoord1 + noteheight;
    
        SetRect (&myrect, left, ycoord1, right, ycoord2);
    
        if (thisisnoteoff == 0)      
        {
            //Note on - erase first
            SetRect (&myrect
            ,defcolwidth*(column)+4,ycoord1
            ,defcolwidth*(column)+defcolwidth-1,ycoord2);
            hbrush = CreateSolidBrush( (COLORREF)0);
            hbrushold = SelectObject(myhdc,hbrush);
            FillRect (myhdc, &myrect, hbrush);    
            SelectObject(myhdc,hbrushold);
            DeleteObject(hbrush);
        
            //Now draw the note on
            SetRect (&myrect, left, ycoord1, right, ycoord2);
            hbrush = CreateSolidBrush( (COLORREF)colr);
            hbrushold = SelectObject(myhdc,hbrush);
            FillRect (myhdc, &myrect, hbrush);    
            SelectObject(myhdc,hbrushold);
            DeleteObject(hbrush);                                 
        }
        else
        {   
            //Note off: erase note-on rectangle
            hbrush = CreateSolidBrush ( 0 );
            hbrushold = SelectObject (myhdc,hbrush);
            FillRect (myhdc, &myrect, hbrush);      
            SelectObject(myhdc,hbrushold);
            DeleteObject(hbrush);   
            
            // Draw fader for first of 5 phases
            hpen = CreatePen ( PS_SOLID, noteoffwidth, (COLORREF)colr );
            hpenold = SelectObject(myhdc,hpen);
            MoveTo (myhdc,left+1, ycoord1+((int)(noteheight/2)));
            LineTo (myhdc,right-1,ycoord1+((int)(noteheight/2)));
            SelectObject(myhdc,hpenold);
            DeleteObject(hpen);
        }
    }
}



WORD FAR PASCAL __export ProcessCtrlChg (HDC myhdc, int sts, int data1, int data2)    
{                                           
    int chnl, left, right, ycoord1, ycoord2, found, redraw, midpt;
    int org, box, yped1, yped2;
    double colr;
    RECT    myrect;
    HBRUSH       hbrush, hbrushold;
          
    if (transform_nodisplay == 1)
    {
        return 0;
    }
          
    //org = 564; // x coordinate base
    org = wndleftx+wndwidth-CTLBOXWIDTH+8+1; 
    yped1 = 305; yped2 = 320;
    
    chnl = sts % 16;       
    
    colr = ChnlColor[chnl];
    
    redraw = 0;
    found = 1;
    switch (data1)
    {
        case 0x40:  //sustain pedal
        {
            //left = 605; right = 615; ycoord1 = 365; ycoord2 = 380;
            left = org+41; right = left+10; ycoord1 = yped1; ycoord2 = yped2;
            if (data2 == 0) 
            {
                colr = 0;
            }
            break;
        }
        case 0x42:  //sostenuto pedal
        {
            //left = 585; right = 595; ycoord1 = 365; ycoord2 = 380;     
            left = org+21; right = left+10; ycoord1 = yped1; ycoord2 = yped2;
            if (data2 == 0) 
            {
                colr = 0;
            }
            break;
        }
        case 0x43:  //soft pedal
        {
            //left = 565; right = 575; ycoord1 = 365; ycoord2 = 380;     
            left = org+1; right = left+10; ycoord1 = yped1; ycoord2 = yped2;
            if (data2 == 0) 
            {
                colr = 0;
            }
            break;
        }
        case 0x07:      // volume
        {
            left = org; right = org+(data2 / 2); 
            ycoord1 =   0 + 3*(chnl+1) -1; ycoord2 =   0 + 3*(chnl+1) +1; 
            redraw = 1;
            box=0;
            break;
        }
        case 0x01:      // modulation
        {
            left = org; right = org+(data2 / 2); 
            ycoord1 =  50 + 3*(chnl+1) -1; ycoord2 =  50 + 3*(chnl+1) +1;
            redraw = 1;
            box=1;
            break;
        }
        case 0x0A:      // panpot
        {
            // 0 = max left (L63), 64 = center(0), 127 = max right (R63)
            midpt = org+31;
        
            if (data2 < 64)
            {
                left = midpt - 31 + (data2/2);
                right = midpt ;
            }
            if (data2 > 64)
            {
                left = midpt;
                right = midpt + (data2 - 64)/2 ;
            }
            if (data2 == 64)
            {
                left = midpt-1;
                right = midpt+1;
            }

            ycoord1 = 100 + 3*(chnl+1) -1; ycoord2 = 100 + 3*(chnl+1) +1;
            redraw = 1;
            box=2;
            break;
        }
        case 0x5D:      // chorus
        {
            left = org; right = org+(data2 / 2); 
            ycoord1 = 150 + 3*(chnl+1) -1; ycoord2 = 150 + 3*(chnl+1) +1;
            redraw = 1;
            box=3;
            break;
        }
        case 0x5B:      // reverb
        {
            left = org; right = org+(data2 / 2); 
            ycoord1 = 200 + 3*(chnl+1) -1; ycoord2 = 200 + 3*(chnl+1) +1;
            redraw = 1;
            box=4;
            break;
        }
        case 0x0B:      //expression
        {         
            left = org; right = org+(data2 / 2); 
            ycoord1 = 250 + 3*(chnl+1) -1; ycoord2 = 250 + 3*(chnl+1) +1;
            redraw = 1;
            box=5;
            break;
        }
        default:      
        {
            found = 0;
            break;
        }
    }
    
    if (found == 1)
    {            
        if (redraw == 1)
        {
            // Erase old box
            SetRect (&myrect, org, ycoord1, org+63, ycoord2+1);
            hbrush = CreateSolidBrush( RGB(0,0,0) );
            hbrushold = SelectObject(myhdc,hbrush);          
            FillRect (myhdc, &myrect, hbrush);    
            SelectObject(myhdc,hbrushold);
            DeleteObject(hbrush);
        }
    
        // add 1 to right and bottom because fillrect doesn't include them
        SetRect (&myrect, left, ycoord1, right+1, ycoord2+1);
        hbrush = CreateSolidBrush( (COLORREF)colr);
        hbrushold = SelectObject(myhdc,hbrush);          
        FillRect (myhdc, &myrect, hbrush);    
        SelectObject(myhdc,hbrushold);
        DeleteObject(hbrush);     
    
        if (redraw == 1)
        {
            //Save controller value for when we need to redraw
            ctrlval[chnl][box].left = left - org;
            ctrlval[chnl][box].right = right+1 - org;
            ctrlval[chnl][box].top = ycoord1;
            ctrlval[chnl][box].bottom = ycoord2+1;
            ctrlcolr[chnl][box] = (COLORREF)colr;
        }
    }    
    return 0;
};

void ProcessAfterTouch (HDC myhdc, int sts, int data1, int data2)    
{                                           
    int chnl;

    // If SoftThru=1 and transform_at2vol = 1 then we will    
    // send a midi out message to change aftertouch to volume.
    
    chnl = sts % 16;       

    if (transform_at2vol == 1)
    {
        sts = 0xB0 + chnl;  //make it a control change event
        data1 = 7;          //volume controller.  leave data2 alone.
    
        // Draw the changed event on the midi monitor.
        ProcessCtrlChg (myhdc, sts, data1, data2);

        // Send the new message
        if (transform_softthru==1)
        {
            if ((sts >= 0xC0) && (sts < 0xE0))      // two byte message
            {    
                midimessage = (long)sts + ((long)data1 * 256) ;
            }
            else
            {
                midimessage = ((long)sts + ((long)data1 * 256) 
                    + ((long)data2 * 256 * 256));
            }
            midiOutShortMsg ( hMidiOut, midimessage );
        }
    }
};

void ProcessProgChg (HDC myhdc, int sts, int data1, int data2)    
{                                           
    int chnl;

    if (transform_nodisplay == 1)
    {   
        return;
    }
    chnl = sts % 16;
    ChnlColor[chnl]=ProgColor[data1];
};

void ProcessChannelPressure (HDC myhdc, int sts, int data1, int data2)    
{
    int chnl;
    
    // If SoftThru=1 and transform_at2vol = 1 then we will    
    // send a midi out message to change aftertouch to volume.
    
    chnl = sts % 16;       

    if (transform_pr2vol == 1)
    {
        sts = 0xB0 + chnl;  //make it a control change event
        data2 = data1;          //convert pressure value to vol ctrl value
        data1 = 7;          //volume controller.  
    
        // Draw the changed event on the midi monitor.
        ProcessCtrlChg (myhdc, sts, data1, data2);
    
        // Send the new message
        if (transform_softthru==1)
        {
            if ((sts >= 0xC0) && (sts < 0xE0))      // two byte message
            {    
                midimessage = (long)sts + ((long)data1 * 256) ;
            }
            else
            {    
                midimessage = ((long)sts + ((long)data1 * 256) 
                    + ((long)data2 * 256 * 256));
            }
            midiOutShortMsg ( hMidiOut, midimessage );
        }
    }
};


void ProcessPitchBend (HDC myhdc, int sts, int data1, int data2)
{                                        
    int chnl, left, right, ycoord1, ycoord2;
    int maxl, maxr, midpt, width;
    double factor;
    double colr;
    int column;
    RECT    myrect;
    HBRUSH       hbrush, hbrushold;
    
    if (transform_nodisplay == 1)
    {
        return;
    }

    maxl=4;
    maxr=defcolwidth-1;
    midpt=(maxr-maxl)/2+maxl;
    factor = (double)63/((double)defcolwidth-(double)maxl);
        
    chnl = sts % 16;              
    column = chcolumn[chnl+1] - 1;
    if (chcolumn[chnl+1] == 0) 
    {
        return;
    }
         
    if (data2 < 0x40)
    {    
        width= (int)(((double)64-(double)data2) / factor);
        left =   defcolwidth*(column)+midpt-(width/2);
        right=   defcolwidth*(column)+midpt;
        colr = ChnlColor[chnl];
    }   
    else
    {
        if (data2 > 0x40)
        {                     
            width= (int)(((double)data2-(double)0x40) / factor);
            left =   defcolwidth*(column)+midpt;
            right=   defcolwidth*(column)+midpt+(width/2);
            colr = ChnlColor[chnl];
        }
        else
        {   
            width = defcolwidth-1;
            left = defcolwidth*(column)+midpt-(width/2);
            right= defcolwidth*(column)+midpt+(width/2);
            colr = RGB(0,0,0); //black
        }
    }
    
    ycoord1= defcolheight+4; 
    ycoord2= defcolheight+9; 
    SetRect (&myrect, left, ycoord1, right, ycoord2);
    
    hbrush = CreateSolidBrush( (COLORREF)colr);
    hbrushold = SelectObject(myhdc,hbrush);
          
    FillRect (myhdc, &myrect, hbrush);    
    SelectObject(myhdc,hbrushold);
    DeleteObject(hbrush);                                 
};                  


WORD FAR PASCAL __export
DrawCols(HWND hwnd, WORD msg, int wParam, DWORD lParam)
{      
    int x,y, ch, half, mychnl, c2;   
    HPEN hpen, hpenold;

    if (transform_nodisplay == 1)
    {
        return 1;   
    }
    
    hdc = GetDC (hwnd);

    //sprintf(debugmsg,"defcolwidth=%d  defnumcols=%d"
    //  , defcolwidth, defnumcols) ;
    //MessageBox(hwnd,debugmsg,"Debug:"
    //      ,MB_OK | MB_ICONINFORMATION );

    hpen=CreatePen(PS_SOLID,2,RGB(255,255,255));        
    hpenold=SelectObject(hdc,hpen);
    
    for (ch=0;ch<=(defnumcols-1);ch++)
    {
        MoveTo(hdc,defcolwidth*ch+2,2);
        LineTo(hdc,defcolwidth*ch+2,defcolheight+2);
        LineTo(hdc,defcolwidth*ch+defcolwidth,defcolheight+2);
        LineTo(hdc,defcolwidth*ch+defcolwidth,2);
        LineTo(hdc,defcolwidth*ch+2,2);  
    }    
    
    half = defcolwidth / 2;
    y=defcolheight+10;               

    for (ch=1;ch <= defnumcols; ch++)
    {
        mychnl = 0;
        for (c2=1;c2 <=16;c2++)
        {       
            if (chcolumn[c2] == ch) 
            {
                mychnl=c2;
            }
        }
        
        x = wndleftx + defcolwidth * (ch-1) + half;

        switch (mychnl)
        {
            case 0:
            {   
                break;
            }
            case 1:
            {   
                MoveTo(hdc,x,y);
                LineTo(hdc,x,y+10);
                break;    
            } 
            case 2:
            {
                MoveTo(hdc,x,y);
                LineTo(hdc,x+5,y);
                LineTo(hdc,x+5,y+5);
                LineTo(hdc,x,y+5);
                LineTo(hdc,x,y+10);
                LineTo(hdc,x+5,y+10);
                break;
            }    
            case 3:
            {   
                MoveTo(hdc,x,y);
                LineTo(hdc,x+5,y);
                LineTo(hdc,x+5,y+10);
                LineTo(hdc,x,y+10);
                MoveTo(hdc,x,y+5);
                LineTo(hdc,x+5,y+5);
                break;
            }
            case 4:
            {
                MoveTo(hdc,x,y);
                LineTo(hdc,x,y+5);
                LineTo(hdc,x+5,y+5);
                LineTo(hdc,x+5,y);
                LineTo(hdc,x+5,y+10);
                break;
            }
            case 5:
            {    
                MoveTo(hdc,x+5,y);
                LineTo(hdc,x,y);
                LineTo(hdc,x,y+5);
                LineTo(hdc,x+5,y+5);
                LineTo(hdc,x+5,y+10);
                LineTo(hdc,x,y+10);
                break;
            }
            case 6:
            {
                MoveTo(hdc,x+5,y);
                LineTo(hdc,x,y);
                LineTo(hdc,x,y+10);
                LineTo(hdc,x+5,y+10);
                LineTo(hdc,x+5,y+5);
                LineTo(hdc,x,y+5);
                break;    
            }
            case 7:
            {
                MoveTo(hdc,x,y);
                LineTo(hdc,x+5,y);
                LineTo(hdc,x+5,y+10);
                break;    
            }
            case 8:
            {
                MoveTo(hdc,x+5,y);
                LineTo(hdc,x,y);
                LineTo(hdc,x,y+10);
                LineTo(hdc,x+5,y+10);
                LineTo(hdc,x+5,y+5);
                LineTo(hdc,x,y+5);
                MoveTo(hdc,x+5,y);
                LineTo(hdc,x+5,y+5);
                break;
            }
            case 9:
            {
                MoveTo(hdc,x,y+10);
                LineTo(hdc,x+5,y+10);
                LineTo(hdc,x+5,y);
                LineTo(hdc,x,y);
                LineTo(hdc,x,y+5);
                LineTo(hdc,x+5,y+5);
                break;    
            }
            case 10:
            {
                MoveTo(hdc,x,y);
                LineTo(hdc,x+5,y);
                LineTo(hdc,x+5,y+10);
                LineTo(hdc,x,y+10);
                LineTo(hdc,x,y);
                MoveTo(hdc,x-5,y);
                LineTo(hdc,x-5,y+10);
                break;
            }
            case 11:
            {
                MoveTo(hdc,x,y);
                LineTo(hdc,x,y+10);
                MoveTo(hdc,x-5,y);
                LineTo(hdc,x-5,y+10);
                break;                
            }
            case 12:
            {
                MoveTo(hdc,x,y);
                LineTo(hdc,x+5,y);
                LineTo(hdc,x+5,y+5);
                LineTo(hdc,x,y+5);
                LineTo(hdc,x,y+10);
                LineTo(hdc,x+5,y+10);
                MoveTo(hdc,x-5,y);
                LineTo(hdc,x-5,y+10);
                break;
            }
            case 13:
            {
                MoveTo(hdc,x,y);
                LineTo(hdc,x+5,y);
                LineTo(hdc,x+5,y+10);
                LineTo(hdc,x,y+10);
                MoveTo(hdc,x,y+5);
                LineTo(hdc,x+5,y+5);
                MoveTo(hdc,x-5,y);
                LineTo(hdc,x-5,y+10);
                break;
            }    
            case 14:
            {
                MoveTo(hdc,x,y);
                LineTo(hdc,x,y+5);
                LineTo(hdc,x+5,y+5);
                LineTo(hdc,x+5,y);
                LineTo(hdc,x+5,y+10);
                MoveTo(hdc,x-5,y);
                LineTo(hdc,x-5,y+10);
                break;
            }    
            case 15:
            {
                MoveTo(hdc,x+5,y);
                LineTo(hdc,x,y);
                LineTo(hdc,x,y+5);
                LineTo(hdc,x+5,y+5);
                LineTo(hdc,x+5,y+10);
                LineTo(hdc,x,y+10);
                MoveTo(hdc,x-5,y);
                LineTo(hdc,x-5,y+10);
                break;
            }    
            case 16:
            {
                MoveTo(hdc,x+5,y);
                LineTo(hdc,x,y);
                LineTo(hdc,x,y+10);
                LineTo(hdc,x+5,y+10);
                LineTo(hdc,x+5,y+5);
                LineTo(hdc,x,y+5);
                MoveTo(hdc,x-5,y);
                LineTo(hdc,x-5,y+10);
                break;
            }        
        } //switch
    } // for
    SelectObject(hdc,hpenold);
    DeleteObject(hpen);
}             

WORD FAR PASCAL __export
DrawCtrlBoxes (HWND hwnd, WORD msg, int wParam, DWORD lParam)
{      
    int x,y,i;           
    HPEN hpen, hpenold;
    HDC hdc;
    
    if (transform_nodisplay == 1)
    {
        return 1; 
    }
    
    hdc = GetDC(hwnd);
    hpen=CreatePen(PS_SOLID,1,RGB(255,255,255));        
    hpenold=SelectObject(hdc,hpen);
        
    x=wndleftx+wndwidth-CTLBOXWIDTH;
    y=0;
    
    MoveTo(hdc,x,y);
    LineTo(hdc,x+72,y);
    LineTo(hdc,x+72,y+300);
    LineTo(hdc,x,y+300);
    LineTo(hdc,x,y);         
    
    LineTo(hdc,x+8,y);
    LineTo(hdc,x+8,y+300);
    LineTo(hdc,x,300);
    
    LineTo(hdc,x,250);           
    LineTo(hdc,x+72,250);
    LineTo(hdc,x+72,200);
    LineTo(hdc,x,200);
    LineTo(hdc,x,150);
    LineTo(hdc,x+72,150);
    LineTo(hdc,x+72,100);
    LineTo(hdc,x,100);
    LineTo(hdc,x,50);
    LineTo(hdc,x+72,50);
    
    // Draw tick marks
    for (i=0;i<=250;i+=50)                        
    {
        MoveTo(hdc,x,y+i+3);    LineTo(hdc,x+2,y+i+3);
        MoveTo(hdc,x,y+i+6);    LineTo(hdc,x+4,y+i+6);
        MoveTo(hdc,x,y+i+9);    LineTo(hdc,x+2,y+i+9);
        MoveTo(hdc,x,y+i+12);   LineTo(hdc,x+6,y+i+12);
        MoveTo(hdc,x,y+i+15);   LineTo(hdc,x+2,y+i+15);
        MoveTo(hdc,x,y+i+18);   LineTo(hdc,x+4,y+i+18);
        MoveTo(hdc,x,y+i+21);   LineTo(hdc,x+2,y+i+21);
        MoveTo(hdc,x,y+i+24);   LineTo(hdc,x+8,y+i+24);
        MoveTo(hdc,x,y+i+27);   LineTo(hdc,x+2,y+i+27);
        MoveTo(hdc,x,y+i+30);   LineTo(hdc,x+4,y+i+30);
        MoveTo(hdc,x,y+i+33);   LineTo(hdc,x+2,y+i+33);
        MoveTo(hdc,x,y+i+36);   LineTo(hdc,x+6,y+i+36);
        MoveTo(hdc,x,y+i+39);   LineTo(hdc,x+2,y+i+39);
        MoveTo(hdc,x,y+i+42);   LineTo(hdc,x+4,y+i+42);
        MoveTo(hdc,x,y+i+45);   LineTo(hdc,x+2,y+i+45);
        MoveTo(hdc,x,y+i+48);   LineTo(hdc,x+8,y+i+48);
    }   
    
    //Draw labels for boxes: VOL, MOD, PAN, CHO, RVB
    x=x+6;
    
    //V
    MoveTo (hdc,x-15,10);       LineTo(hdc,x-13,15);    LineTo(hdc,x-10,9);
    //O
    MoveTo (hdc,x-15,20);       LineTo(hdc,x-11,20);    LineTo(hdc,x-11,25);
    LineTo (hdc,x-15,25);       LineTo(hdc,x-15,20);
    //L
    MoveTo (hdc,x-15,30);       LineTo(hdc,x-15,35);    LineTo(hdc,x-10,35);
    //M
    MoveTo (hdc,x-15,65);       LineTo(hdc,x-15,60);    LineTo(hdc,x-13,62);
    LineTo (hdc,x-11,60);       LineTo(hdc,x-11,66);
    //O
    MoveTo (hdc,x-15,70);       LineTo(hdc,x-15,75);    LineTo(hdc,x-11,75);
    LineTo (hdc,x-11,70);       LineTo(hdc,x-15,70);
    //D
    MoveTo (hdc,x-15,80);       LineTo(hdc,x-15,85);    LineTo(hdc,x-12,85);
    LineTo (hdc,x-11,84);       LineTo(hdc,x-11,81);    LineTo(hdc,x-12,80);
    LineTo (hdc,x-15,80);
    //P
    MoveTo (hdc,x-15,115);      LineTo(hdc,x-15,110);   LineTo(hdc,x-10,110);
    LineTo (hdc,x-10,113);      LineTo(hdc,x-15,113);
    //A
    MoveTo (hdc,x-15,125);      LineTo(hdc,x-15,120);   LineTo(hdc,x-10,120);
    LineTo (hdc,x-10,125);      LineTo(hdc,x-10,123);   LineTo(hdc,x-15,123);
    //N
    MoveTo (hdc,x-15,135);      LineTo(hdc,x-15,130);   LineTo(hdc,x-10,135);
    LineTo (hdc,x-10,129);
    //C
    MoveTo (hdc,x-10,160);      LineTo(hdc,x-15,160);   LineTo(hdc,x-15,165);
    LineTo (hdc,x-9,165);
    //H
    MoveTo (hdc,x-15,170);      LineTo(hdc,x-15,175);   LineTo(hdc,x-15,173);
    LineTo (hdc,x-10,173);      LineTo(hdc,x-10,175);   LineTo(hdc,x-10,169);
    //O
    MoveTo (hdc,x-15,180);      LineTo(hdc,x-15,185);   LineTo(hdc,x-11,185);
    LineTo (hdc,x-11,180);      LineTo(hdc,x-15,180);
    //R
    MoveTo (hdc,x-15,215);      LineTo(hdc,x-15,210);   LineTo(hdc,x-10,210);
    LineTo (hdc,x-10,213);      LineTo(hdc,x-15,213);   LineTo(hdc,x-9,216);
    //V
    MoveTo (hdc,x-15,220);      LineTo(hdc,x-13,225);   LineTo(hdc,x-10,219);
    //B
    MoveTo (hdc,x-15,230);      LineTo(hdc,x-15,236);   LineTo(hdc,x-11,236);
    LineTo (hdc,x-10,235);      LineTo(hdc,x-10,234);   LineTo(hdc,x-11,233);
    LineTo (hdc,x-15,233);      LineTo(hdc,x-11,233);   LineTo(hdc,x-10,232);
    LineTo (hdc,x-10,231);      LineTo(hdc,x-11,230);   LineTo(hdc,x-15,230);
    //E
    MoveTo (hdc,x-10,260);      LineTo(hdc,x-15,260);   LineTo(hdc,x-15,266);
    LineTo (hdc,x-9,266);       MoveTo(hdc,x-10,263);   LineTo(hdc,x-15,263);
    //X
    MoveTo (hdc,x-15,270);      LineTo(hdc,x-9,276);
    MoveTo (hdc,x-10,270);      LineTo(hdc,x-16,276);    
    //P
    MoveTo (hdc,x-15,285);      LineTo(hdc,x-15,280);   LineTo(hdc,x-10,280);
    LineTo (hdc,x-10,283);      LineTo(hdc,x-15,283);

    SelectObject(hdc,hpenold);
    DeleteObject(hpen);    
}

WORD FAR PASCAL __export ClearMainDisplay(HWND hwnd, WORD msg, int wParam, DWORD lParam)
{
    RECT myrect;
    HBRUSH hbrush,hbrushold;
    HDC myhdc;
    
    myhdc = GetDC(hwnd);                            
                            
    myrect.left = 0;
    myrect.right = wndleftx + wndwidth ;
    myrect.top = 0;
    myrect.bottom = wndheight;            
    
    hbrush = GetStockObject(BLACK_BRUSH);
    hbrushold = SelectObject(myhdc,hbrush);          
    FillRect (myhdc, &myrect, hbrush);    
    SelectObject(myhdc,hbrushold);
    DeleteObject(hbrush);
    
    
    DrawCols(hwnd,msg,wParam,lParam);         
    
    DrawCtrlBoxes(hwnd, msg, wParam, lParam);  
    
    return 0;   
} 


WORD FAR PASCAL __export ProcessEvent (HDC hdc, int sts, int data1, int data2)
{
    if ((sts >= 0x80) && (sts < 0xA0))
    {
        if (mode == TEXTDISPLAY)
        {    
            ProcessTextNoteEvent (hdc, sts,data1,data2);
        }
        else
        {
            ProcessNoteEvent (hdc, sts,data1,data2);
        }
    }

    if ((sts >= 0xA0) && ( sts < 0xB0))
    ProcessAfterTouch (hdc, sts,data1,data2);
    
    if ((sts >= 0xB0) && ( sts < 0xC0))
    ProcessCtrlChg (hdc, sts,data1,data2);
    
    if ((sts >= 0xC0) && ( sts < 0xD0))
    ProcessProgChg (hdc, sts,data1,data2);
    
    if ((sts >= 0xD0) && ( sts < 0xE0))
    ProcessChannelPressure (hdc, sts,data1,data2);

    if ((sts >= 0xE0) && (sts < 0xF0))
    ProcessPitchBend (hdc, sts,data1,data2);

    return 0;
}                         
        
WORD FAR PASCAL __export TimerCallbackProc
    (HWND hwnd, WORD msg, int idevent, DWORD dwtime)
{                                                                     
    int i, width, FadeToDraw;
    int left, right, ycoord1, ycoord2;
    double colr, fcolr;
    int fchnl, fcolm, fnnum, fleft, fright, fycoord;
    int thresh1,thresh2,thresh3,thresh4;
    RECT    myrect;
    HBRUSH  hbrush, hbrushold;
    HPEN    hpen,hpenold;
    int     goodcol;
    int noterange, noteheight, notetop;
    
    noterange = defhighnote - deflownote + 1 ;              
    if (noterange <= 0)	// prevent division by 0 or use of negative #s
    {
    	noterange = 1;
    }
    noteheight = (int)( (double)defcolheight / (double)noterange );
    notetop = (defcolheight - (noterange * noteheight)) / 2;
    
    hdc = GetDC (hwnd);// Create the device context
       
    FadeToDraw = 0;
    
    // See if we have note offs that need to be faded
    for (i=0;i<=NumFadeEvents;i++)
    {
        fchnl = FadeChannel[i];
        fcolm = chcolumn[fchnl + 1] - 1;
        if (chcolumn[fchnl+1] == 0)
        {    
            goodcol = 0;
        }
        else
        {
            goodcol = 1;
        }
        
        fnnum = FadeNoteNum[i]; 
        fleft = FadeLeft[i];
        fright = FadeRight[i];
        fycoord = FadeYcoord[i];                                          
        
        // defensive code put in to avoid EGPFault at 0001:3DE8 in MIDMON6
        // also only do fade for graphic bar display - not text display mode.
        if ((fcolm >= 0) && (fcolm <= (defnumcols-1) && (goodcol == 1)) 
        && (fnnum >=deflownote) && (fnnum <=defhighnote)
        && (mode == GRAPHICDISPLAY))
        {               
            if (noteinuse [fchnl][fnnum] == 0)
            {       
                //note not in use.  
                if (FadeCounter[i] == 0)
                {
                    //we have a note to be blacked out - it's done its time
                    width = notewidth [fchnl][fnnum] ;
            
                    if ((fnnum >= deflownote) 
                    &&  (fnnum <= defhighnote))   //PIANO RANGE
                    {
                        ycoord1= (defhighnote-fnnum)*noteheight+5+notetop;
                        ycoord2= ycoord1 + noteheight; 
                                
                        colr = 0;
                        left=    defcolwidth*(fcolm)+4;
                        right=   defcolwidth*(fcolm)+defcolwidth-1;     
                        SetRect (&myrect, left, ycoord1, right, ycoord2);
                        hbrush = CreateSolidBrush( (COLORREF)colr);
                        hbrushold = SelectObject(hdc,hbrush);
                        FillRect (hdc, &myrect, hbrush);    
                        SelectObject(hdc,hbrushold);
                        DeleteObject(hbrush);           
                    
                        //set up so we don't come back here
                        noteinuse [fchnl][fnnum] = 1;
                
                        FadeToDraw = 1;         
                    }               
                }
                else    //fadecounter <> 0 yet, subtract 1
                {
                    FadeCounter[i] --;
                }               
        
                //Check if fadecounter has crossed a threshold of 
                //20% 40% 60% 80% of the FadeRate for the channel.
                //If so, change the line style.                 
                thresh1 = (int)(FadeRate[fchnl]*0.80);
                thresh2 = (int)(FadeRate[fchnl]*0.60);
                thresh3 = (int)(FadeRate[fchnl]*0.40);
                thresh4 = (int)(FadeRate[fchnl]*0.20);
                if (FadeCounter[i] > 0)
                {   
                    if ((FadeCounter[i] < thresh1)
                    && (FadePhase[i] == 5))
                    {
                        ycoord1= (defhighnote-fnnum)*noteheight+5+notetop;
                        ycoord2= ycoord1 + noteheight; 

                        colr = 0;
                        left=    defcolwidth*(fcolm)+4;
                        right=   defcolwidth*(fcolm)+defcolwidth-1;     
                        SetRect (&myrect, left, ycoord1, right, ycoord2);
                        hbrush = CreateSolidBrush( (COLORREF)colr);
                        hbrushold = SelectObject(hdc,hbrush);
                        FillRect (hdc, &myrect, hbrush);    
                        SelectObject(hdc,hbrushold);
                        DeleteObject(hbrush);
            
                        fcolr = p2color;
                        hpen = CreatePen (PS_SOLID, p2width, (COLORREF)fcolr);
                        hpenold = SelectObject(hdc,hpen);
                        MoveTo (hdc,fleft,fycoord);
                        LineTo (hdc,fright,fycoord);
                        SelectObject(hdc,hpenold);
                        DeleteObject(hpen);                                 
                        FadeToDraw = 1;            
                        FadePhase[i] = 4;       
                        
                        //sprintf(debugmsg,"Phase 5 done");
                        //MessageBox(hwnd,debugmsg,"Phase 5"
                        //,MB_OK | MB_ICONINFORMATION );
                    }

                    if ((FadeCounter[i] < thresh2)
                    && (FadePhase[i] == 4))
                    {
                        ycoord1= (defhighnote-fnnum)*noteheight+5+notetop;
                        ycoord2= ycoord1 + noteheight; 

                        colr = 0;
                        left=    defcolwidth*(fcolm)+4;
                        right=   defcolwidth*(fcolm)+defcolwidth-1;     
                        SetRect (&myrect, left, ycoord1, right, ycoord2);
                        hbrush = CreateSolidBrush( (COLORREF)colr);
                        hbrushold = SelectObject(hdc,hbrush);
                        FillRect (hdc, &myrect, hbrush);    
                        SelectObject(hdc,hbrushold);
                        DeleteObject(hbrush);
            
                        fcolr = p3color;
                        hpen = CreatePen (PS_SOLID, p3width, (COLORREF)fcolr);
                        hpenold = SelectObject(hdc,hpen);
                        MoveTo (hdc,fleft,fycoord);
                        LineTo (hdc,fright,fycoord);
                        SelectObject(hdc,hpenold);
                        DeleteObject(hpen);                                 
                        FadeToDraw = 1;                 
                        FadePhase[i] = 3;             
                        
                        //sprintf(debugmsg,"Phase 4 done");
                        //MessageBox(hwnd,debugmsg,"Phase 4"
                        //,MB_OK | MB_ICONINFORMATION );
                    }

                    if ((FadeCounter[i] < thresh3)
                    && (FadePhase[i] == 3))
                    {
                        ycoord1= (defhighnote-fnnum)*noteheight+5+notetop;
                        ycoord2= ycoord1 + noteheight; 

                        colr = 0;
                        left=    defcolwidth*(fcolm)+4;
                        right=   defcolwidth*(fcolm)+defcolwidth-1;     
                        SetRect (&myrect, left, ycoord1, right, ycoord2);
                        hbrush = CreateSolidBrush( (COLORREF)colr);
                        hbrushold = SelectObject(hdc,hbrush);
                        FillRect (hdc, &myrect, hbrush);    
                        SelectObject(hdc,hbrushold);
                        DeleteObject(hbrush);

                        fcolr = p4color;
                        hpen = CreatePen (PS_SOLID, p4width, (COLORREF)fcolr);
                        hpenold = SelectObject(hdc,hpen);
                        MoveTo (hdc,fleft,fycoord);
                        LineTo (hdc,fright,fycoord);
                        SelectObject(hdc,hpenold);
                        DeleteObject(hpen);                                 
                        FadeToDraw = 1; 
                        FadePhase[i] = 2;             
                          
                        //sprintf(debugmsg,"Phase 3 done");
                        //MessageBox(hwnd,debugmsg,"Phase 3"
                        //,MB_OK | MB_ICONINFORMATION );
                    }

                    if ((FadeCounter[i] < thresh4)
                    && (FadePhase[i] == 2))
                    {
                        ycoord1= (defhighnote-fnnum)*noteheight+5+notetop;
                        ycoord2= ycoord1 + noteheight; 

                        colr = 0;
                        left=    defcolwidth*(fcolm)+4;
                        right=   defcolwidth*(fcolm)+defcolwidth-1;     
                        SetRect (&myrect, left, ycoord1, right, ycoord2);
                        hbrush = CreateSolidBrush( (COLORREF)colr);
                        hbrushold = SelectObject(hdc,hbrush);
                        FillRect (hdc, &myrect, hbrush);    
                        SelectObject(hdc,hbrushold);
                        DeleteObject(hbrush);

                        fcolr = p5color;
                        hpen = CreatePen (PS_SOLID, p5width, (COLORREF)fcolr);
                        hpenold = SelectObject(hdc,hpen);
                        MoveTo (hdc,fleft,fycoord);
                        LineTo (hdc,fright,fycoord);
                        SelectObject(hdc,hpenold);
                        DeleteObject(hpen);                                 
                        FadeToDraw = 1; 
                        FadePhase[i] = 1;
                    }                       
                }
            }           
            else    // note is in use - don't fade it.
            {      // a note on has probably reset the NoteInUse flag                      
                //free up the array element so it can be reused
                FadeChannel[i]=0;
                FadeNoteNum[i]=0;
            }
        }       //valid channel and note number
    }       // for loop

    // This IF statement was causing GPF's when it was in the WM_TIMER
    // logic, so it was moved to a callback function and makeprocinstance is
    // used in LibMain to make sure it gets the handle to the midibuf.
    
    if ((MidiBuf->MIDIEventHead != MidiBuf->MIDIEventTail)
    ||  (FadeToDraw == 1))
    {
        InvalidateRect(hwnd,NULL,FALSE);
        UpdateWindow(hwnd);    
    }       
    
    ReleaseDC (hwnd, hdc);                
    return 0;
}            
        

WORD FAR PASCAL __export ChgKeySig
   (HWND hwnd, WORD msg, int wParam, DWORD lParam)
{            
    sfmode *= -1;
    return 0;
}

WORD FAR PASCAL __export ChgMode
   (HWND hwnd, WORD msg, int wParam, DWORD lParam)
{            
    if (mode == TEXTDISPLAY)
    {
        mode = GRAPHICDISPLAY;
    }
    else
    {   
        mode = TEXTDISPLAY;
    }
    return 0;
}

WORD FAR PASCAL __export ChangeTempo
   (HWND hwnd, WORD msg, int wParam, DWORD lParam
   , float tempochg, char multdiv)
{            
    LRESULT rc;
    double songpos;
    int wasplaying;
    double numevents;
    long tempo, tempotime;
    int event;                 
    //char debugmsg[80];

    wasplaying = 0;
    rc=SendMessage(hPowerTracks,WM_USER+246,0,0); //is playback happening
    if (rc==1)
    {
        SendMessage(hPowerTracks,WM_USER + 215,0,0);    //stop                  
        wasplaying = 1;
    }
    rc=SendMessage(hPowerTracks,WM_USER+212,0,0); //save song position
    songpos = (double)rc;
    rc=SendMessage(hPowerTracks,WM_USER+213,0,0); //rewind

    rc=SendMessage(hPowerTracks,WM_USER+234,0,0); //get # tempo events
    numevents = (double)rc;
    
    if (numevents >= MAXTEMPOEVENTS)
    {
        MessageBox(hwnd,"Too many tempo events to use this function"
            	, "ERROR"
            	,MB_OK | MB_ICONSTOP ); 
        return 999;
    }

	for (event = 0 ; event <= (int)(numevents - 1); event ++)
    {
    	//get tempo time
    	rc=SendMessage(hPowerTracks,WM_USER+232,0,(LPARAM)event);
    	tempotime=(long)rc;
    	
    	//get tempo value
        rc=SendMessage(hPowerTracks,WM_USER+231,0,(LPARAM)event); 
        tempo = (long)rc;
    
    	if (multdiv == 'M')       
   		{
   			tempo = (long)((double)tempo * (double)tempochg);
   		}
   		else
   		{
   		    if (multdiv = 'D')
   			{
   				tempo = (long)((double)tempo / (double)tempochg);
   			}
   		}                
	    
	    if (tempotime == 0)
       	{
       		tempotime = 1;
       	}
       	
       	if (tempo < 10)
       	{
       		tempo = 10;
       	}
       	if (tempo > 500)
       	{
       		tempo = 500;
       	}
       	
		TempoTimes[event] = tempotime;
		TempoValues[event] = tempo;
	}
	
    for (event = (int)numevents-1 ; event >= 1; event --)
    {
        	//DeleteTempo - note- you cannot delete tempo event #0
        	rc=SendMessage(hPowerTracks,WM_USER+236,0,(LPARAM)event);
        	if (rc != 1)
        	{
        		MessageBox(hwnd,"Error deleting tempo event"
            	, "ERROR"
            	,MB_OK | MB_ICONSTOP ); 
        	}                                                             
   	       	//FOR DEBUGGING:
	       	//rc=SendMessage(hPowerTracks,WM_USER+206,0,0); //update screens
    	   	//sprintf(debugmsg,"Sent Delete Msg: Event=%d ",event);
       		//MessageBox(hwnd,debugmsg
           	//, "DEBUG"
           	//,MB_OKCANCEL | MB_ICONINFORMATION ); 

    }                                                        
    
    for (event = 0 ; event <= (int)(numevents - 1); event ++)
    {
    
    	tempotime = (long)TempoTimes[event];
    	tempo = (long)TempoValues[event];
    	
        //FOR DEBUGGING:
        //sprintf(debugmsg,"Event = %d Tempotime= %ld  Tempo= %ld"
        //	,event,tempotime,tempo);
        //MessageBox(hwnd,debugmsg
        //    	, "DEBUG"
        //    	,MB_OKCANCEL | MB_ICONINFORMATION ); 
        
   		
		//It seems like you have to set the song position to the spot where
		//you are changing the tempo, or it doesn't take effect properly.
		rc=SendMessage(hPowerTracks,WM_USER+213,0,(LPARAM)tempotime);    
		  
       	//InsertTempo
       	
       	rc=SendMessage(hPowerTracks,WM_USER+235
       		,(WPARAM)tempo,(LPARAM)tempotime);
       	if (rc != 1)
       	{
       		MessageBox(hwnd,"Error inserting tempo event"
           	, "ERROR"
           	,MB_OK | MB_ICONSTOP ); 
       	}                                                             
        
       	//FOR DEBUGGING:
       	//rc=SendMessage(hPowerTracks,WM_USER+206,0,0); //update screens
       	//sprintf(debugmsg,"Sent Insert Msg: Tempo=%ld Tempotime= %ld"
       	//,tempo,tempotime);
       	//MessageBox(hwnd,debugmsg
        //   	, "DEBUG"
        //   	,MB_OKCANCEL | MB_ICONINFORMATION ); 
        
    }
    
    rc=SendMessage(hPowerTracks,WM_USER+206,0,0); //update screens
    rc=SendMessage(hPowerTracks,WM_USER+213,0,(LPARAM)songpos); //restore position
    if (wasplaying == 1)
    {
        SendMessage(hPowerTracks,WM_USER + 214,0,0);    //play                  
    }
    return 0;
}


WORD FAR PASCAL __export OptimizeDisplay
    (HWND hwnd, WORD msg, int wParam, DWORD lParam)
{
    struct TTrackInfo * PMyTrackInfo;
    struct TIndexedMIDIData * PMyIndexedMIDIData;    
    long Result;
    int CurrTrack;
    long HighestTrack = 1;
    unsigned char * by;     
    char * * cptr;
    char * * cptr2;
    HGLOBAL hglb, hglb2;
    void FAR * lpvBuffer;
    void FAR * lpvBuffer2;
    int PlayMuteStatus, ForcedChannel;
    double NumEvents;
    int i,j, maxlow, maxhi, ix, temp;
    unsigned char midistatbyte, midichnlbyte, midinotebyte
    , mididata1, mididata2;
    FILE * outfile;     
      
    //For debugging:
	//char textmsg[80];

    outfile = fopen("\\MIDISTAT.OUT","w");
    if (outfile == NULL)
    {
        MessageBox(hwnd,"Cannot write to file \\MIDISTAT.OUT"
            , "ERROR"
            ,MB_OK | MB_ICONSTOP ); 
    	return 999;
    }
    
    //Initialize midistat array
    for (i=0 ; i<256; i++)
    {    
        midistat[i] = 0;
    }
    for (i=0; i<17; i++)
    {
        panvalue[i] = 0;
        pansort[i]  = 0;
        lownote[i]  = 127;
        highnote[i] = 0;
        statcol[i] = i;                                       
        
        //For debugging:
        //fprintf (outfile,"Lownote[%d]=%d\n",i,lownote[i]);
    }
    maxlow = 127;
    maxhi = 0;
    statcolct = 0;

    //Get highest track number
    HighestTrack = SendMessage(hPowerTracks,WM_USER + 204,0,0);

    //sprintf (textmsg,"The highest track is %d",HighestTrack);
    //MessageBox(hwnd,textmsg,"Optimize Routine:"
    //        ,MB_OK | MB_ICONINFORMATION );
    
    for (CurrTrack=1; CurrTrack<HighestTrack; CurrTrack++)
    {
        hglb = GlobalAlloc(GMEM_SHARE, 128);
        lpvBuffer = GlobalLock(hglb);
                                 
        //Set Track Number
        SendMessage(hPowerTracks,WM_USER + 201, CurrTrack, 0);
    
        PMyTrackInfo = lpvBuffer;
        by=(unsigned char *)PMyTrackInfo;                        
        by++;     
        cptr = (char * *)by;
        *cptr = (unsigned char *)lpvBuffer + 48;
    
        by = (unsigned char *)PMyTrackInfo;

        //sprintf (textmsg,"%2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X",
        //    *by,*(by+1),*(by+2),*(by+3),*(by+4),*(by+5),*(by+6),*(by+7)
        //    ,*(by+8),*(by+9),*(by+10),*(by+11),*(by+12),*(by+13),*(by+14),*(by+15)
        //    ,*(by+16),*(by+17),*(by+18),*(by+19),*(by+20),*(by+21),*(by+22),*(by+23)
        //    ,*(by+24),*(by+25),*(by+26),*(by+27),*(by+28),*(by+29),*(by+30),*(by+31)
        //    );
        //MessageBox(hwnd,textmsg,"Optimize Routine:"
        //    ,MB_OK | MB_ICONINFORMATION );  
            
        Result = SendMessage(hPowerTracks
                ,WM_USER + 210,0,(LPARAM)PMyTrackInfo); //GetTrackInfo

        by = (unsigned char *)PMyTrackInfo;
        
        //sprintf (textmsg,"Track %d %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X",
        //    CurrTrack, *by,*(by+1),*(by+2),*(by+3),*(by+4),*(by+5),*(by+6),*(by+7)
        //    ,*(by+8),*(by+9),*(by+10),*(by+11),*(by+12),*(by+13),*(by+14),*(by+15)
        //    ,*(by+16),*(by+17),*(by+18),*(by+19),*(by+20),*(by+21),*(by+22),*(by+23)
        //    ,*(by+24),*(by+25),*(by+26),*(by+27),*(by+28),*(by+29),*(by+30),*(by+31)
        //    );
        //MessageBox(hwnd,textmsg,"After sending message:"
        //    ,MB_OK | MB_ICONINFORMATION );  

        if (Result != 1)
        {   //sprintf (textmsg,"Return code %d for msg 210",Result);
            //MessageBox(hwnd,textmsg,"Error in Optimize Routine:"
            //    ,MB_OK | MB_ICONINFORMATION );
            return 999;
        }
    
        PlayMuteStatus = *by;
        ForcedChannel = *(by+5);        
        
        //For debugging:
        //sprintf(textmsg,"Track %d : Forced Channel = %d"
        //	,CurrTrack, ForcedChannel);
        //MessageBox(hwnd,textmsg,"Debugging info:"
        //    ,MB_OK | MB_ICONINFORMATION );  
        	
        
        if (PlayMuteStatus == 2)  // track is playable
        {
            Result = SendMessage(hPowerTracks
                ,WM_USER + 203,0,(LPARAM)PMyTrackInfo); //Get # events
            
            NumEvents = (double)Result;
                       
            hglb2 = GlobalAlloc(GMEM_SHARE, 128);
            lpvBuffer2 = GlobalLock(hglb2);
            PMyIndexedMIDIData = lpvBuffer2;
            by=(unsigned char *)PMyIndexedMIDIData;                        
            by += 14;
            cptr2 = (char * *)by;
            *cptr2 = (unsigned char *)lpvBuffer + 20;
            by = (unsigned char *)PMyIndexedMIDIData;

            for (ix = 0; ix < NumEvents; ix++)
            {
                PMyIndexedMIDIData->Index = ix;
                Result = SendMessage(hPowerTracks       //GetEvent
                    ,WM_USER + 209,0,(LPARAM)PMyIndexedMIDIData); 
                
                if (Result != 1)
                {   
                    //sprintf (textmsg,"Return code %d for msg 209",Result);
                    //MessageBox(hwnd,textmsg,"Error in Optimize Routine:"
                    //    ,MB_OK | MB_ICONINFORMATION );
                    return 999;
                }

                by = (unsigned char *)PMyIndexedMIDIData;
        
                //sprintf (textmsg,"Track %d %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X",
                //    CurrTrack, *by,*(by+1),*(by+2),*(by+3),*(by+4),*(by+5),*(by+6),*(by+7)
                //    ,*(by+8),*(by+9),*(by+10),*(by+11),*(by+12),*(by+13),*(by+14),*(by+15)
                //    ,*(by+16),*(by+17),*(by+18),*(by+19),*(by+20),*(by+21),*(by+22),*(by+23)
                //    ,*(by+24),*(by+25),*(by+26),*(by+27),*(by+28),*(by+29),*(by+30),*(by+31)
                //   );
                //MessageBox(hwnd,textmsg,"After sending GetEvent message:"
                //    ,MB_OK | MB_ICONINFORMATION );  

                midistatbyte = *(by+4);    
                mididata1    = *(by+5);
                mididata2    = *(by+6);
                  
				//12/28/97 - convert midi status byte to use forced channel
				// but make sure forced channel is valid first (PT allows 
				// you to set up a forced channel of 0)
				
                if (((midistatbyte % 0x10) != (ForcedChannel - 1)) 
                    && (ForcedChannel >= 1) && (ForcedChannel <= 16))
                {
                	midistatbyte = (midistatbyte / 0x10) * 0x10
                				 + (ForcedChannel - 1);        			
                }
                
                if ((midistatbyte >= 0x80) && (midistatbyte <= 0xff))
                {
                    midistat[midistatbyte] += 1;
                }
                if ((midistatbyte >= 0x90) && (midistatbyte <= 0x9f))
                {
                   	midichnlbyte = midistatbyte % 0x10;
                    midinotebyte = mididata1;
                    
                    if ((midinotebyte > 0) && (midinotebyte <= 0x7f))
                    {
                    	if (midinotebyte < lownote[midichnlbyte] )
                    	{
	                        lownote[midichnlbyte] = midinotebyte;
                    	}
                    	if (midinotebyte > highnote[midichnlbyte] )
                    	{
	                        highnote[midichnlbyte] = midinotebyte;
                    	}
                    }
                }
                if ((midistatbyte >= 0xB0) && (midistatbyte <= 0xBF))
                {
                   	midichnlbyte = midistatbyte % 0x10;
                                                        
                    if (mididata1 == 10)      // pan controller
                    {
                        if (panvalue[midichnlbyte] == 0)
                        {
                                panvalue[midichnlbyte] = mididata2;
                        }
                    }
                }
                
                
                //For Debugging:
                //fprintf (outfile,"Track=%2d  Sts=%02X  Data1=%3d  Data2=%3d  LowNote=%3d  HighNote=%3d  PanVal=%3d\n"
                //, CurrTrack, midistatbyte, mididata1, mididata2
                //,lownote[midichnlbyte], highnote[midichnlbyte],panvalue[midichnlbyte]);

            }  // for each event 
                        
            GlobalUnlock(hglb2);
            GlobalFree(hglb2);
             
        } // if track is playable
        
        GlobalUnlock(hglb);
        GlobalFree(hglb);

    } // for each track

    // Now use statistics to set up display
    for (i=0;i<16;i++)
    {
        if (midistat[9*16+i] > 0) 
        {
            statcolct++;
        }
        if (lownote[i] < maxlow) 
        {    
            maxlow = lownote[i];
        }
        if (highnote[i] > maxhi) 
        {    
            maxhi = highnote[i];
        }
    }

    for (i=0;i<16;i++)
    {       
        if (midistat[9*16+i] > 0)
        {    
            pansort[i] = panvalue[i];
        }
        else
        {
            pansort[i] = 128;
        }
    }
    for (i=14; i>=1; i--)        //bubble sort pan values
    {       
        for (j=0; j<=i; j++)
        {       
            if ( pansort[ statcol[j] ] > pansort[ statcol[j+1] ] )
            {       
                temp = statcol[j]  ;
                statcol[j] = statcol [j+1] ;
                statcol[j+1] = temp;
            }
        }
    }
    	                    
//	//Don't do redrawing here.    	                    
//  for (i=0 ; i<statcolct; i++)
//  {
//      chcolumn[ statcol[i]+1 ] = i+1;
//  }
//
//  defnumcols = statcolct;
//  deflownote = maxlow;
//  defhighnote = maxhi;
    
    
    // Write data to file C:\MIDISTAT.OUT
	fprintf(outfile,"[ColumnChannels]\nNumCols=%d\nLowNote=%d\nHighNote=%d\n"
	, statcolct, maxlow, maxhi);
	for (i=0 ; i<statcolct; i++)
   	{
       	fprintf(outfile,"%d=%d\n",i+1,statcol[i]+1);
    }     
    
    //FOR DEBUGGING:
    //for (i=128 ; i<=255; i++)
    //{
    //   	fprintf(outfile,"Stat [%2X]=%d\n",i,midistat[i]);
    //}     
    //for (i=0;i<16;i++)
    //{	
    // 	fprintf(outfile,"LowNote[%d]=%d\t",i,lownote[i]);
    // 	fprintf(outfile,"HighNote[%d]=%d\t",i,highnote[i]);
    // 	fprintf(outfile,"PanValue[%d]=%d\n",i,panvalue[i]);
    //}         
         
    fclose(outfile);
       
    return 0;
}   // optimize routine


WORD FAR PASCAL __export ProcessChar  
   (HWND hwnd, WORD msg, int wParam, DWORD lParam)
// (DWORD wParam, HDC myhdc)
{
    LRESULT rc;
    
    int box,chnl;
    RECT myrect;   
    HBRUSH hbrush,hbrushold;
    
    char textmsg[80];
    HDC myhdc;
    HCURSOR hcurSave;
    
    
    OPENFILENAME ofn;
	char szDirName[256];
	char szFile[256], szFileTitle[256];
	UINT  i, cbString;
	char  chReplace;    /* string separator for szFilter */
	char  szFilter[256];

	FILE * infile;
	FILE * outfile;
	char filechar; 
	int filerc, fileerror;    
	int org;
    
    myhdc=GetDC(hwnd);
        
    if ((wParam == 0x50) || (wParam == 0x70))       //VK_P
    {   
        SendMessage(hPowerTracks,WM_USER + 214,0,0);        //play                  
    }
    if ((wParam == 0x53) || (wParam == 0x73)) //VK_S
    {
        SendMessage(hPowerTracks,WM_USER + 215,0,0);    //stop
        if (MouseInvisible == 1)
        {
        	ShowCursor(TRUE);
        	MouseInvisible = 0;
        }
    }
    if (wParam == 0x20) // spacebar
    {
        rc=SendMessage(hPowerTracks,WM_USER+246,0,0); //is playback happening
        if (rc==1)
        {
            SendMessage(hPowerTracks,WM_USER + 215,0,0);    //stop                  
        	if (MouseInvisible == 1)
        	{
        		ShowCursor(TRUE);
        		MouseInvisible = 0;
        	}
        }
        else
        {
            SendMessage(hPowerTracks,WM_USER + 214,0,0);    //play                  
        }
    }
    if ((wParam == 0x57) || (wParam == 0x77)) //VK_W
    {
        SendMessage(hPowerTracks,WM_USER+213,0,0);      //rewind
    }       
    if ((wParam == 0x47) || (wParam == 0x67)) //VK_G
    {
        SendMessage(hPowerTracks,WM_USER+244,0,(LPARAM)&textmsg[0]);
        MessageBox(hwnd,textmsg,"The Current File Name Is:"
            ,MB_OK | MB_ICONINFORMATION );
    }                            

    if ((wParam == 0x4D) || (wParam == 0x6D)) //VK_M	//open MON file
    {
    	if (MouseInvisible == 1)
        {
        	ShowCursor(TRUE);
        	MouseInvisible = 0;
        }
    	
    	//Use GetOpenFileName to get a filename from user,
    	
    	rc=SendMessage(hPowerTracks,WM_USER+246,0,0); //is playback happening
        if (rc==1)
        {    
            MessageBox(hwnd,"Please stop playback before choosing this function."
            , "Open MON File"
            ,MB_OK | MB_ICONSTOP );   
        }
        else        
        {   //File Open code copied from Microsoft Visual C++ help screens.
        
	        //Start in the current directory 
			strcpy (szDirName, "\\");
			szFile[0] = '\0';
            
   			if ((cbString = LoadString(hOurInst, IDS_MONSTRING,
        		szFilter, sizeof(szFilter))) != 0) 
        	{

				chReplace = szFilter[cbString - 1]; /* retrieve wildcard */

				for (i = 0; szFilter[i] != '\0'; i++) 
				{
		    		if (szFilter[i] == chReplace)
       				{
		       			szFilter[i] = '\0';
       				}
				}

				/* Set all structure members to zero. */
	
				memset(&ofn, 0, sizeof(OPENFILENAME));

				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = szFilter;
				ofn.nFilterIndex = 1;
				ofn.lpstrFile= szFile;
				ofn.nMaxFile = sizeof(szFile);
				ofn.lpstrFileTitle = szFileTitle;
				ofn.nMaxFileTitle = sizeof(szFileTitle);
			
				//Use NULL to start at the current directory.
				ofn.lpstrInitialDir = NULL;
				//ofn.lpstrInitialDir = szDirName;
				
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
					  	| OFN_HIDEREADONLY;

				if (GetOpenFileName(&ofn)) 
				{
					//Copy the file to C:\MIDISTAT.OUT so it can be loaded with X
					
					infile = fopen(szFile, "r");				
					outfile = fopen("\\MIDISTAT.OUT","w");
 					if (infile == NULL)
 					{
	 					MessageBox(hwnd,"Cannot read input MON file"
            			, "ERROR"
            			,MB_OK | MB_ICONSTOP );	
 					}
 					else if (outfile == NULL)
    				{
	        			MessageBox(hwnd,"Cannot write to file \\MIDISTAT.OUT"
    	        		, "ERROR"
            			,MB_OK | MB_ICONSTOP );	
    				}
    				else
    				{
	    				fileerror = 0;
    					while (!feof(infile))
    					{	
	    					filechar = fgetc(infile);
    						filerc = fputc(filechar, outfile);
    						if (filerc == EOF)
    						{
	    						fileerror = 1;
    						}
    					}
						fclose(infile);
						fclose(outfile);
						if (fileerror == 1)
						{
							MessageBox(hwnd,"The MON file could not be copied to \\MIDISTAT.OUT."
	           				, "ERROR LOADING MON FILE"
            				,MB_OK | MB_ICONINFORMATION );	
						}
						else
						{
							MessageBox(hwnd,"The file was copied to \\MIDISTAT.OUT.  You can now press X twice to update the display."
            				, "File Successfullly Loaded"
            				,MB_OK | MB_ICONINFORMATION );	
            			}
					}
				}
        	}    
        	else
        	{
        		MessageBox(hwnd,"Filter String resource could not be loaded."
            	, "Internal Error in File Open function"
            	,MB_OK | MB_ICONSTOP );   
        	}
    	}
	}
	
	
    if ((wParam == 0x4E) || (wParam == 0x6E)) //VK_N	//open new song
    {
    	if (MouseInvisible == 1)
        {
        	ShowCursor(TRUE);
        	MouseInvisible = 0;
        }
    	
    	//Use GetOpenFileName to get a filename from user,
    	//then send Powertracks message #243 to open it.
    	
    	rc=SendMessage(hPowerTracks,WM_USER+246,0,0); //is playback happening
        if (rc==1)
        {    
            MessageBox(hwnd,"Please stop playback before choosing this function."
            , "Open File"
            ,MB_OK | MB_ICONSTOP );   
        }
        else        
        {   //File Open code copied from Microsoft Visual C++ help screens.
        
	        //Start in the current directory 
			strcpy (szDirName, "\\");
			szFile[0] = '\0';

			if ((cbString = LoadString(hOurInst, IDS_FILTERSTRING,
        		szFilter, sizeof(szFilter))) != 0) 
        	{
    		
				chReplace = szFilter[cbString - 1]; /* retrieve wildcard */

				for (i = 0; szFilter[i] != '\0'; i++) 
				{
	    			if (szFilter[i] == chReplace)
       				{
	       				szFilter[i] = '\0';
       				}
				}

				/* Set all structure members to zero. */

				memset(&ofn, 0, sizeof(OPENFILENAME));

				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = szFilter;
				ofn.nFilterIndex = 1;
				ofn.lpstrFile= szFile;
				ofn.nMaxFile = sizeof(szFile);
				ofn.lpstrFileTitle = szFileTitle;
				ofn.nMaxFileTitle = sizeof(szFileTitle);
			
				//Use NULL to start at the current directory.
				ofn.lpstrInitialDir = NULL;
				//ofn.lpstrInitialDir = szDirName;
				
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
						  | OFN_HIDEREADONLY;

				if (GetOpenFileName(&ofn)) 
				{
					rc = SendMessage(hPowerTracks,
                		WM_USER + 243, 0, (LPARAM)&szFile[0] );

                	if (rc == 1)
                	{
                		rc = MessageBox(hwnd, "Do you want to optimize the display?"
		            	, "File Opened", MB_YESNO | MB_ICONQUESTION);    
		            	
		    			if (rc == IDYES)
            			{   
            				//*** CODE DUPLICATED FROM 'O' FUNCTION BELOW ***
            			
			    			/* Set the cursor to the hourglass and save the previous cursor. */
			    			hcurSave = SetCursor(LoadCursor(NULL, IDC_WAIT));

                			//ClearMainDisplay(hwnd, msg, wParam, lParam);
                			rc = OptimizeDisplay(hwnd, msg, wParam, lParam);
                			//ResizeWindow(wndleftx, wndlefty, wndwidth, wndheight);
                			//InvalidateRect(hwnd,NULL,FALSE);
                			//UpdateWindow(hwnd);    
            
                			/* Restore the previous cursor. */
			    			SetCursor(hcurSave);                
			
						    if (rc == 0)
			    			{
			        			MessageBox(hwnd,"The optimization data was written to \\MIDISTAT.OUT.  Press the X key twice to use this data to redraw the Midi Monitor."
                    			, "Optimization Complete"
                    			,MB_OK | MB_ICONINFORMATION );   
                			}
                			else
                			{
            	    			MessageBox(hwnd,"I am sorry to report that an error occurred in the optimization routine.  Try using the DOS program MIDISTAT.EXE to optimize the song."
                    			, "Optimization Not Completed"
                    			,MB_OK | MB_ICONSTOP );   
                			}
            			}        	
		            }
		            else
		            {
		            	MessageBox(hwnd,szFile
            			, "An error occurred while opening file:"
            			,MB_OK | MB_ICONSTOP );   
		            }
	        	}
			}
			else
			{
				MessageBox(hwnd,"Filter String resource could not be loaded."
            	, "Internal Error in File Open function"
            	,MB_OK | MB_ICONSTOP );   
            }
        }    
    }                            

    if ((wParam == 0x54) || (wParam == 0x74)) //VK_T - toggle text color mode
    {
		if (defTextSingleColor == 0)
		{
			defTextSingleColor = 1;
		}
		else
		{
		 	defTextSingleColor = 0;
		}
    }

    if ((wParam == 0x49) || (wParam == 0x69)) //VK_I - make mouse invisible
    {
    	if (MouseInvisible == 0)
    	{
            rc = MessageBox(hwnd,
"This key will make the mouse cursor invisible. You'll need to remember to press the 'V' key to make it visible. If you don't do this and you switch to another program, you may have to restart Windows to see the mouse. Do you really want to do this?"
                , "Make Mouse Invisible"
                ,MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);   
            if (rc == 1)
            {
    			ShowCursor(FALSE);
    			MouseInvisible = 1;
    		}
    	}
   		else
   		{
   			ShowCursor(TRUE);  
    		MouseInvisible = 0;
   		}
    }

    if ((wParam == 0x56) || (wParam == 0x76)) //VK_V - make mouse visible
    {   
    	if (MouseInvisible == 1)
    	{
    		ShowCursor(TRUE);  
    		MouseInvisible = 0;
    	}
    }
                                                  
    if ((wParam == 0x4F) || (wParam == 0x6F)) //VK_O - optimize
    {   
    	if (MouseInvisible == 1)
        {
        	ShowCursor(TRUE);
        	MouseInvisible = 0;
        }
        rc=SendMessage(hPowerTracks,WM_USER+246,0,0); //is playback happening
        if (rc==1)
        {    
            MessageBox(hwnd,"Please stop playback before choosing this function."
            , "Optimize Display"
            ,MB_OK | MB_ICONSTOP );   
        }
        else
        {         
            rc = MessageBox(hwnd,"This function will optimize the number of columns and bar height for the song that is currently loaded into PowerTracks.  It may take a while to do this so please be patient.  Do you want to do this now?"
                , "Optimize Display"
                ,MB_OKCANCEL | MB_ICONQUESTION );   
                
            if (rc == 1)
            {     
			    /* Set the cursor to the hourglass and save the previous cursor. */
			    hcurSave = SetCursor(LoadCursor(NULL, IDC_WAIT));

                //ClearMainDisplay(hwnd, msg, wParam, lParam);
                rc = OptimizeDisplay(hwnd, msg, wParam, lParam);
                //ResizeWindow(wndleftx, wndlefty, wndwidth, wndheight);
                //InvalidateRect(hwnd,NULL,FALSE);
                //UpdateWindow(hwnd);    
            
                /* Restore the previous cursor. */
			    SetCursor(hcurSave);                
			
			    if (rc == 0)
			    {
			        MessageBox(hwnd,"The optimization data was written to \\MIDISTAT.OUT.  Press the X key twice to use this data to redraw the Midi Monitor."
                    , "Optimization Complete"
                    ,MB_OK | MB_ICONINFORMATION );   
                }
                else
                {
            	    MessageBox(hwnd,"I am sorry to report that an error occurred in the optimization routine.  Try using the DOS program MIDISTAT.EXE to optimize the song."
                    , "Optimization Not Completed"
                    ,MB_OK | MB_ICONSTOP );   
                }
            }
        }
    }

/* FOR TESTING - TO SEND MESSAGES TO POWERTRACKS MANUALLY
    if ((wParam == 0x4D) || (wParam == 0x6D)) //VK_M - messaging
    {
        if (MsgInProgress == 0)
        {
            MessageBox(hwnd,"Begin Recording Message: Msg# <tab> WParam <tab> LParam <M>","Message Start"
            ,MB_OK | MB_ICONINFORMATION );
            MsgInProgress = 1;
            MsgPhase = 1; 
            MsgWMUSER = 0;
            MsgWParam = 0;
            MsgLParam = 0;
        }
        else
        {   
            MsgInProgress = 0;
            MsgPhase = 0;

            sprintf (MsgText,"WM_USER+%d Wparam=%d LParam=%d",
                MsgWMUSER,MsgWParam,MsgLParam);
            
            rc = MessageBox(hwnd,MsgText,"Message Recorded. Send Now?"
                ,MB_OKCANCEL | MB_ICONINFORMATION );   
                
            if (rc == 1)
            {
                MsgResult = SendMessage(hPowerTracks,
                WM_USER + MsgWMUSER,MsgWParam,MsgLParam);
                
                sprintf (MsgText,"Return Value = %ld", MsgResult);
                
                MessageBox(hwnd,MsgText,"Message Was Sent"
                ,MB_OK | MB_ICONINFORMATION );
            }
        }
    }
    
    if ((wParam >= 0x30) && (wParam <= 0x39))   // Number
    {
        if (MsgPhase == 1)
        {
            MsgWMUSER *= 10;
            MsgWMUSER += wParam - 0x30;
        }
        if (MsgPhase == 2)
        {   
            MsgWParam *= 10;
            MsgWParam += wParam - 0x30;
        }
        if (MsgPhase == 3)
        {   
            MsgLParam *= 10;
            MsgLParam += wParam - 0x30;
        }
    }
    
    if (wParam == 0x09) // tab key
    {                    
        MsgPhase++;
    }
*/
                     
    if ((wParam == 0x52) || (wParam == 0x72)) //VK_R
    {
        // REDRAW LINES IN EACH BOX USING SAVED DATA
        for (box=0;box <=5; box++)
        {
            for (chnl=0; chnl <=15; chnl++)
            {
                myrect = ctrlval[chnl][box];    
                if ((myrect.left == 0) && (myrect.right ==0)
                    && (myrect.top ==0) && (myrect.bottom ==0))
                {
                }
                else
                {
                    //Adjust the coordinates to avoid overwriting message bar
                    //myrect.top += 30;
                    //myrect.bottom +=30;
                    
                    org = wndleftx+wndwidth-CTLBOXWIDTH+8+1;
                    myrect.left = myrect.left + org;
					myrect.right = myrect.right + org;

                    hbrush = CreateSolidBrush(ctrlcolr[chnl][box]);
                    hbrushold = SelectObject(myhdc,hbrush);          
                    FillRect (myhdc, &myrect, hbrush);    
                    SelectObject(myhdc,hbrushold);
                    DeleteObject(hbrush);
                }
            }
        }
    }
   
    if ((wParam == 0x48) || (wParam == 0x68))   //H - sharp mode
    {
        mode = TEXTDISPLAY;
        sfmode = SHARPMODE;
        ClearMainDisplay (hwnd, msg, wParam, lParam);
    }
    if ((wParam == 0x4C) || (wParam == 0x6C))   //L - lower case flats
    {
        mode = TEXTDISPLAY;
        sfmode = FLATMODE;           
        ClearMainDisplay (hwnd, msg, wParam, lParam);
    }
    if ((wParam == 0x42 || (wParam == 0x62)))   //B - graphic bars
    {
        if (mode == TEXTDISPLAY)
        {       
            mode = GRAPHICDISPLAY;
            ClearMainDisplay (hwnd, msg, wParam, lParam);
        }
    }    
    
    if ((wParam == 0x55 || (wParam == 0x75)))   //U - increase tempo up
    {                                            
    	/* Set the cursor to the hourglass and save the previous cursor. */
		hcurSave = SetCursor(LoadCursor(NULL, IDC_WAIT));
        
        ChangeTempo (hwnd, msg, wParam, lParam, (float)1.1,'M');

        /* Restore the previous cursor. */
		SetCursor(hcurSave);  
    }    
    if ((wParam == 0x44 || (wParam == 0x64)))   //D - decrease tempo down
    {
    	/* Set the cursor to the hourglass and save the previous cursor. */
		hcurSave = SetCursor(LoadCursor(NULL, IDC_WAIT));

        ChangeTempo (hwnd, msg, wParam, lParam, (float)1.1, 'D');

        /* Restore the previous cursor. */
		SetCursor(hcurSave);
    }    

    if ((wParam == 0x58 || (wParam == 0x78)))   //X - clear display
    {
        ClearMainDisplay(hwnd, msg, wParam, lParam);
        ProcessINIfile("\\midistat.out");    
        
        GetClientRect(hwnd,&newrect);
        wndleftx = newrect.left;
        wndlefty = newrect.top;
        wndwidth = newrect.right - newrect.left;
        wndheight = newrect.bottom - newrect.top;
        
        ResizeWindow(wndleftx, wndlefty, wndwidth, wndheight);
        InvalidateRect(hwnd,NULL,FALSE);
        UpdateWindow(hwnd);    
    }    

    if ((wParam == 0x5A) || (wParam == 0x7A))   //Z: zero out ctrl colors
    {
        for (box=0;box <=5; box++)
        {
            for (chnl=0; chnl <=15; chnl++)
            {       
                ctrlval[chnl][box].left = 0;
                ctrlval[chnl][box].right = 0;
                ctrlval[chnl][box].top = 0;
                ctrlval[chnl][box].bottom = 0;
                ctrlcolr[chnl][box] = 0;
            }
        }   
        //erase & redraw controller boxes
        myrect.left = wndleftx+wndwidth-CTLBOXWIDTH;
        myrect.right = wndleftx+wndwidth-CTLBOXWIDTH+72;
        myrect.top = 0;
        myrect.bottom = 300;
        hbrush = GetStockObject(BLACK_BRUSH);
        hbrushold = SelectObject(myhdc,hbrush);          
        FillRect (myhdc, &myrect, hbrush);    
        SelectObject(myhdc,hbrushold);
        DeleteObject(hbrush);
    
        DrawCtrlBoxes(hwnd, msg, wParam, lParam);
    }
    return 0;
}                               

// WINDOW FUNCTION - processes windows messages

long FAR PASCAL WindowProc (hwnd, messg, wParam, lParam)
    HWND    hwnd;    // which window invoked us
    unsigned messg;   // the window's message passed to us
    WORD    wParam;  // message data
    LONG    lParam;
{
    int sts,data1,data2;
            
    switch (messg)
    {
        case WM_PAINT:  
        {
            hdc = BeginPaint (hwnd, &ps);// Create the device context

            /************* YOUR ROUTINES BELOW **************/
                 
            GetClientRect(hwnd,&newrect);
            
            wndleftx = newrect.left;
            wndlefty = newrect.top;
            wndwidth = newrect.right - newrect.left;
            wndheight = newrect.bottom - newrect.top;
            
            ResizeWindow (wndleftx, wndlefty, wndwidth, wndheight);       
                     
            DrawCols(hwnd,messg,wParam,lParam);
            DrawCtrlBoxes(hwnd,messg,wParam,lParam);
        
            while (GetEvent (&sts,&data1,&data2))
            {                                               
                ProcessEvent (hdc, sts,data1,data2);
            } 

            /************* YOUR ROUTINES ABOVE **************/
        
            ValidateRect (hwnd, NULL);   // We're done with the WM_PAINT msg
            EndPaint (hwnd, &ps);       // Destroy the device context   
            break;
        }
    
        case WM_CHAR:
        {
            ProcessChar(hwnd, messg, wParam, lParam);
            break;     
        }
        case WM_LBUTTONDOWN:                                        
        {
            ChgKeySig(hwnd, messg, wParam, lParam);
            break;
        }
        case WM_RBUTTONDOWN:
        {
            ChgMode(hwnd, messg, wParam, lParam); 
            ClearMainDisplay (hwnd, messg, wParam, lParam); 
            break;
        }    

        
        case WM_DESTROY:
        {
           	if (MouseInvisible == 1)
	       	{
    	    	ShowCursor(TRUE);
        		MouseInvisible = 0;
        	}
        	
            //Don't post quit message because it exits PowerTracks
            break;
        }
        default:  // Have windows deal with the message itself
        {
            return ( DefWindowProc (hwnd, messg, wParam, lParam));
            break;
        }
    }
    return (0L);
}

/*** END OF PROGRAM ***/
