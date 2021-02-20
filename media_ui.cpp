/* -*- mode: c++; c-file-style: "k&r"; -*- */

#include <FL/Fl.H>
#include <FL/Enumerations.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Counter.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_JPEG_Image.H>
#include <FL/Fl_Pixmap.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pwd.h>
#include <gps.h>
#include <netdb.h>
#include <stdarg.h>
#include <getopt.h>
// libmpd
#include <mpd/client.h>
#include <mpd/status.h>
#include <mpd/entity.h>
#include <mpd/search.h>
#include <mpd/tag.h>
#include <mpd/message.h>
//

#include <vector>
#include <string>
#include <ctime>
#include <algorithm>
#include <regex>

// INI file support
#define SI_SUPPORT_IOSTREAMS
#include "SimpleIni.h"

#include "gpiod.h"
#include "sunriset.h"
#include "flhu.xpm"

using namespace std;

extern int errno;

static const uint8_t VERSION_MAJOR = 0;
static const uint8_t VERSION_MINOR = 99;
static const uint8_t VERSION_SUB   = 3;

static const int OPTION_MPD_SERVER    = 1000;
static const int OPTION_MPD_PORT      = 1001;
static const int OPTION_NO_GPIOD      = 1002;
static const int OPTION_GPIOD_PORT    = 1003;
static const int OPTION_GPS_SERVER    = 1004;
static const int OPTION_GPS_PORT      = 1005;
static const int OPTION_CONFIG        = 1006;
static const int OPTION_POS_DEFAULT   = 1007;
static const int OPTION_POS_LAT       = 1008;
static const int OPTION_POS_LONG      = 1009;

static const char short_options[] = "?hNs:p:G:P:W";
static const struct option long_options[] = {
  { "help"            , no_argument      , 0, 'h'                   },
  { "network-default" , required_argument, 0, 'N'                   },
  { "monkey-server"   , required_argument, 0, 's'                   },
  { "monkey-port"     , required_argument, 0, 'p'                   },
  { "gps-server"      , required_argument, 0, OPTION_GPS_SERVER     },
  { "gps-port"        , required_argument, 0, OPTION_GPS_PORT       },
  { "mpd-server"      , required_argument, 0, OPTION_MPD_SERVER     },
  { "mpd-port"        , required_argument, 0, OPTION_MPD_PORT       },
  { "gpiod-port"      , required_argument, 0, OPTION_GPIOD_PORT     },
  { "no-gpiod"        , no_argument      , 0, OPTION_NO_GPIOD       },
  { "windowed"        , no_argument      , 0, 'W'                   },
  { "config"          , no_argument      , 0, OPTION_CONFIG         },
  { "position-default", no_argument      , 0, OPTION_POS_DEFAULT    },
  { "latitude"        , required_argument, 0, OPTION_POS_LAT        },
  { "longitude"       , required_argument, 0, OPTION_POS_LONG       },
  { 0                 , no_argument      , 0,  0                    }
};

static const size_t RDS_LENGTH_MAX             = 128;
static const size_t NAME_LENGTH_MAX            = 64;
static const size_t TYPE_LENGTH_MAX            = 15;
static const size_t ENSEMBLE_LENGTH_MAX        = 256;
static const int32_t FM_FREQ_MIN               = 87500;
static const int32_t FM_FREQ_MAX               = 108000;
// Bastille, Paris
static const double DEFAULT_POSITION_LATITUDE  = 48.853164;
static const double DEFAULT_POSITION_LONGITUDE = 2.369137;


// DAB/FM browsers columns width
static const int DABBROWSER_PROGRAMNAME_WIDTH  = 515;
static int DABTabWidths[]                      = { 70, DABBROWSER_PROGRAMNAME_WIDTH, 185, 0 }; // Strenght, ProgramName, ProgramType

static const int FMBROWSER_PROGRAMNAME_WIDTH   = 220;
static int FMTabWidths[]                       = { 70, 180, FMBROWSER_PROGRAMNAME_WIDTH, 185, 0 }; // Strenght, QRG, ProgramName, ProgramType

static int PLTabWidths[]                       = { 300, 400, 0 }; // Artist, Title

static const string FLHUDIR                    = "/.flhu";

static string ScriptsDir;

/* Display resolution */
static const int32_t DISPLAY_WIDTH             = 1280; // minimal display width
static const int32_t DISPLAY_HEIGHT            = 360; // minimal display height 400: 40 diff correspond to the vertical shift of the FL_DOUBLE_WINDOW when created (not fullscreen) to see the tool bar of window manager.

/* Display Layout */
static const size_t ELB                        = 2; // Empty Left Border - Area left empty
static const size_t ETB                        = 2; // Empty Top Border - Area left empty
static const size_t ERB                        = 2; // Empty Right Border - Area left empty
static const size_t EBB                        = 2; // Empty Bottom Border - Area left empty
static const size_t DBW                        = 4; // Distance Between Widgets
static const size_t IBH                        = 18; // Info Box Height
static const size_t IBLS                       = 18; // Info Box Label Size 
static const size_t BIBH                       = 44; // Big Info Box Height 
static const size_t BIBLS                      = 28; // Big Info Label Size 
static const size_t CIBW                       = 110; // Clock Info Box Width
static const size_t SIBW                       = 110; // Speed Info Box Width
static const size_t RIBW                       = 100; // Rate Info Box Width
static const size_t TLS                        = 30;  // Tab Label Size
static const size_t BTS                        = 22;  // Browser Text Size
static const size_t SLS                        = 22;  // Slider Text Size
static const size_t TW                         = 40;  // Tab Width (DAB and FM)
static const size_t SBW                        = 40;  // Scroll Bar Width
static const size_t SBH                        = 40;  // Scroll Bar Height
static const size_t PSW                        = 280; // Player Seek Width
static const size_t PBW                        = 50;  // Player Button Width
static const size_t SPB                        = 20;  // Shift Player Buttons
static const size_t CFSW                       = 150; // Cross Fade Slider Width
static const size_t SCFS                       = 15;  // Shift Cross Fade Slider
static const size_t MSFS                       = 147; // Music Search Field Shift
static const size_t MSFW                       = 226; // Music Search Field Width
static const size_t KBH                        = 40; // Keyboard Button Heigth

/* Wait Icon */
static const float    WAIT_IMAGE_PAUSE         = 0.06;             // how long to hold each image

static const char     DEFAULT_SERVER_NAME[]    = "127.0.0.1";
static const uint32_t DEFAULT_SERVER_PORT      = 8888;
static const size_t   MAXBUF                   = 2048;
static const uint32_t FMFREQ_MIN               = 87500;
static const uint32_t FMFREQ_MAX               = 108000;

static const size_t STRENGTH_IMAGES_MAX = 8;

/* TrueType font setting */
static const Fl_Font MAC_FONT                  = Fl_Font((FL_FREE_FONT + 1));
static const Fl_Color SELECTION_COLOR          = 0x31313100;
static const Fl_Color SELECTION_KEYBOARD_COLOR = 0xbcbcbc00;


struct DABItem; // Opaque declaration


// Monkeyboard's frame types
typedef enum
{
     DAB,
     FM,
     OTHER
} ReplyType_t;

// Playlist modes
typedef enum
{
     PLMODE_PLAYLISTS = 0,
     PLMODE_TRACKS,
} PlListMode_t;

// MPD query types
typedef enum
{
     QUERY_ARTIST = 0,
     QUERY_ALBUM,
     QUERY_TITLE,
     QUERY_ANY,
     QUERY_FILE,
     QUERY_MAX
} QueryCategory_t;

// Theme types
typedef enum
{
     DAY,
     NIGHT
} DayTime_t;

// Implementation of scrolling text box, derived from Fl_Box
class Fl_Scrolling_Box : public Fl_Box
{
public:

     Fl_Scrolling_Box(int32_t X, int32_t Y, int32_t W, int32_t H, const char *l = NULL, useconds_t useconds = (1000 * 1000)) :
	  Fl_Box(X, Y, W, H, NULL),
	  m_useconds((useconds < 100000) ? 100000 : useconds),
	  m_label(NULL),
	  m_overflow(false),
	  m_maxOffset(0),
	  m_offset(0),
	  m_direction(true),
	  m_mutex(PTHREAD_MUTEX_INITIALIZER)
     {
	  Init(l);
     }

     Fl_Scrolling_Box(Fl_Boxtype b, int X, int Y, int W, int H, const char *l, useconds_t useconds) :
	  Fl_Box(b, X, Y, W, H, NULL),
	  m_useconds((useconds < 100000) ? 100000 : useconds),
	  m_label(NULL),
	  m_overflow(false),
	  m_maxOffset(0),
	  m_offset(0),
	  m_direction(true),
	  m_mutex(PTHREAD_MUTEX_INITIALIZER)
     {
	  Init(l);
     }

     ~Fl_Scrolling_Box()
     {
	  pthread_cancel(m_thread);
	  pthread_join(m_thread, NULL);
     }

     //
     // Set label, const char * variant
     // If the label is identical to the actual one
     // the function will just ignore the extra steps.
     // The widget stores a copy of the label, and handle
     // memory allocations/releases
     //
     void SetLabel(const char *label)
     {
	  pthread_mutex_lock(&m_mutex);

	  if (label)
	  {
	       // Identical, no need to redo
	       if (m_label && (strcmp(m_label, label) == 0))
	       {
		    pthread_mutex_unlock(&m_mutex);
		    return;
	       }

	       if (m_label)
	       {
		    free(m_label);
	       }

	       m_label = strdup(label);

	       CheckOverflow();
	  }
	  else
	  {
	       if (m_label)
	       {
		    free(m_label);
		    m_label = NULL;
	       }

	       m_direction = true;
	       m_maxOffset = 0;
	       m_offset = 0;
	       m_overflow = false;

	       Fl_Box::align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);
	       Fl_Box::label(m_label);
	  }

	  pthread_mutex_unlock(&m_mutex);
     }

     //
     // Set label, std::string variant
     //
     void SetLabel(std::string label)
     {
	  SetLabel(label.c_str());
     }

protected:
     //
     // Called from contructors
     //
     void Init(const char *l)
     {
 	  if (pthread_create(&m_thread, NULL, this->ThreadLabel, this) != 0)
 	  {
 	       perror("pthread_create");
 	       abort();
 	  }

	  SetLabel(l);
     }

     //
     // Virtual inherited
     //
     int handle(int h)
     {
	  return Fl_Box::handle(h);
     }

     //
     // A resize event arose
     //
     void resize(int x, int y, int w, int h)
     {
	  Fl_Box::resize(x, y, w, h);

	  pthread_mutex_lock(&m_mutex);
	  CheckOverflow();
	  pthread_mutex_unlock(&m_mutex);
     }

     //
     // Check if the label is overflowing the widget width
     // In this case, the horizontal scrolling will be set
     //
     void CheckOverflow()
     {
	  if (m_label)
	  {
	       m_direction = true;
	       m_maxOffset = 0;
	       m_offset = 0;
	       m_overflow = false;

	       // Doing some measurements
	       fl_font(MAC_FONT, IBLS);

	       double lW = fl_width(m_label);
	       double wW = Fl_Box::w() - 10;

	       m_overflow = (lW >= wW);

	       if (m_overflow)
	       {
		    char *p = m_label + 1;

		    // look to the max string length
		    while (*p != '\0')
		    {
			 if (fl_width(p) < wW)
			 {
			      break;
			 }

			 m_maxOffset++;
			 p++;
		    }

		    // Unable to find the maxOffset (paranoid node ON)
		    if (m_maxOffset == 0)
		    {
			 // didn't found;
			 m_overflow = false;
		    }
		    else
		    {
			 m_maxOffset++;
			 Fl_Box::align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
		    }
	       }

	       if (!m_overflow)
	       {
		    Fl_Box::align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);
		    Fl_Box::label(m_label);
	       }
	  }
     }

     //
     // Horizontal scrolling thread
     //
     static void *ThreadLabel(void *data)
     {
	  Fl_Scrolling_Box *self = (Fl_Scrolling_Box *)data;

	  do
	  {
	       usleep(self->m_useconds);

	       pthread_mutex_lock(&(self->m_mutex));
	       if (self->m_overflow && self->m_label)
	       {
		    //Fl::lock();
		    self->label(self->m_label + self->m_offset);
		    self->redraw();
		    //Fl::unlock();
		    //Fl::awake();

		    if (self->m_direction)
		    {
			 self->m_offset++;

			 if (self->m_offset > self->m_maxOffset)
			 {
			      self->m_direction = !self->m_direction;
			      self->m_offset--;
			 }
		    }
		    else
		    {
			 self->m_offset--;

			 if (self->m_offset < 0)
			 {
			      self->m_direction = !self->m_direction;
			      self->m_offset++;
			 }
		    }
	       }
	       pthread_mutex_unlock(&(self->m_mutex));

	  } while(true);

	  pthread_exit(NULL);
     }

private:
     useconds_t        m_useconds;
     char             *m_label;
     bool              m_overflow;
     int32_t           m_maxOffset;
     int32_t           m_offset;
     bool              m_direction;
     pthread_t         m_thread;
     pthread_mutex_t   m_mutex;
};

// Monkeyboard's listener class ( see ThreadListener() )
class ListenerSession
{
public:

     ListenerSession(int sockfd) : socket(sockfd), running(true), hasStarted(false)
     {
     }

     int  socket;
     bool running;
     bool hasStarted;
};


// FM table item
class FMItem
{
     friend DABItem;

public:

     FMItem() : FMItem(0, 0, "-", "-", "-")
     {
     }

     FMItem(uint32_t index, uint16_t strength, string programName, string rdsText, string programType) :
	  Index(index),
	  Strength(strength),
	  ProgramName(programName),
	  RDSText(rdsText),
	  ProgramType(programType)
     {
     }

     void SetProgramName(const char *str)
     {
	  _chomp(ProgramName, str);
     }

     void SetRDSText(const char *str)
     {
	  _chomp(RDSText, str);
     }

     void SetProgramType(const char *str)
     {
	  _chomp(ProgramType, str);
     }

private:
     static void _chomp(string &value, const char *str)
     {
	  if (str)
	  {
	       value = std::regex_replace(str, std::regex("^ +| +$|( ) +"), "$1");
	       if (value == "")
	       {
		    value = "-";
	       }
	  }
	  else
	  {
	       value = "-";
	  }
     }
public:

     uint32_t  Index;
     uint16_t  Strength;
     string    ProgramName;
     string    RDSText;
     string    ProgramType;
};


// DAB table item (derived from FMItem)
class DABItem : public FMItem
{
public:

     DABItem() : DABItem(0, 0, "-", "-", "-", "-", "-")
     {
     }

     DABItem(uint32_t index, uint16_t strength, string programName, string rdsText, string programType, string programRate, string ensemble) :
	  FMItem(index, strength, programName, rdsText, programType),
	  ProgramRate(programRate),
	  Ensemble(ensemble)
     {
     }

     void SetProgramRate(const char *str)
     {
	  FMItem::_chomp(ProgramRate, str);
     }

     void SetEnsemble(const char *str)
     {
	  FMItem::_chomp(Ensemble, str);
     }

     string    ProgramRate;
     string    Ensemble;
};


// Main class
class MediaUI
{
public:

     MediaUI() :
	  isReady(false),
	  isRunning(false),
	  scanRequestedByMe(false),
	  settingsFile(string(getenv("HOME") + string("/.flhu/mediaui.ini"))),
	  overrideDaytime(0),
	  currentDaytime(DAY),
	  gpsAvailable(false),
	  plListMode(PLMODE_PLAYLISTS),
	  foregroundColor(FL_WHITE),

	  gpiodConnected(false),
	  gpiodCond(PTHREAD_COND_INITIALIZER),
	  gpiodCondMutex(PTHREAD_MUTEX_INITIALIZER),

	  monkeyConnected(false),
	  monkeyCond(PTHREAD_COND_INITIALIZER),
	  monkeyCondMutex(PTHREAD_MUTEX_INITIALIZER),
	  currentStreamIndex(-1),
	  timeChannelSelection(0),
	  noGPIOD(false),
	  mpdQueryCategory(QUERY_FILE),
	  isDABScanning(false),
	  isFMScanning(false),
	  mpd(NULL),
	  currentStrength(0)
	  {
	       memset(&gpsData, 0, sizeof(gps_data_t));
	       strengthPMGs.clear();
	       waitPNGs.clear();
	       DABVector.clear();
	       FMVector.clear();
	       gpiodCommands.clear();
	       monkeyCommands.clear();

	       //iniFile.SetUnicode();
	       iniFile.LoadFile(settingsFile.c_str());

	       // Reload configuration
	       // [ Server ]
	       monkeyServerName   = iniFile.GetValue("Monkey Server", "Name", DEFAULT_SERVER_NAME);
	       monkeyServerPort   = (uint32_t) iniFile.GetLongValue("Monkey Server", "Port", DEFAULT_SERVER_PORT);

	       // [ GPIOD ]
	       gpiodServerPort    = (uint32_t) iniFile.GetLongValue("GPIOD Server", "Port", GPIOD_DEFAULT_PORT);

	       // [ MPD Server ]
	       mpdServerName      = iniFile.GetValue("MPD Server", "Name", "localhost");
	       mpdServerPort      = (uint32_t) iniFile.GetLongValue("MPD Server", "Port", 6600);

	       // [ GPS Server ]
	       gpsServerName      = iniFile.GetValue("GPS Server", "Name", "localhost");
	       gpsServerPort      = iniFile.GetValue("GPS Server", "Port", DEFAULT_GPSD_PORT);

	       // [ GPS Position ]
	       gpsLatitude        = iniFile.GetDoubleValue("GPS Position", "Latitude", DEFAULT_POSITION_LATITUDE);
	       gpsLongitude       = iniFile.GetDoubleValue("GPS Position", "Longitude", DEFAULT_POSITION_LONGITUDE);
	       
	       // [ Bluetooth ]
	       bluetoothMAC       = iniFile.GetValue("Bluetooth", "Address", "00:00:00:00:00:00");

	       // [ Player ]
	       playerRandom       = iniFile.GetBoolValue("Player", "Random", true);
	       playerCrossfade    = (uint8_t) iniFile.GetLongValue("Player", "Crossfade", 0);
	  }

     void UpdateConfig()
     {
	  // [ MonkeyServer ]
	  iniFile.SetValue("Monkey Server", "Name", monkeyServerName.c_str());
	  iniFile.SetLongValue("Monkey Server", "Port", monkeyServerPort);
	  // [ GPIOD Server ]
	  iniFile.SetLongValue("GPIOD Server", "Port", gpiodServerPort);
	  // [ MPD Server ]
	  iniFile.SetValue("MPD Server", "Name", mpdServerName.c_str());
	  iniFile.SetLongValue("MPD Server", "Port", mpdServerPort);
	  // [ GPS Server ]
	  iniFile.SetValue("GPS Server", "Name", gpsServerName.c_str());
	  iniFile.SetValue("GPS Server", "Port", gpsServerPort.c_str());
	  // [ GPS Position ]
	  iniFile.SetDoubleValue("GPS Position", "Latitude", gpsLatitude);
	  iniFile.SetDoubleValue("GPS Position", "Longitude", gpsLongitude);
	  // [ Bluetooth ]
	  iniFile.SetValue("Bluetooth", "Address", bluetoothMAC.c_str());
	  // [ Player ]
	  iniFile.SetBoolValue("Player", "Random", playerRandom);
	  iniFile.SetLongValue("Player", "Crossfade", playerCrossfade);
     }

     void SaveConfig()
     {
	  if (isReady)
	  {
	       UpdateConfig();

	       if (iniFile.SaveFile(settingsFile.c_str()) != SI_OK)
	       {
		    fprintf(stderr, "ERROR: iniFile.Save(%s) failed: %s\n", settingsFile.c_str(), strerror(errno));
	       }
	  }
     }

     bool                               isReady;
     bool                               isRunning;
     bool                               scanRequestedByMe;
     string                             settingsFile;
     time_t                             overrideDaytime;
     DayTime_t                          currentDaytime;
     bool                               gpsAvailable;
     PlListMode_t                       plListMode;

     Fl_Color                           foregroundColor;

     string                             bluetoothMAC;

     uint32_t                           gpiodServerPort;
     pthread_t                          gpiodThread;
     pthread_t                          gpiodPingThread;
     bool                               gpiodConnected;

     pthread_cond_t                     gpiodCond;
     pthread_mutex_t                    gpiodCondMutex;
     vector<char *>                     gpiodCommands;

     string                             monkeyServerName;
     uint32_t                           monkeyServerPort;
     pthread_t                          monkeyThread;
     bool                               monkeyConnected;
     pthread_cond_t                     monkeyCond;
     vector<char *>                     monkeyCommands;
     pthread_mutex_t                    monkeyCondMutex;

     int32_t                            currentStreamIndex;

     time_t                             timeChannelSelection;
     vector<DABItem *>                  DABVector;
     vector<FMItem *>                   FMVector;

     string                             gpsServerName;
     string                             gpsServerPort;
     struct gps_data_t                  gpsData;
     double                             gpsLatitude;
     double                             gpsLongitude;

     // Strength Icon
     vector<Fl_PNG_Image *>             strengthPMGs;
     vector<Fl_PNG_Image *>             waitPNGs;
     vector<Fl_PNG_Image *>::iterator   itWait;

     bool                               noGPIOD;

     QueryCategory_t                    mpdQueryCategory;


     bool                               isDABScanning;
     bool                               isFMScanning;
     CSimpleIniA                        iniFile;
     struct mpd_connection             *mpd;
     bool                               playerRandom;
     uint8_t                            playerCrossfade;

     string                             mpdServerName;
     uint32_t                           mpdServerPort;

     uint16_t                           currentStrength;
};

// Keyboard enum (defines layout)
typedef enum
{
     // First row
     KEY_Q = 0,
     KEY_W,
     KEY_E,
     KEY_R,
     KEY_T,
     KEY_Y,
     KEY_U,
     KEY_I,
     KEY_O,
     KEY_P,
     KEY_7,
     KEY_8,
     KEY_9,
     // Second row
     KEY_A,
     KEY_S,
     KEY_D,
     KEY_F,
     KEY_G,
     KEY_H,
     KEY_J,
     KEY_K,
     KEY_L,
     KEY_DEL,
     KEY_4,
     KEY_5,
     KEY_6,
     // Third row
     KEY_SPACE,
     KEY_Z,
     KEY_X,
     KEY_C,
     KEY_V,
     KEY_B,
     KEY_N,
     KEY_M,
     KEY_SEEK,
     KEY_0,
     KEY_1,
     KEY_2,
     KEY_3,
     KEY_MAX_KEYS
} KeyboardLayout_t;


typedef struct
{
     KeyboardLayout_t  key;
     const char       *label;
} KeyboardKey_t;


static const KeyboardKey_t Keyboard_Keys[KEY_MAX_KEYS] =
{
     // First row
     { KEY_Q,     "Q"     },
     { KEY_W,     "W"     },
     { KEY_E,     "E"     },
     { KEY_R,     "R"     },
     { KEY_T,     "T"     },
     { KEY_Y,     "Y"     },
     { KEY_U,     "U"     },
     { KEY_I,     "I"     },
     { KEY_O,     "O"     },
     { KEY_P,     "P"     },
     { KEY_7,     "7"     },
     { KEY_8,     "8"     },
     { KEY_9,     "9"     },
     // Second row
     { KEY_A,     "A"     },
     { KEY_S,     "S"     },
     { KEY_D,     "D"     },
     { KEY_F,     "F"     },
     { KEY_G,     "G"     },
     { KEY_H,     "H"     },
     { KEY_J,     "J"     },
     { KEY_K,     "K"     },
     { KEY_L,     "L"     },
     { KEY_DEL,   "Del"   },
     { KEY_4,     "4"     },
     { KEY_5,     "5"     },
     { KEY_6,     "6"     },
     // Third row
     { KEY_SPACE, "Spc"   },
     { KEY_Z,     "Z"     },
     { KEY_X,     "X"     },
     { KEY_C,     "C"     },
     { KEY_V,     "V"     },
     { KEY_B,     "B"     },
     { KEY_N,     "N"     },
     { KEY_M,     "M"     },
     { KEY_SEEK,  "Seek"  },
     { KEY_0,     "0"     },
     { KEY_1,     "1"     },
     { KEY_2,     "2"     },
     { KEY_3,     "3"     }
};


// MPD Query categories
typedef struct
{
     const char        *label;
     uint32_t           width;
     QueryCategory_t    query;
} MSQButtonsLabel_t;

// May respect the MSButtonCategory_t enum
static const MSQButtonsLabel_t MSQueryButtonsLabels[QUERY_MAX + 1] =
{
     { "Artist",   74,  QUERY_ARTIST },
     { "Album",    80,  QUERY_ALBUM  },
     { "Title",    62,  QUERY_TITLE  },
     { "Any",      58,  QUERY_ANY    },
     { "Filename", 103, QUERY_FILE   },
     { "AllSongs", 0,   QUERY_MAX    }
};


// EQ Presets
typedef enum
{
     EQ_PRESET_OFF = 0,
     EQ_PRESET_BASS,
     EQ_PRESET_JAZZ,
     EQ_PRESET_LIVE,
     EQ_PRESET_VOCAL,
     EQ_PRESET_ACOUSTIC,
     EQ_MAX_PRESETS
} EQPresetType_t;

typedef struct
{
     EQPresetType_t  type;
     const char     *label;
     const char     *command;
} EQPreset_t;

// This array needs to respect the EQPreseet_t order
static const EQPreset_t EQPresets[EQ_MAX_PRESETS] =
{
     { EQ_PRESET_OFF,      "EQ OFF",   "E000000000000" },
     { EQ_PRESET_BASS,     "BASS",     "E200000000000" },
     { EQ_PRESET_JAZZ,     "JAZZ",     "E210000000000" },
     { EQ_PRESET_LIVE,     "LIVE",     "E220000000000" },
     { EQ_PRESET_VOCAL,    "VOCAL",    "E230000000000" },
     { EQ_PRESET_ACOUSTIC, "ACSTIC",   "E240000000000" }
};


// Bluetooth keys (defines layout)
typedef enum
{
     BLUETOOTH_KEY_1 = 0,
     BLUETOOTH_KEY_4,
     BLUETOOTH_KEY_7,
     BLUETOOTH_KEY_COLON,
     BLUETOOTH_KEY_A,
     BLUETOOTH_KEY_D,

     BLUETOOTH_KEY_2,
     BLUETOOTH_KEY_5,
     BLUETOOTH_KEY_8,
     BLUETOOTH_KEY_0,
     BLUETOOTH_KEY_B,
     BLUETOOTH_KEY_E,

     BLUETOOTH_KEY_3,
     BLUETOOTH_KEY_6,
     BLUETOOTH_KEY_9,
     BLUETOOTH_KEY_DEL,
     BLUETOOTH_KEY_C,
     BLUETOOTH_KEY_F,

     BLUETOOTH_KEY_MEDIA,
     BLUETOOTH_KEY_CALL,
     BLUETOOTH_KEY_STOP,
     BLUETOOTH_KEY_PAIR,
     BLUETOOTH_KEY_TRUST,
     BLUETOOTH_KEY_CONN,

     BLUETOOTH_KEY_MUTE,
     BLUETOOTH_KEY_VOL_MORE,
     BLUETOOTH_KEY_VOL_LESS,
     BLUETOOTH_KEY_PREV,
     BLUETOOTH_KEY_NEXT,
     BLUETOOTH_KEY_DISC,

     BLUETOOTH_MAX_KEYS
} BluetoothKeyLayout_t;

typedef struct
{
     BluetoothKeyLayout_t   key;
     const char            *label;
     uint8_t                col;
} BluetoothKey_t;

static BluetoothKey_t Bluetooth_Keys[BLUETOOTH_MAX_KEYS] =
{
     { BLUETOOTH_KEY_1,        "1",     0 },
     { BLUETOOTH_KEY_4,        "4",     0 },
     { BLUETOOTH_KEY_7,        "7",     0 },
     { BLUETOOTH_KEY_COLON,    ":",     0 },
     { BLUETOOTH_KEY_A,        "A",     0 },
     { BLUETOOTH_KEY_D,        "D",     0 },

     { BLUETOOTH_KEY_2,        "2",     1 },
     { BLUETOOTH_KEY_5,        "5",     1 },
     { BLUETOOTH_KEY_8,        "8",     1 },
     { BLUETOOTH_KEY_0,        "0",     1 },
     { BLUETOOTH_KEY_B,        "B",     1 },
     { BLUETOOTH_KEY_E,        "E",     1 },

     { BLUETOOTH_KEY_3,        "3",     2 },
     { BLUETOOTH_KEY_6,        "6",     2 },
     { BLUETOOTH_KEY_9,        "9",     2 },
     { BLUETOOTH_KEY_DEL,      "Del",   2 },
     { BLUETOOTH_KEY_C,        "C",     2 },
     { BLUETOOTH_KEY_F,        "F",     2 },

     { BLUETOOTH_KEY_MEDIA,    "Media", 3 },
     { BLUETOOTH_KEY_CALL,     "Call",  3 },
     { BLUETOOTH_KEY_STOP,     "Stop",  3 },
     { BLUETOOTH_KEY_PAIR,     "Pair",  3 },
     { BLUETOOTH_KEY_TRUST,    "Trust", 3 },
     { BLUETOOTH_KEY_CONN,     "Conn.", 3 },

     { BLUETOOTH_KEY_MUTE,     "Mute",  4 },
     { BLUETOOTH_KEY_VOL_MORE, "@2<-",  4 },
     { BLUETOOTH_KEY_VOL_LESS, "@2->",  4 },
     { BLUETOOTH_KEY_PREV,     "@<<",   4 },
     { BLUETOOTH_KEY_NEXT,     "@>>",   4 },
     { BLUETOOTH_KEY_DISC,     "Disc.", 4 }
};

typedef enum
{
     PLAYER_KEY_RANDOM = 0,
     PLAYER_KEY_PREV,
     PLAYER_KEY_PLAY,
     PLAYER_KEY_NEXT,
     PLAYER_KEY_MAX
} PlayerKey_t;

// Main class (global)
static MediaUI *mui = NULL;

// Main window
static Fl_Double_Window *win;

// DAB
static Fl_Browser *DABBrowser;

/* FM */
static Fl_Browser *FMBrowser;

// PHONE - Monkeyboard Music Type / BlueTooth
static Fl_Input *MACQueryText;
static Fl_Button *EQPresetKeys[EQ_MAX_PRESETS];
static Fl_Button *BluetoothKeys[BLUETOOTH_MAX_KEYS];

// Playlist
static Fl_Browser *PLBrowser;
static Fl_Button *PlayerKeys[PLAYER_KEY_MAX]; // Random / Prev / PLAY|PAUSE / Next
static const char *Player_Keys_Labels[PLAYER_KEY_MAX + 1] = { "@refresh", "@|<", "@>", "@>|", "@||" };
static Fl_Slider *PlayerSeek;
static Fl_Box *PlayerCrossfadeBox;
static Fl_Counter *PlayerCrossfade;

// Music Search Query
static Fl_Input *MSQueryText;
static Fl_Round_Button *MSQueryCheckboxes[QUERY_MAX];
static Fl_Browser *MSBrowser;
static Fl_Group *KeyBoard;
static Fl_Button *KeyboardKeys[KEY_MAX_KEYS];

// Status (top window)
static Fl_Box *playStatusInfo;
static Fl_Scrolling_Box *ensembleInfo;
static Fl_Box *rateInfo;
static Fl_Button *strengthIcon;
static Fl_Box *clockInfo;
static Fl_Box *speedInfo;
static Fl_Button *navigationIcon;
static Fl_Scrolling_Box *currentStreamInfo;


static Fl_Box *waitImage;
static Fl_Tabs *tabs;
static Fl_Group *dab;
static Fl_Group *fm;
static Fl_Group *Phone;


static Fl_Group *pl;
static Fl_Group *ms;
static Fl_Group *QueryType;


// Function proto
static void QueryMPDPlaylist(QueryCategory_t category);
static void SetDaytimeColors(DayTime_t dt);
static void PLBrowserUpdateTrackList(size_t index, bool startPlayback = true);
static void PLBrowser_CB(Fl_Widget *w, void *data);



// Took from https://gist.github.com/Fonger/98cc95ac39fbe1a7e4d9
#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t size)
{
     size_t srclen;
     size_t dstlen;

     dstlen = strlen(dst);
     size -= dstlen + 1;

     if (!size)
     {
	  return (dstlen);
     }

     srclen = strlen(src);

     if (srclen > size)
     {
	  srclen = size;
     }

     memcpy(dst + dstlen, src, srclen);
     dst[dstlen + srclen] = '\0';

     return (dstlen + srclen);
}
#endif /* !HAVE_STRLCAT */

//
// display a message to stdout.
//
static void __attribute__ ((format (printf, 1, 2))) sockErr(const char *fmt, ...)
{
     va_list args;

     va_start(args, fmt);
     vfprintf(stderr, fmt, args);
     va_end(args);
}

//
// Reset all scanning status members
//
static void ResetScanningState()
{
     mui->isDABScanning = false;
     mui->isFMScanning = false;
     mui->scanRequestedByMe = false;
}

//
// MPD buttons/slider/crossfade enability function, according to MPD connection status
//
static void UpdateActiveStateFromMPDStatus()
{
     if (mui->isReady && mui->isRunning)
     {
	  if (mui->mpd && (mui->plListMode == PLMODE_TRACKS))
	  {
	       if (PlayerKeys[PLAYER_KEY_PLAY]->active() == 0)
	       {
		    for (uint8_t i = PLAYER_KEY_RANDOM; i < PLAYER_KEY_MAX; i++)
		    {
			 PlayerKeys[i]->activate();
		    }
	       }

	       if (PlayerSeek->active() == 0)
	       {
		    PlayerSeek->activate();
	       }

	       if (PlayerCrossfade->active() == 0)
	       {
		    PlayerCrossfade->activate();
	       }
	  }
	  else
	  {
	       if (PlayerKeys[PLAYER_KEY_PLAY]->active() != 0)
	       {
		    for (uint8_t i = PLAYER_KEY_RANDOM; i < PLAYER_KEY_MAX; i++)
		    {
			 PlayerKeys[i]->deactivate();
		    }
	       }

	       if (PlayerSeek->active() != 0)
	       {
		    PlayerSeek->value(0);
		    PlayerSeek->deactivate();
	       }

	       if (PlayerCrossfade->active() != 0)
	       {
		    PlayerCrossfade->deactivate();
	       }
	  }
     }
}

//
// MPD related function
//
static bool MPDCheck(bool verbose = true)
{
     if (mui->isRunning == false)
     {
	  return false;
     }

     if ((mpd_connection_get_error(mui->mpd) != MPD_ERROR_SUCCESS) || (mpd_response_finish(mui->mpd) == false))
     {
	  if (verbose)
	  {
	       fprintf(stderr, "%s\n", mpd_connection_get_error_message(mui->mpd));
	  }
	  mpd_connection_free(mui->mpd);
	  mui->mpd = NULL;
	  UpdateActiveStateFromMPDStatus();
	  return false;
     }

     return true;
}

//
// Close the MPD connection
//
static void MPDEnd(bool verbose = true)
{
     if (mui->mpd)
     {
	  MPDCheck(verbose);
     }
}

//
// Start a new MPD connection
//
void MPDBegin()
{
     if (mui->mpd == NULL)
     {
	  mui->mpd = mpd_connection_new(mui->mpdServerName.c_str(), mui->mpdServerPort, 30000);

	  if (mpd_connection_get_error(mui->mpd) != MPD_ERROR_SUCCESS)
	  {
	       sockErr("   - Failed to connected to MPD server %s:%d: %s\n", mui->mpdServerName.c_str(), mui->mpdServerPort, mpd_connection_get_error_message(mui->mpd));
	       mpd_connection_free(mui->mpd);
	       mui->mpd = NULL;
	       UpdateActiveStateFromMPDStatus();
	       return;
	  }
	  sockErr("   - Connected to MPD server %s:%d\n", mui->mpdServerName.c_str(), mui->mpdServerPort);

	  mpd_connection_set_keepalive(mui->mpd, true);
     }
     UpdateActiveStateFromMPDStatus();
}


//
// play: true: play, false: pause
//
static void MPDPlay(bool play)
{
     struct mpd_status *status;

     MPDBegin();
     if (!mui->mpd || !mpd_send_status(mui->mpd))
     {
	  printf("mpd_send_status() failed\n");
	  MPDEnd();
	  return;
     };

     if ((status = mpd_recv_status(mui->mpd)) == NULL)
     {
	  printf("mpd_recv_status() failed\n");
	  MPDEnd();
	  return;
     }

     // Play or Pause
     if (!play && (mpd_status_get_state(status) == MPD_STATE_PLAY))
     {
	  if (!mpd_send_pause(mui->mpd, true))
	  {
	       printf("mpd_send_play() failed\n");
	       MPDEnd();
	       mpd_status_free(status);
	       return;
	  }
     }
     else if (play && (mpd_status_get_state(status) != MPD_STATE_PLAY))
     {
	  if (!mpd_send_play(mui->mpd))
	  {
	       printf("mpd_send_play() failed\n");
	       MPDEnd();
	       mpd_status_free(status);
	       return;
	  }
     }
     MPDEnd();

     mpd_status_free(status);
}

//
// Clamp <value> to min/max
//
static int32_t _clamp(int32_t value, int32_t minValue, int32_t maxValue)
{
     return (::max(minValue, ::min(value, maxValue)));
}

//
// map <x> to <outMin>/<outMax>
//
static int32_t _map(int32_t x, int32_t inMin, int32_t inMax, int32_t outMin, int32_t outMax)
{
     return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

//
// Change the strength icon according to <strength> value
//
static void UpdateStrength(uint16_t strength)
{
     if (mui->strengthPMGs.size())
     {
	  strengthIcon->image(mui->strengthPMGs[_map((_clamp(strength, 15, 99)), 15, 100, 0, 8)]);
     }
     mui->currentStrength = strength;
}

//
// Load icons images into memory
//
static bool LoadStrengthIconImages()
{
     char filename[PATH_MAX + NAME_MAX];
     size_t i = 0;

     do
     {
	  snprintf(filename, sizeof(filename), "%s%s%s%01zu.%s", getenv("HOME"), FLHUDIR.c_str(), "/applelogo/applelogo", i, "png");

	  Fl_PNG_Image *img = new Fl_PNG_Image(filename);

	  if (img->w() == 0)
	  {
	       delete img;
	       break;
	  }

	  mui->strengthPMGs.push_back(img);
	  i++;

     } while(true);

     return true;
}

//
// Show Next Image of Wait Icon (e.g. flying bat)
//
static void DisplayWaitAnim_Task(void *data)
{
     static int Xpos = -1;
     static int Voffset = 0;
     static bool Vdir = false;

     if (mui->isRunning == false)
     {
	  return;
     }
#if 0
     // Enable/Disable the DAB/FM browsers accrording to scanning status
     if ((mui->isDABScanning || mui->isFMScanning) && ((DABBrowser->active() == 1) || (FMBrowser->active() == 1)))
     {
	  DABBrowser->deactivate();
	  FMBrowser->deactivate();
     }
     else if (((mui->isDABScanning == false) && (mui->isFMScanning == false)) && ((DABBrowser->active() == 0) || (FMBrowser->active() == 0)))
     {
	  DABBrowser->activate();
	  FMBrowser->activate();
     }
#endif

     // Scanning is finished, Hide The Bat (tm)
     if ((mui->isDABScanning == false) && (mui->isFMScanning == false) && waitImage->visible())
     {
	  waitImage->hide();
     }

     if (mui->waitPNGs.size() && (mui->isDABScanning || mui->isFMScanning))
     {
	  // Set image before showing
	  if (waitImage->visible() == false)
	  {
	       mui->itWait = mui->waitPNGs.begin();

	       // Put the image out of the window
	       Xpos = 1 - (*mui->itWait)->w();
	       Voffset = 0;
	       Vdir = false;

	       waitImage->image((*mui->itWait));
	       waitImage->position(Xpos, (win->h() - (*mui->itWait)->h()) / 2);
	       waitImage->show();
	  }
	  else // Already visible
	  {
	       waitImage->image((*mui->itWait));
	       Xpos += 15; // Horizonal step;

	       if (Xpos > win->w())
	       {
		    Xpos = 1 - (*mui->itWait)->w();
		    Voffset = 0;
		    Vdir = false;
	       }

	       if (Vdir)
	       {
		    if (Voffset < 32)
		    {
			 Voffset += 4;
		    }
		    else
		    {
			 Vdir = !Vdir;
			 Voffset -= 4;
		    }
	       }
	       else
	       {
		    if (Voffset > -32)
		    {
			 Voffset -= 4;
		    }
		    else
		    {
			 Vdir = !Vdir;
			 Voffset += 4;
		    }
	       }

	       waitImage->position(Xpos, ((win->h() - (*mui->itWait)->h()) / 2) + Voffset);
	  }

	  // Prepare next image
	  mui->itWait++;

	  // Loop
	  if (mui->itWait == mui->waitPNGs.end())
	  {
	       mui->itWait = mui->waitPNGs.begin();
	  }

	  Fl::lock();
	  win->redraw();
	  Fl::unlock();
	  Fl::awake();
     }

     if (mui->isRunning)
     {
	  Fl::repeat_timeout(((mui->isDABScanning || mui->isFMScanning) ? WAIT_IMAGE_PAUSE : 1.0), DisplayWaitAnim_Task);
     }
}

//
// Load all wait images into memory
//
static bool LoadWaitImages()
{
     char filename[PATH_MAX + NAME_MAX];
     size_t i = 0;

     do
     {
	  snprintf(filename, sizeof(filename), "%s%s%s%03zu.%s", getenv("HOME"), FLHUDIR.c_str(), "/bat/new/bat-", i, "png");

	  Fl_PNG_Image *img = new Fl_PNG_Image(filename);

	  if (img->w() == 0)
	  {
	       delete img;
	       break;
	  }
	  mui->waitPNGs.push_back(img);
	  i++;

     } while (true);

     mui->itWait = mui->waitPNGs.begin();

     return true;
}

//
// Clear the information boxes, located in the top of the window
//
static void ClearInfos()
{
     //playStatusInfo->label(NULL);
     ensembleInfo->SetLabel(NULL);
     rateInfo->label(NULL);
     mui->currentStreamIndex = -1;
     currentStreamInfo->SetLabel(NULL);
}

//
// Update displayed informations when selecting a channel in the DAB browser, using the latest available data.
//
static void UpdateInfosAfterChannelSelection(DABItem *item)
{
     mui->currentStreamIndex = item->Index;
     UpdateStrength(item->Strength);
     ensembleInfo->SetLabel(item->Ensemble);
     rateInfo->copy_label(item->ProgramRate.c_str());

     currentStreamInfo->SetLabel(NULL);

     if (strcmp(item->RDSText.c_str(), "-") != 0)
     {
	  currentStreamInfo->SetLabel(item->RDSText);
     }
     else
     {
	  if (strcmp(item->ProgramName.c_str(), "-") != 0)
	  {
	       currentStreamInfo->SetLabel(item->ProgramName);
	  }
     }
}

//
// Update displayed informations when selecting a channel in the FM browser, using the latest available data.
//
static void UpdateInfosAfterChannelSelection(FMItem *item)
{
     mui->currentStreamIndex = item->Index;
     UpdateStrength(item->Strength);

     currentStreamInfo->SetLabel(NULL);
     if (strcmp(item->RDSText.c_str(), "-") != 0)
     {
	  currentStreamInfo->SetLabel(item->RDSText);
     }
     else
     {
	  if (strcmp(item->ProgramName.c_str(), "-") != 0)
	  {
	       currentStreamInfo->SetLabel(item->ProgramName);
	  }
     }
}


//
// Clear and Init browsers
//
static void UpdateBrowserFirstLine(Fl_Browser *br, bool updateOnly = false)
{
     if (br)
     {
	  char buf[128] = {0};

	  if (br == DABBrowser)
	  {
	       snprintf(buf, sizeof(buf) - 1, "%s%s", (mui->currentDaytime == DAY ? "@C31" : "@C30"), "@l@c@.UPDATE DAB STATIONS LIST");

	       if (updateOnly)
	       {
		    br->text(1, buf);
	       }
	       else
	       {
		    br->add(buf);
	       }
	  }
	  else if (br == FMBrowser)
	  {
	       snprintf(buf, sizeof(buf) - 1, "%s%s", (mui->currentDaytime == DAY ? "@C31" : "@C30"), "@l@c@.UPDATE FM STATIONS LIST");

	       if (updateOnly)
	       {
		    br->text(1, buf);
	       }
	       else
	       {
		    br->add(buf);
	       }
	  }
	  else if(br == PLBrowser)
	  {
	       snprintf(buf, sizeof(buf) - 1, "%s%s", (mui->currentDaytime == DAY ? "@C31" : "@C30"), "@l@c@.UPDATE PLAYLISTS LIST");

	       if (updateOnly)
	       {
		    br->text(1, buf);
	       }
	       else
	       {
		    br->add(buf);
	       }
	  }
     }
}

//
// Returns index of selected item in the browser, -1 if no selection
//
static int32_t GetIndexFromBrowserSelection(Fl_Browser *br)
{
     uint32_t sel;

     if ((sel = br->value()) > 1)
     {
	  sel -= 2;

	  if (br == DABBrowser)
	  {
	       if (sel < mui->DABVector.size())
	       {
		    return mui->DABVector[sel]->Index;
	       }
	  }
	  else if (br == FMBrowser)
	  {
	       if (sel < mui->FMVector.size())
	       {
		    return mui->FMVector[sel]->Index;
	       }
	  }
     }

     return -1;
}

//
// Force selection of the entry that matches the item index
//
static void SelectBrowserItemIndex(Fl_Browser *br, uint32_t idx)
{
     if (br == DABBrowser)
     {
	  std::vector<DABItem *>::iterator it = mui->DABVector.begin();

	  while (it != mui->DABVector.end())
	  {
	       if ((*it)->Index == idx)
	       {
		    int32_t pos = std::distance(mui->DABVector.begin(), it) + 2;

		    ClearInfos();
		    UpdateInfosAfterChannelSelection((*it));
		    br->select(pos, 1);
		    break;
	       }
	       it++;
	  }
     }
     else if (br == FMBrowser)
     {
	  std::vector<FMItem *>::iterator it = mui->FMVector.begin();

	  while (it != mui->FMVector.end())
	  {
	       if ((*it)->Index == idx)
	       {
		    int32_t pos = std::distance(mui->FMVector.begin(), it) + 2;

		    ClearInfos();
		    UpdateInfosAfterChannelSelection((*it));
		    br->select(pos, 1);
		    // UpdateStrength((*it)->Strength);
		    break;
	       }
	       it++;
	  }
     }
}

//
// Note: it's called fl::locked
//
static void UpdateBrowserLayout(Fl_Browser *br)
{
     if (br == DABBrowser)
     {
	  //int old = DABTabWidths[1];
	  DABBrowser->redraw();
	  Fl::flush();

	  // Resize the ProgramName column
	  if (DABBrowser->scrollbar.maximum() > 1.0)
	  {
	       DABTabWidths[1] = (DABBROWSER_PROGRAMNAME_WIDTH - TW);
	  }
	  else
	  {
	       DABTabWidths[1] = DABBROWSER_PROGRAMNAME_WIDTH;
	  }

	  // if (old != DABTabWidths[1])
	  // {
	  //      DABBrowser->redraw();
	  //      Fl::flush();
	  // }
     }
     else if (br == FMBrowser)
     {
	  //int old = DABTabWidths[1];

	  FMBrowser->redraw();
	  Fl::flush();

	  // Resize the ProgramName column
	  if (FMBrowser->scrollbar.maximum() > 1.0)
	  {
	       FMTabWidths[1] = (FMBROWSER_PROGRAMNAME_WIDTH - TW);
	  }
	  else
	  {
	       FMTabWidths[1] = FMBROWSER_PROGRAMNAME_WIDTH;
	  }

	  // if (old != DABTabWidths[1])
	  // {
	  //      FMBrowser->redraw();
	  //      Fl::flush();
	  // }
     }
     else if (br == PLBrowser)
     {
	  PLBrowser->redraw();
	  Fl::flush();

	  // Soon
     }
     else if (br == MSBrowser)
     {
	  MSBrowser->redraw();
	  Fl::flush();

	  // Soon
     }

}

//
// Clear browser content and update the first entry
//
static void ClearBrowser(Fl_Browser *br)
{
     if (br)
     {
	  br->clear();
	  UpdateBrowserFirstLine(br);
     }
}

//
// Clear vector content
//
static void ClearCharVector(vector<char *> &v)
{
     vector<char *>::iterator it = v.begin();

     while (it != v.end())
     {
	  free((*it)); // Allocated by strdup()
	  it++;
     }
     v.clear();
}

//
// Clear the DAB vector
//
static void ClearDABVector(void)
{
     vector<DABItem *>::iterator itdab = mui->DABVector.begin();

     while (itdab != mui->DABVector.end())
     {
	  delete (*itdab);
	  itdab++;
     }
     mui->DABVector.clear();
}

//
// Clear the FM vector
//
static void ClearFMVector(void)
{
     vector<FMItem *>::iterator itfm = mui->FMVector.begin();

     while (itfm != mui->FMVector.end())
     {
	  delete (*itfm);
	  itfm++;
     }
     mui->FMVector.clear();
}

//
// Event Handler
//
static int EventHandler(int event)
{
#if 0
     if (Fl::event() == Fl_Event::FL_SCREEN_CONFIGURATION_CHANGED)
     {
	  printf("******************************* SCREEN **********************************\n");
     }
     else if (Fl::event() == Fl_Event::FL_FULLSCREEN)
     {
	  printf("******************************* FULLSCREEN **********************************\n");
     }
     else
     {
	  uint8_t d = win->damage();

	  printf("******************************* %d %d %d **********************************\n", Fl::event(), d, ((Fl_Widget *)dab)->visible());
#if 0
	  enum Fl_Damage {
	       FL_DAMAGE_CHILD    = 0x01, /**< A child needs to be redrawn. */
	       FL_DAMAGE_EXPOSE   = 0x02, /**< The window was exposed. */
	       FL_DAMAGE_SCROLL   = 0x04, /**< The Fl_Scroll widget was scrolled. */
	       FL_DAMAGE_OVERLAY  = 0x08, /**< The overlay planes need to be redrawn. */
	       FL_DAMAGE_USER1    = 0x10, /**< First user-defined damage bit. */
	       FL_DAMAGE_USER2    = 0x20, /**< Second user-defined damage bit. */
	       FL_DAMAGE_ALL      = 0x80  /**< Everything needs to be redrawn. */
	  };
#endif
	  if (d & Fl_Damage::FL_DAMAGE_CHILD)
	       printf("FL_DAMAGE_CHILD | ");
	  if (d & Fl_Damage::FL_DAMAGE_EXPOSE)
	       printf("FL_DAMAGE_EXPOSE | ");
	  if (d & Fl_Damage::FL_DAMAGE_SCROLL)
	       printf("FL_DAMAGE_SCROLL | ");
	  if (d & Fl_Damage::FL_DAMAGE_OVERLAY)
	       printf("FL_DAMAGE_OVERLAY | ");
	  if (d & Fl_Damage::FL_DAMAGE_USER1)
	       printf("FL_DAMAGE_USER1| ");
	  if (d & Fl_Damage::FL_DAMAGE_USER2)
	       printf("FL_DAMAGE_USER2 | ");
	  if (d & Fl_Damage::FL_DAMAGE_ALL)
	       printf("FL_DAMAGE_ALL");
	  printf("\n");


	  //printf("******************************* IS DAB: %d **********************************\n", (tabs->value() == dab));
     }
#endif

#if 0
     //printf("******************************* EVENT %d  **********************************\n", Fl::event());
     if ((Fl::event() == 0) && (win->damage() & Fl_Damage::FL_DAMAGE_ALL))
     {
	  if (tabs->value() == dab)
	  {
	       //printf("Update DABBrowser\n");
	       UpdateBrowserLayout(DABBrowser);
	  }
	  else if (tabs->value() == fm)
	  {
	       //printf("Update FMBrowser\n");
	       UpdateBrowserLayout(FMBrowser);
	  }
	  else if (tabs->value() == pl)
	  {
	       //printf("Update PLBrowser\n");
	       UpdateBrowserLayout(PLBrowser);
	  }
	  else if (tabs->value() == ms)
	  {
	       //printf("Update MSBrowser\n");
	       UpdateBrowserLayout(MSBrowser);
	  }
     }
#endif

     // Ignore Escape key
     if ((Fl::event() == FL_SHORTCUT) && (Fl::event_key() == FL_Escape))
     {
	  return 1;
     }

     return 0;  // we had no interest in the event
}

//
// Catch all events before Fltk process them, to get mouse events on top few widgets (unclickable)
//
static int EventDispatcher(int e, Fl_Window *w)
{
     // Catch mouse button 1 release event over the clock or speed widgets
     if ((e & FL_RELEASE) && (Fl::event() == Fl_Event::FL_NO_EVENT) && (Fl::event_button() == FL_LEFT_MOUSE))
     {
	  if (Fl::belowmouse() == clockInfo)
	  {
	       mui->overrideDaytime = (time(NULL) + (60 * 30)); // override for the next 30 minutes;
	       SetDaytimeColors(mui->currentDaytime == DAY ?  NIGHT : DAY);
	  }
	  else if (Fl::belowmouse() == speedInfo)
	  {
	       mui->overrideDaytime = 0;
	  }
     }

     return Fl::handle_(e, w);
}

//
// Send a command, without argument, to GPIOD
//
static void SendToGPIOD(GPIOD_Command_t command)
{
     if (mui->isRunning && mui->gpiodConnected)
     {
	  char  buf[256] = {0};

	  snprintf(buf, 255, "%d\n", uint32_t(command));

	  // lock
	  pthread_mutex_lock(&mui->gpiodCondMutex);
	  // push back the command
	  mui->gpiodCommands.push_back(strdup(buf));
	  // semaphore trigger
	  pthread_cond_signal(&mui->gpiodCond);
	  // unlock
	  pthread_mutex_unlock(&mui->gpiodCondMutex);
     }
}

//
// Send a command to GPIOD
//
static void SendToGPIOD(GPIOD_Command_t command, const char *msg)
{
     if (mui->isRunning && mui->gpiodConnected && msg)
     {
	  char  buf[256] = {0};

	  snprintf(buf, 255, "%d:%s\n", uint32_t(command), msg);

	  // lock
	  pthread_mutex_lock(&mui->gpiodCondMutex);
	  // push back the command
	  mui->gpiodCommands.push_back(strdup(buf));
	  // semaphore trigger
	  pthread_cond_signal(&mui->gpiodCond);
	  // unlock
	  pthread_mutex_unlock(&mui->gpiodCondMutex);
     }
}

//
// Execute a shell command
//
static int Shell(const char *cmd)
{
     return system(cmd);
}

//
// Execute a shell command (std::string variant)
//
static int Shell(const string &cmd)
{
     return Shell(cmd.c_str());
}

//
// Set the DAY/NIGHT color theme
//
static void SetDaytimeColors(DayTime_t dt)
{
     mui->currentDaytime = dt;

     switch (dt)
     {
     	case DAY:
	     // White
	     //mui->foregroundColor = FL_WHITE;
	     //Fl::background(0xFF,0xFF,0xFF); // R,G,B Box border color
	     //Fl::background2(0xFF,0xFF,0xFF); // R,G,B Text input border color

	     // Blueish
	     mui->foregroundColor = fl_rgb_color(47, 249, 255);
	     Fl::background(47, 249, 255); // R,G,B Box border color
	     Fl::background2(47, 249, 255); // R,G,B Text input border color

	     win->selection_color(SELECTION_COLOR);
	     Fl::foreground(0x00,0x00,0x00); // R,G,Bforeground color

	     // Sliders background
	     DABBrowser->scrollbar.color(fl_rgb_color(49, 49, 49));
	     FMBrowser->scrollbar.color(fl_rgb_color(49, 49, 49));
	     PLBrowser->scrollbar.color(fl_rgb_color(49, 49, 49));
	     MSBrowser->scrollbar.color(fl_rgb_color(49, 49, 49));
	     PlayerSeek->color(fl_rgb_color(49, 49, 49));

	     //Fl::background(0x6B, 0x8E, 0x23); // R,G,B Box border color
	     //Fl::background2(0x6B, 0x8E, 0x23); // R,G,B Text input border color
	     //Fl::foreground(0x00,0x00,0x00); // R,G,Bforeground color
	     break;

     	case NIGHT:
	     //mui->foregroundColor = FL_RED;
	     mui->foregroundColor = Fl_Color(0x9F000000);

	     win->selection_color(SELECTION_COLOR);
	     Fl::background(0x9F,0x00,0x00); // Box border color
	     Fl::background2(0x9F,0x00,0x00); // Text input border color
	     Fl::foreground(0x00,0x00,0x00); // foreground color
	     break;
     }

     playStatusInfo->labelcolor(mui->foregroundColor);
     ensembleInfo->labelcolor(mui->foregroundColor);
     rateInfo->labelcolor(mui->foregroundColor);
     clockInfo->labelcolor(mui->foregroundColor);

     if (mui->gpsAvailable)
     {
	  speedInfo->labelcolor(mui->foregroundColor);
     }
     else
     {
	  speedInfo->labelcolor(Fl_Color(((mui->currentDaytime == DAY) ? 31 : 30)));
     }

     currentStreamInfo->labelcolor(mui->foregroundColor);
     tabs->labelcolor(mui->foregroundColor);
     dab->labelcolor(mui->foregroundColor);

     DABBrowser->labelcolor(mui->foregroundColor);
     DABBrowser->textcolor(mui->foregroundColor);
     DABBrowser->selection_color(mui->foregroundColor);
     UpdateBrowserFirstLine(DABBrowser, true);

     fm->labelcolor(mui->foregroundColor);

     FMBrowser->labelcolor(mui->foregroundColor);
     FMBrowser->textcolor(mui->foregroundColor);
     FMBrowser->selection_color(mui->foregroundColor);
     //FMBrowser->scrollbar.color(fl_rgb_color(0, 0, 0));
     //FMBrowser->scrollbar.color(fl_rgb_color(49, 49, 49));
     //FMBrowser->scrollbar.color(mui->foregroundColor);

     UpdateBrowserFirstLine(FMBrowser, true);

     Phone->labelcolor(mui->foregroundColor);

     for (uint8_t i = EQ_PRESET_OFF; i < EQ_MAX_PRESETS; i++)
     {
	  EQPresetKeys[i]->labelcolor(mui->foregroundColor);
     }

     for (uint8_t i = BLUETOOTH_KEY_1; i < BLUETOOTH_MAX_KEYS; i++)
     {
	  if (i == BLUETOOTH_KEY_COLON || i == BLUETOOTH_KEY_DEL ||
	      i == BLUETOOTH_KEY_A || i == BLUETOOTH_KEY_B || i == BLUETOOTH_KEY_C ||
	      i == BLUETOOTH_KEY_D || i == BLUETOOTH_KEY_E || i == BLUETOOTH_KEY_F)
	  {
	       BluetoothKeys[i]->labelcolor(FL_BLACK);
	  }
	  else
	  {
	       BluetoothKeys[i]->labelcolor(mui->foregroundColor);
	  }
     }

     MACQueryText->labelcolor(mui->foregroundColor);
     MACQueryText->labelfont(MAC_FONT);
     MACQueryText->textfont(MAC_FONT);

     pl->labelcolor(mui->foregroundColor);
     PLBrowser->labelcolor(mui->foregroundColor);
     PLBrowser->textcolor(mui->foregroundColor);
     PLBrowser->selection_color(mui->foregroundColor);
     UpdateBrowserFirstLine(PLBrowser, true);

     PlayerCrossfadeBox->labelcolor(mui->foregroundColor);

     PlayerSeek->labelcolor(mui->foregroundColor);
     ms->labelcolor(mui->foregroundColor);
     MSQueryText->labelcolor(mui->foregroundColor);
     MSQueryText->labelfont(MAC_FONT);
     MSQueryText->textfont(MAC_FONT);
     QueryType->labelcolor(mui->foregroundColor);

     for (uint8_t i = QUERY_ARTIST; i < QUERY_MAX; i++)
     {
	  MSQueryCheckboxes[i]->labelcolor(mui->foregroundColor);
     }

     MSBrowser->labelcolor(mui->foregroundColor);
     MSBrowser->textcolor(mui->foregroundColor);
     MSBrowser->selection_color(mui->foregroundColor);

     KeyBoard->labelcolor(mui->foregroundColor);

     for (uint8_t i = KEY_Q; i <= KEY_3; i++)
     {
	  if (i == KEY_SPACE || i == KEY_SEEK || i == KEY_DEL)
	  {
	       KeyboardKeys[i]->labelcolor(FL_BLACK);
	  }
	  else
	  {
	       KeyboardKeys[i]->labelcolor(mui->foregroundColor);
	  }
     }

     //Fl::lock();
     win->redraw();
     Fl::flush();
     //Fl::unlock();
     //Fl::awake();
}

//
// Build a formatted string from DAB item data, used to fill the browser
//
static const char *BuildBrowserLineFromItem(DABItem *pDAB)
{
     static char buf[256];

     // Leave the extra \t as it will cut the text to column width
     snprintf(buf, (sizeof(buf) - 1), "@r%d \t%s\t@c%s\t",
	      pDAB->Strength, pDAB->ProgramName.c_str(), pDAB->ProgramType.c_str());

     return &buf[0];
}

//
// Build a formatted string from FM item data, used to fill the browser
//
static const char *BuildBrowserLineFromItem(FMItem *pFM)
{
     static char buf[256];

     // Leave the extra \t as it will cut the text to column width
     snprintf(buf, (sizeof(buf) - 1), "@r%3d \t@c%.6g MHz\t%s\t@c%s\t",
	      pFM->Strength, float(float(pFM->Index) / 1000.0), pFM->ProgramName.c_str(), pFM->ProgramType.c_str());

     return &buf[0];
}

//
// Parse and handle monkey's data
//
static void ParseServerReply(char *serverReply)
{
     char        *buffer = NULL;
     char        *token;
     bool         updateItem = false;
     char         delim[] = "|";
     int          maxfield = 50;
     int          numberOfFields = 0;
     uint32_t     ItemIndex = 0;
     ReplyType_t  replyType;
     DABItem     *pDAB = NULL;
     FMItem      *pFM = NULL;
     uint8_t      fpi = 0; // field parsing index
     char         field[maxfield][MAXBUF];

     //printf("*** %s: '%s'\n", __PRETTY_FUNCTION__, serverReply);
     
     memset(field, 0, sizeof(field));
     buffer = strdup(serverReply); // Don't use the source buffer, as strtok() modifies it.

     token = strtok(buffer, delim);
     while (token != NULL)
     {
	  if (strcmp(token, "DONE") != 0)
	  {
	       strcpy(field[numberOfFields], token);
	       numberOfFields++;
	  }

	  token = strtok(NULL, delim);
     }

     if (strcmp(field[0], "DABIndex") == 0)
     {
	  ResetScanningState();

	  fpi++; // First field
	  replyType = DAB;
	  updateItem = true;

	  //check if the DABIndex already exists in the DABVector:
	  ItemIndex = atol(field[1]);

	  if (mui->DABVector.size() > 0)
	  {
	       vector<DABItem *>::iterator it = mui->DABVector.begin();

	       while (it != mui->DABVector.end()) // Check if the item exists in the DABVector
	       {
		    if ((*it)->Index == ItemIndex) // the DAB index already exist - let's update its content.
		    {
			 pDAB = (*it);
			 fpi++; // Second field
			 break; // No need to go further
		    }

		    it++;
	       }
	  }

	  if (pDAB == NULL) // Create a new entry in DABVector
	  {
	       DABItem *dabItem = new DABItem; // use DABItem constructor to fill the DABVector

	       dabItem->Index = ItemIndex;
	       fpi++; // Second field (Index)

	       mui->DABVector.push_back(dabItem);

	       DABBrowser->add(NULL); /* Create new entry in pDABTable */
	       UpdateBrowserLayout(DABBrowser);

	       pDAB = dabItem;
	  }

     }
     else if (strcmp(field[0], "FMIndex") == 0)
     {
	  ResetScanningState();

	  fpi++; // First field
	  replyType = FM;
	  updateItem = true;

	  //check if the FMIndex already exists in the FMVector:
	  ItemIndex = ::stoi(field[1]);

	  // Drop it
	  if ((ItemIndex < FM_FREQ_MIN) || (ItemIndex > FM_FREQ_MAX))
	  {
	       return;
	  }

	  rateInfo->copy_label("");
	  ensembleInfo->SetLabel(NULL);

	  if (mui->FMVector.size() > 0)
	  {
	       vector<FMItem *>::iterator it = mui->FMVector.begin();

	       while (it != mui->FMVector.end()) // Check if the item exists in the FMVector
	       {
		    if ((*it)->Index == ItemIndex) // the FM index already exist - let's update its content.
		    {
			 pFM = (*it);
			 fpi++; // Second field
			 break; // No need to go further
		    }

		    it++;
	       }
	  }

	  if (pFM == NULL) // Create a new entry in DABVector
	  {
	       FMItem *fmItem = new FMItem; // use FMItem constructor to fill the FMVector

	       fmItem->Index = ItemIndex;
	       fpi++; // Second field (Index)

	       mui->FMVector.push_back(fmItem);

	       FMBrowser->add(NULL); /* Create new entry in pDABTable */
	       UpdateBrowserLayout(FMBrowser);

	       pFM = fmItem;
	  }

     }
     else
     {
	  replyType = OTHER;
     }

     // Now parse the remaining fields (start from the third)
     while (fpi < numberOfFields)
     {
	  // Handle the destination type (DAB/FM/OTHER)
	  switch (replyType)
	  {
	  	case OTHER:
		     // Reentrant
		     if (strcmp(field[fpi], "DABIndex") == 0)
		     {
			  char *p = strstr(serverReply + 10, "DABIndex");

			  if (p)
			  {
			       ParseServerReply(p);
			  }

			  // Don't go further as the rest of the fields
			  // dont belong to this entry
			  fpi = numberOfFields;
		     }
		     else if (strcmp(field[fpi], "FMIndex") == 0)
		     {
			  char *p = strstr(serverReply + 8, "FMIndex");

			  if (p)
			  {
			       ParseServerReply(p);
			  }

			  // Don't go further as the rest of the fields
			  // dont belong to this entry
			  fpi = numberOfFields;
		     }
		     //
		     //*** Volume ***
		     //0 - 16
		     else if (strcmp(field[fpi], "Volume") == 0)
		     {
			  //printf("Volume:%s\n", field[fpi + 1]);
			  fpi++;
		     }
		     //
		     //*** PlayStatus ***
		     //Text
		     else if (strcmp(field[fpi], "PlayStatus") == 0)
		     {
			  if ((strcmp(field[fpi + 1], "Scanning DAB...") == 0) && (mui->isDABScanning == false))
			  {
			       printf("** DAB Scanning\n");
			       mui->isDABScanning = true;

			       ClearInfos();
			       ClearBrowser(DABBrowser);

			       ClearDABVector();
			  }
			  else if ((strcmp(field[fpi + 1], "Scanning DAB Done") == 0) && (mui->isDABScanning == true))
			  {
			       printf("** DAB STOPS Scanning\n");
			       ResetScanningState();
			  }
			  else if ((strcmp(field[fpi + 1], "Scanning FM...") == 0) && (mui->isFMScanning == false))
			  {
			       printf("** FM Scanning\n");
			       mui->isFMScanning = true;

			       ClearInfos();
			       ClearBrowser(FMBrowser);

			       ClearFMVector();
			  }
			  else if ((strcmp(field[fpi + 1], "Scanning FM Done") == 0) && (mui->isFMScanning == true))
			  {
			       printf("** FM STOPS Scanning\n");
			       ResetScanningState();
			  }

			  //printf("PlayStatus:%s\n", field[fpi + 1]);
			  playStatusInfo->copy_label(field[fpi + 1]);

			  fpi++;
		     }
		     //
		     //*** Message ***
		     // Message is not used yet
		     // Message 	FMIndex
		     // 0: Error	1 - MAXFMSTATION
		     // 1: Low
		     //		CardSamplingRate
		     //		24, 32 (FM), 48
		     else if (strcmp(field[fpi], "Message") == 0 )
		     {
			  int32_t newIndex = 0;

			  //printf("*** Message | '%s' | '%s'\n", field[fpi + 1], field[fpi + 2]);

			  if ((mui->isDABScanning == false) && (sscanf(field[fpi + 2], "DABIndex:%d", &newIndex) == 1))
			  {
			       int32_t selectedIndex = GetIndexFromBrowserSelection(DABBrowser);

			       if (((selectedIndex == -1) || (selectedIndex != newIndex)) && (mui->DABVector.size() > 0))
			       {
				    if ((time(NULL) - mui->timeChannelSelection) > 3)
				    {
					 SelectBrowserItemIndex(DABBrowser, newIndex);
				    }
			       }
			  }
			  else if ((mui->isDABScanning == false) && (sscanf(field[fpi + 2], "FMIndex:%d", &newIndex) == 1))
			  {
			       int32_t selectedIndex = GetIndexFromBrowserSelection(FMBrowser);

			       if (((selectedIndex == -1) || (selectedIndex != newIndex)) && (mui->FMVector.size() > 0))
			       {
				    if ((time(NULL) - mui->timeChannelSelection) > 3)
				    {
 					 SelectBrowserItemIndex(FMBrowser, newIndex);
				    }
			       }
			  }

			  fpi += 2;
		     }
		     //
		     //*** HighSignalFreq ***
		     else if (strcmp(field[fpi], "HighSignalFreq") == 0)
		     {
			  //printf("HighSignalFreq:%s\n", field[fpi + 1]);
			  fpi++;
		     }
		     //
		     //*** DAB ***
		     // Switch to DAB Mode
		     else if (strcmp(field[fpi], "DAB") == 0)
		     {
			  //printf("Current MODE set to DAB MODE: %s\n", field[fpi]);
		     }
		     //
		     //*** FM ***
		     // Switch to FM Mode
		     else if (strcmp(field[fpi], "FM") == 0)
		     {
			  //printf("Current MODE set to FM MODE: %s\n", field[fpi]);
		     }
		     break;

	  	case DAB:
	  	case FM:
		     // DABIndex |x| Strength |x| ProgramName |x| RDSText |x| ProgramType |x| ProgramRate (only DAB) |x| EnsembleName (only DAB) |x|
		     // FMIndex  |x| Strength |x| ProgramName |x| RDSText |x| ProgramType |x|

		     // Reentrant
		     if (strcmp(field[fpi], "DABIndex") == 0)
		     {
			  char *p = strstr(serverReply + 10, "DABIndex");

			  if (p)
			  {
			       ParseServerReply(p);
			  }

			  // Don't go further as the rest of the fields
			  // dont belong to this entry
			  fpi = numberOfFields;
		     }
		     else if (strcmp(field[fpi], "FMIndex") == 0)
		     {
			  char *p = strstr(serverReply + 8, "FMIndex");

			  if (p)
			  {
			       ParseServerReply(p);
			  }

			  // Don't go further as the rest of the fields
			  // dont belong to this entry
			  fpi = numberOfFields;
		     }
		     //
		     //*** Strength ***
		     else if (strcmp(field[fpi], "Strength") == 0)
		     {
			  uint16_t strength = atoi(field[fpi + 1]);

			  ResetScanningState();

			  updateItem = true;
			  //printf(" > Strength: %s\n", field[fpi + 1]);

			  if (replyType == DAB)
			  {
			       pDAB->Strength = strength;
			  }
			  else //FM
			  {
			       pFM->Strength = strength;
			  }

			  if (mui->currentStrength != strength)
			  {
			       UpdateStrength(strength);
			  }

			  fpi++;
		     }
		     //
		     //*** ProgramName
		     else if (strcmp(field[fpi], "ProgramName") == 0 )
		     {
			  //printf(" > ProgramName: %s\n", field[fpi + 1]);

			  ResetScanningState();

			  //
			  // If there is no RDS set, display the ProgramName in the
			  // RDS area too.
			  //

			  if (strlen(field[fpi + 1]) && (strcmp(field[fpi + 1], " ") != 0))
			  {
			       updateItem = true;

			       if (replyType == DAB)
			       {
				    pDAB->SetProgramName(field[fpi + 1]);

				    if (((mui->currentStreamIndex != -1) && (ItemIndex == uint32_t(mui->currentStreamIndex))) &&
					(strcmp(pDAB->RDSText.c_str(), "-") == 0))
				    {
					 currentStreamInfo->SetLabel(field[fpi + 1]);
				    }

			       }
			       else //FM
			       {
				    pFM->SetProgramName(field[fpi + 1]);

				    if (((mui->currentStreamIndex != -1) && (ItemIndex == uint32_t(mui->currentStreamIndex))) &&
					(strcmp(pFM->RDSText.c_str(), "-") == 0))
				    {
					 currentStreamInfo->SetLabel(field[fpi + 1]);
				    }
			       }
			  }

			  fpi++;
		     }
		     //
		     //*** RDSText ***
		     else if (strcmp(field[fpi], "RDSText") == 0)
		     {
			  //printf(" > RDSText: '%s'\n", field[fpi + 1]);

			  ResetScanningState();

			  if (strlen(field[fpi + 1]) && (strcmp(field[fpi + 1], " ") != 0))
			  {
			       if (replyType == DAB)
			       {
				    if (strcmp(pDAB->RDSText.c_str(), field[fpi + 1]) != 0)
				    {
					 updateItem = true;
					 pDAB->SetRDSText(field[fpi + 1]);
				    }
			       }
			       else //FM
			       {
				    if (strcmp(pFM->RDSText.c_str(), field[fpi + 1]) != 0)
				    {
					 updateItem = true;
					 pFM->SetRDSText(field[fpi + 1]);
				    }
			       }

			       if (updateItem && ((mui->currentStreamIndex != -1) && (ItemIndex == uint32_t(mui->currentStreamIndex))))
			       {
				    currentStreamInfo->SetLabel(field[fpi + 1]);
			       }
			  }

			  fpi++;
		     }
		     //
		     //*** ProgramType ***
		     else if (strcmp(field[fpi], "ProgramType") == 0)
		     {
			  ResetScanningState();

			  //printf(" > ProgramType: %s\n", field[fpi + 1]);

			  if (strlen(field[fpi + 1]) && (strcmp(field[fpi + 1], " ") != 0))
			  {
			       updateItem = true;

			       if (replyType == DAB)
			       {
				    pDAB->SetProgramType(field[fpi + 1]);
			       }
			       else //FM
			       {
				    pFM->SetProgramType(field[fpi + 1]);
			       }
			  }

			  fpi++;
		     }
		     //
		     //*** ProgramRate (only for DAB) ***
		     else if (strcmp(field[fpi], "ProgramRate") == 0)
		     {
			  ResetScanningState();

			  updateItem = true;
			  //printf(" > ProgramRate: '%s'\n", field[fpi + 1]);

			  if (replyType == DAB)
			  {
			       pDAB->SetProgramRate(field[fpi + 1]);

			       if ((mui->currentStreamIndex != -1) && (ItemIndex == uint32_t(mui->currentStreamIndex)))
			       {
				    rateInfo->copy_label(field[fpi + 1]);
			       }
			  }

			  fpi++;
		     }
		     //
		     //*** Ensemble *** (only for DAB)
		     else if (strcmp(field[fpi], "EnsembleName") == 0)
		     {
			  ResetScanningState();

			  //printf(" > Ensemble: %s\n", field[fpi + 1]);

			  if (strlen(field[fpi + 1]) && (strcmp(field[fpi + 1], " ") != 0))
			  {
			       updateItem = true;

			       if (replyType == DAB)
			       {
				    pDAB->SetEnsemble(field[fpi + 1]);
				    if ((mui->currentStreamIndex != -1) && (ItemIndex == uint32_t(mui->currentStreamIndex)))
				    {
					 ensembleInfo->SetLabel(field[fpi + 1]);
				    }
			       }
			  }

			  fpi++;
		     }
		     break;
	  }

	  fpi++;
     }

     //
     // Update the browser with updated item
     //
     if (updateItem)
     {
	  switch(replyType)
	  {
	  	case DAB:
		{
		     std::vector<DABItem *>::iterator it = std::find(mui->DABVector.begin(), mui->DABVector.end(), pDAB);
		     int32_t pos = std::distance(mui->DABVector.begin(), it) + 2;

		     DABBrowser->text(pos, BuildBrowserLineFromItem(pDAB));
		}
		break;

	  	case FM:
		{
		     std::vector<FMItem *>::iterator it = std::find(mui->FMVector.begin(), mui->FMVector.end(), pFM);
		     int32_t pos = std::distance(mui->FMVector.begin(), it) + 2;

		     FMBrowser->text(pos, BuildBrowserLineFromItem(pFM));
		}
		break;

	  	case OTHER:
		     // noop
		     break;
	  }
     }

     if (buffer)
     {
	  free(buffer); // allocated by strdup()
     }
}

//
// Check if the socket is still opened
//
static int sockCheckOpened(int socket)
{
     fd_set   readfds, writefds, exceptfds;
     int      retval;
     struct   timeval timeout;

     for(;;)
     {
	  FD_ZERO(&readfds);
	  FD_ZERO(&writefds);
	  FD_ZERO(&exceptfds);
	  FD_SET(socket, &exceptfds);

	  timeout.tv_sec  = 0;
	  timeout.tv_usec = 0;

	  retval = select(socket + 1, &readfds, &writefds, &exceptfds, &timeout);

	  if((retval == -1) && ((errno != EAGAIN) && (errno != EINTR)))
	  {
	       return 0;
	  }

	  if (retval != -1)
	  {
	       return 1;
	  }
     }

     return 0;
}

//
// Read data from socket
//
static ssize_t sockRead(int socket, char *buf, size_t len)
{
     char    *pbuf;
     ssize_t  r, rr;
     void    *nl;

     if((socket < 0) || (buf == NULL) || (len < 1))
     {
	  return -1;
     }

     if(!sockCheckOpened(socket))
     {
	  return -1;
     }

     if (--len < 1)
     {
	  return(-1);
     }

     pbuf = buf;

     do
     {
	  if((r = recv(socket, pbuf, len, MSG_PEEK)) <= 0)
	  {
	       return -1;
	  }

	  if((nl = memchr(pbuf, '\n', r)) != NULL)
	  {
	       r = ((char *) nl) - pbuf + 1;
	  }

	  if((rr = read(socket, pbuf, r)) < 0)
	  {
	       return -1;
	  }

	  pbuf += rr;
	  len -= rr;

     } while((nl == NULL) && len);

     *pbuf = '\0';

     return (pbuf - buf);
}

//
// Write data to socket
//
static int sockWrite(int socket, const char *buf, size_t len)
{
     ssize_t  size;
     int      wlen = 0;

     if((socket < 0) || (buf == NULL))
     {
	  return -1;
     }

     if(!sockCheckOpened(socket))
     {
	  return -1;
     }

     while (len > 0)
     {
	  size = write(socket, buf, len);

	  if(size <= 0)
	  {
	       return -1;
	  }

	  len -= size;
	  wlen += size;
	  buf += size;
     }

     return wlen;
}

//
// Create a socket
//
static int sockCreate(uint32_t port, const char *transport, struct sockaddr_in *sin)
{
     struct protoent   *itransport;
     int                sock;
     int                type;
     int                proto = 0;

     memset(sin, 0, sizeof(*sin));

     sin->sin_family = AF_INET;
     sin->sin_port   = htons(port);

     itransport = getprotobyname(transport);

     if(!itransport)
     {
	  sockErr("   - Protocol not registered: %s\n", transport);
     }
     else
     {
	  proto = itransport->p_proto;
     }

     if(strcmp(transport, "udp") == 0)
     {
	  type = SOCK_DGRAM;
     }
     else
     {
	  type = SOCK_STREAM;
     }

     if ((sock = socket(AF_INET, type, proto)) < 0)
     {
	  sockErr("   - Cannot create socket: %s\n", strerror(errno));
	  return -1;
     }

     if (fcntl(sock, F_SETFD, FD_CLOEXEC) < 0)
     {
	  sockErr("   - Socket cannot be made uninheritable (%s)\n", strerror(errno));
	  return -1;
     }

     return sock;
}

//
// Create a socket connection to a remote
//
static int sockClient(const char *host, uint32_t port, const char *transport)
{
     union {
	  struct sockaddr_in in;
	  struct sockaddr sa;
     } fsin;
     struct hostent      *ihost;
     int                  sock;

     if ((sock = sockCreate(port, transport, &fsin.in)) < 0)
     {
	  return -1;
     }

     if ((fsin.in.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
     {
	  ihost = gethostbyname(host);

	  if(!ihost)
	  {
	       sockErr("   - Unknown host: %s\n", host);
	       return -1;
	  }

	  memcpy(&fsin.in.sin_addr, ihost->h_addr_list[0], ihost->h_length);
     }

     if(connect(sock, &fsin.sa, sizeof(fsin.in)) < 0)
     {
	  int err = errno;

	  close(sock);
	  errno = err;
	  sockErr("   - Unable to connect %s[%d]: %s\n", host, port, strerror(errno));
	  return -1;
     }

     return sock;
}

//
// Thread that receives data from Monkey server
//
static void *ThreadListener(void *data)
{
     ListenerSession *session = (ListenerSession *)data;
     char             buffer[2048];
     ssize_t          len;
     sigset_t         sigpipe_mask;

     pthread_yield();

     session->hasStarted = true;

     sigemptyset(&sigpipe_mask);
     sigaddset(&sigpipe_mask, SIGPIPE);
     if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, NULL) == -1)
     {
	  perror("pthread_sigmask");
     }

     do
     {
	  memset(buffer, 0, sizeof(buffer));
	  len = sockRead(session->socket, buffer, (sizeof(buffer) - 1));

	  if(len < 0)
	  {
	       session->running = false;
	  }
	  else
	  {
	       if(len > 0)
	       {
		    char *p = strrchr(buffer, '\n');
		    if (p)
		    {
			 *p = 0;
		    }

		    //printf("GOT: '%s'\n", buffer);

		    Fl::lock();
		    ParseServerReply(buffer);
		    Fl::unlock();
		    Fl::awake();
	       }
	  }

     } while (session->running);

     pthread_exit(NULL);
}

//
// Thread that sends data to Monkey server
//
static void *ThreadMONKEY(void *data)
{
     MediaUI           *m = (MediaUI *)data;
     ListenerSession   *session = NULL;
     int                socket = -1;
     pthread_t          threadSession;
     sigset_t           sigpipe_mask;

     pthread_yield();
	       
     sigemptyset(&sigpipe_mask);
     sigaddset(&sigpipe_mask, SIGPIPE);
     if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, NULL) == -1)
     {
	  perror("pthread_sigmask");
     }

     sockErr("   - Trying to connect Monkey server %s:%d\n", mui->monkeyServerName.c_str(), mui->monkeyServerPort);
     while ((socket = sockClient(mui->monkeyServerName.c_str(), mui->monkeyServerPort, "tcp")) < 0)
     {
	  if (!mui->isRunning)
	  {
	       break;
	  }
	  
	  sockErr("   - sockClient() (Monkey Server) failed, waiting 5s\n");
	  
	  for (uint8_t i = 0; i < 50; i++)
	  {
	       usleep(100000);
	       pthread_yield();

	       if (!mui->isRunning)
	       {
		    break;
	       }
	  }
     }

     // Create the listener thread
     if (m->isRunning)
     {
	  mui->monkeyConnected = true;

	  sockErr("   - Connected to Monkey server %s:%d\n", mui->monkeyServerName.c_str(), mui->monkeyServerPort);

	  session = new ListenerSession(socket);

	  if (pthread_create(&threadSession, NULL, ThreadListener, session) != 0)
	  {
	       mui->monkeyConnected = false;
	       perror("pthread_create");
	       m->isRunning = false;
	       pthread_exit(NULL);
	  }
	  else
	  {
	       while(!session->hasStarted)
	       {
		    pthread_yield();
	       }

	       char Xcmd[3] = {0};
	       sprintf(Xcmd, "%c%c", 'X', '\n');
	       mui->monkeyCommands.push_back(strdup(Xcmd));
	  }
     }

     if (m->isRunning)
     {
	  do
	  {
	       //char *cmd = NULL;

	       pthread_mutex_lock(&(m->monkeyCondMutex));
	       pthread_cond_wait((&m->monkeyCond), &(m->monkeyCondMutex));

	       // We're leaving
	       if (!m->isRunning)
	       {
		    mui->monkeyConnected = false;
		    // Cancel listener thread
		    session->running = false;
		    pthread_cancel(threadSession);
		    pthread_join(threadSession, NULL);
		    delete session;

		    ClearCharVector(mui->monkeyCommands);
		    pthread_mutex_unlock(&(m->monkeyCondMutex));

		    // Leave
		    break;
	       }

	       vector<char *>::iterator it = mui->monkeyCommands.begin();

	       while (it != mui->monkeyCommands.end())
	       {
		    // Send cmd
		    if (sockWrite(socket, (*it), strlen((*it))) < 0)
		    {
			 mui->monkeyConnected = false;
			 ResetScanningState();

			 // The server has gone
			 printf("   - ERROR: The Monkey Server Went Tits Up\n");

			 ClearCharVector(mui->monkeyCommands);

			 pthread_mutex_unlock(&(mui->monkeyCondMutex));

			 // Cancel the Listener thread
			 session->running = false;
			 pthread_cancel(threadSession);
			 pthread_join(threadSession, NULL);
			 delete session;
			 session = NULL;

			 // Try to reconnect
			 while ((socket = sockClient(mui->monkeyServerName.c_str(), mui->monkeyServerPort, "tcp")) < 0)
			 {
			      if (!mui->isRunning)
			      {
				   break;
			      }
			      
			      sockErr("   - sockClient() (Monkey Server) failed, waiting 5s\n");
			      
			      for (uint8_t i = 0; i < 50; i++)
			      {
				   usleep(100000);
				   pthread_yield();
				   
				   if (!mui->isRunning)
				   {
					break;
				   }
			      }
			 }

			 // Are we leaving
			 if (!m->isRunning)
			 {
			      ClearCharVector(mui->monkeyCommands);

			      pthread_mutex_unlock(&(m->monkeyCondMutex));

			      pthread_exit(NULL);
			 }

			 mui->monkeyConnected = true;
     			 sockErr("   - Connected to Monkey server %s:%d\n", mui->monkeyServerName.c_str(), mui->monkeyServerPort);

			 // Recreate listener thread
			 session = new ListenerSession(socket);

			 if (pthread_create(&threadSession, NULL, ThreadListener, session) != 0)
			 {
			      perror("pthread_create");
			      m->isRunning = false;
			      pthread_exit(NULL);
			 }
			 else
			 {
			      while(!session->hasStarted)
			      {
				   pthread_yield();
			      }

			      pthread_mutex_lock(&(mui->monkeyCondMutex));

			      // Check if doesn't already exists
			      if (std::find(mui->monkeyCommands.begin(), mui->monkeyCommands.end(), "X\n") == mui->monkeyCommands.end())
			      {
				   // ReSync
				   char Xcmd[3];
				   sprintf(Xcmd, "%c%c", 'X', '\n');
				   mui->monkeyCommands.push_back(strdup(Xcmd));
			      }

			      it = mui->monkeyCommands.begin();
			      continue;
			 }
		    }
		    else
		    {
			 // Write success

			 // Are we leaving
			 if (!m->isRunning)
			 {
			      mui->monkeyConnected = false;
			      session->running = false;
			      pthread_cancel(threadSession);
			      pthread_join(threadSession, NULL);
			      delete session;
			      session = NULL;

			      ClearCharVector(mui->monkeyCommands);

			      // Unlock cond mutex, then leave
			      pthread_mutex_unlock(&(m->monkeyCondMutex));
			      pthread_exit(NULL);
			 }
		    }

		    it++;
	       }

	       ClearCharVector(mui->monkeyCommands);

	       // Unlock cond mutex
	       pthread_mutex_unlock(&(m->monkeyCondMutex));

	  } while (m->isRunning);
     }
     else
     {
	  mui->monkeyConnected = false;
	  
	  if (session)
	  {
	       session->running = false;
	       pthread_cancel(threadSession);
	       pthread_join(threadSession, NULL);
	       delete session;
	       session = NULL;
	  }

	  ClearCharVector(mui->monkeyCommands);
     }

     pthread_exit(NULL);
}

//
// Send commands to Monkey server
//
static void SendToMONKEY(const char *fmt, ...)
{
     if (fmt && mui->isRunning && mui->monkeyConnected)
     {
	  char     buf[256] = {0};
	  va_list  args;

	  va_start(args, fmt);
	  vsnprintf(buf, (sizeof(buf) - 2), fmt, args);
	  va_end(args);

	  // Each line sent is '\n' terminated
	  if((buf[strlen(buf)] == '\0') && (buf[strlen(buf) - 1] != '\n'))
	  {
	       strlcat(buf, "\n", sizeof(buf));
	  }

	  // Don't send status update if the card is scanning
	  if ((mui->isDABScanning || mui->isFMScanning) && (buf[0] == 's') && mui->scanRequestedByMe)
	  {
	       return;
	  }
	  else if ((buf[0] == 'D') || (buf[0] == 'F'))
	  {
	       mui->scanRequestedByMe = true;
	  }

	  // lock
	  pthread_mutex_lock(&mui->monkeyCondMutex);

	  // prepare cmd
	  mui->monkeyCommands.push_back(strdup(buf));

	  // semaphore trigger
	  pthread_cond_signal(&mui->monkeyCond);

	  // unlock
	  pthread_mutex_unlock(&mui->monkeyCondMutex);
     }
}

//
// Send periodically status command to Monkey server
//
static void GetRadioStatus_Task(void *data)
{
     SendToMONKEY("s");
     if (mui->isRunning)
     {
	  Fl::repeat_timeout(3.0, GetRadioStatus_Task);
     }
}

//
// Thread that sends data to GPIOD server
//
static void *ThreadGPIOD(void *data)
{
     int                socket = -1;
     sigset_t           sigpipe_mask;

     pthread_yield();
     
     sigemptyset(&sigpipe_mask);
     sigaddset(&sigpipe_mask, SIGPIPE);
     if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, NULL) == -1)
     {
	  perror("pthread_sigmask");
     }

     sockErr("   - Trying to connect GPIOD server %s:%d\n", mui->monkeyServerName.c_str(), mui->gpiodServerPort);
     while ((socket = sockClient(mui->monkeyServerName.c_str(), mui->gpiodServerPort, "tcp")) < 0)
     {
	  if (!mui->isRunning)
	  {
	       break;
	  }
	  
	  sockErr("   - sockClient() (GPIOD Server) failed, waiting 5s\n");
	  
	  for (uint8_t i = 0; i < 50; i++)
	  {
	       usleep(100000);
	       pthread_yield();
	       
	       if (!mui->isRunning)
	       {
		    break;
	       }
	  }
     }

     if (mui->isRunning)
     {
	  mui->gpiodConnected = true;

	  sockErr("   - Connected to GPIOD server %s:%d\n", mui->monkeyServerName.c_str(), mui->gpiodServerPort);

	  do
	  {
	       pthread_mutex_lock(&(mui->gpiodCondMutex));
	       pthread_cond_wait((&mui->gpiodCond), &(mui->gpiodCondMutex));

	       // We're leaving
	       if (!mui->isRunning)
	       {
		    mui->gpiodConnected = false;
		    ClearCharVector(mui->gpiodCommands);
		    pthread_mutex_unlock(&(mui->gpiodCondMutex));

		    // Leave
		    break;
	       }

	       vector<char *>::iterator it = mui->gpiodCommands.begin();

	       while (it != mui->gpiodCommands.end())
	       {
		    if (sockWrite(socket, (*it), strlen((*it))) < 0)
		    {
			 mui->gpiodConnected = false;
			 pthread_mutex_unlock(&(mui->gpiodCondMutex));

			 // The server has gone
			 printf("   - ERROR: The GPIOD Server Went Tits Up\n");

			 // Try to reconnect
			 while ((socket = sockClient(mui->monkeyServerName.c_str(), mui->gpiodServerPort, "tcp")) < 0)
			 {
			      if (!mui->isRunning)
			      {
				   break;
			      }
			      
			      sockErr("   - sockClient() (GPIOD Server) failed, waiting 5s\n");
			      
			      for (uint8_t i = 0; i < 50; i++)
			      {
				   usleep(100000);
				   pthread_yield();
				   
				   if (!mui->isRunning)
				   {
					break;
				   }
			      }
			 }

			 // Are we leaving
			 if (!mui->isRunning)
			 {
			      ClearCharVector(mui->gpiodCommands);

			      pthread_exit(NULL);
			 }

			 mui->gpiodConnected = true;
			 pthread_mutex_lock(&(mui->gpiodCondMutex));
			 sockErr("   - Connected to GPIOD server %s:%d\n", mui->monkeyServerName.c_str(), mui->gpiodServerPort);

			 it = mui->gpiodCommands.begin();
			 continue;
		    }
		    else
		    {
			 // Write success

			 // Are we leaving
			 if (!mui->isRunning)
			 {
			      mui->gpiodConnected = false;
			      ClearCharVector(mui->gpiodCommands);

			      // Unlock mutex and cond, then leave
			      pthread_mutex_unlock(&(mui->gpiodCondMutex));
			      pthread_exit(NULL);
			 }
		    }

		    it++;
	       }

	       ClearCharVector(mui->gpiodCommands);

	       pthread_mutex_unlock(&(mui->gpiodCondMutex));

	  } while(mui->isRunning);
     }
     
     mui->gpiodConnected = false;

     pthread_exit(NULL);
}

//
// Send periodically PING command to GPIOD server
//
static void *ThreadPingGPIOD(void *data)
{
     pthread_detach(pthread_self());

     do
     {
	  sleep(60 * 5);

	  if (mui->isRunning && mui->gpiodConnected)
	  {
	       SendToGPIOD(GPIOD_PING);
	  }

     } while(mui->isRunning);

     pthread_exit(NULL);
}

//
// Open GPSD connection
//
static void gpsOpenDevice()
{
     mui->gpsAvailable = false;
     
     if ((gps_open(mui->gpsServerName.c_str(), mui->gpsServerPort.c_str(), &mui->gpsData)) == -1)
     {
	  fprintf(stderr, "   - GPS open error code: %d, reason: %s\n", errno, gps_errstr(errno));
	  return;
     }
     else
     {
	  gps_stream(&mui->gpsData, WATCH_ENABLE | WATCH_JSON, NULL);
     }
     
     mui->gpsAvailable = true;
}

//
// Update the color theme from the Sun elevation, accordinlgy to GPS position
// Some code has been taken from Navit
//
static void UpdateTheme(bool positionIsValid, time_t currTime)
{
     struct tm    gmt;
     double       trise, tset, lat, lon;
     bool         afterSunrise = false;
     bool         afterSunset = false;

     if (positionIsValid)
     {
	  lat = mui->gpsLatitude = mui->gpsData.fix.latitude;
	  lon = mui->gpsLongitude = mui->gpsData.fix.longitude;
	  
#if GPSD_API_MAJOR_VERSION >= 9
	  currTime = mui->gpsData.fix.time.tv_sec;
#else
	  currTime = mui->gpsData.fix.time;
#endif
     }
     else
     {
	  lat = mui->gpsLatitude;
	  lon = mui->gpsLongitude;
     }

     if (gmtime_r(&currTime, &gmt))
     {
	  if (__sunriset__((gmt.tm_year + 1900), (gmt.tm_mon + 1), gmt.tm_mday, lon, lat, -5,1, &trise, &tset) == 0)
	  {
	       //printf("trise: %u:%u\n", HOURS(trise), MINUTES(trise));
	       //printf("tset: %u:%u\n", HOURS(tset), MINUTES(tset));
	       
	       if (((HOURS(trise) * 60) + MINUTES(trise)) < ((currTime % 86400) / 60))
	       {
		    afterSunrise = true;
	       }
	       
	       if (((((HOURS(tset) * 60) + MINUTES(tset)) < ((currTime % 86400) / 60))) ||
		   ((((HOURS(trise) * 60) + MINUTES(trise)) > ((currTime % 86400) / 60))))
	       {
		    afterSunset = true;
	       }
	       
	       if (afterSunrise && (afterSunset == false) && (mui->currentDaytime == NIGHT))
	       {
		    SetDaytimeColors(DAY);
	       }
	       else if (afterSunset && (mui->currentDaytime == DAY))
	       {
		    SetDaytimeColors(NIGHT);
	       }
	  }
     }
}

//
// Task that updates the clock and speed informations
//
static void UpdateTimeAndSpeed_Task(void *data)
{
     static int8_t   hasGPS = -1;
     static int8_t   hasFIX = -1;
     bool            updateSpeed = false;
     time_t          currTime;
     struct tm       timeInfo;
     char            gpsSpeed[9] = { 0 }; // maxlen: "[No Fix]"
     char            timeData[6] = { 0 };
     bool            positionIsValid = false;
     struct timeval  tv;

     if (mui->gpsAvailable)
     {
	  int rlen;
	  
	  if (hasGPS <= 0)
	  {
	       hasGPS = 1;
	  }
	  
#if GPSD_API_MAJOR_VERSION >= 7
	  while ((rlen = gps_read(&mui->gpsData, NULL, 0)) > 0);
#else
	  while ((rlen = gps_read(&mui->gpsData)) > 0);
#endif
	  // Error
	  if (rlen == -1)
	  {
	       updateSpeed = true;
	       snprintf(gpsSpeed, sizeof(gpsSpeed), "%s", "[ERROR]");
	    
	       fprintf(stderr, "error occured reading gps data. code: %d, reason: %s\n", errno, gps_errstr(errno));
	       gps_close(&mui->gpsData);
	       gpsOpenDevice(); // will update the speed on the next iteration
	       hasGPS = 0;
	       hasFIX = 0;
	  }
	  else
	  {
	       /* Display data from the GPS receiver. */
	       if ((positionIsValid = ((mui->gpsData.set & STATUS_SET) && (mui->gpsData.status >= STATUS_FIX) && (mui->gpsData.set & LATLON_SET) &&
				       ((mui->gpsData.fix.mode == MODE_2D) || (mui->gpsData.fix.mode == MODE_3D))))
		   && ((mui->gpsData.set & SPEED_SET) && (!isnan(mui->gpsData.fix.speed))))
	       {
		    updateSpeed = true;
		    
		    if (hasFIX <= 0)
		    {
			 hasFIX = 1;
			 speedInfo->labelcolor(mui->foregroundColor);
			 speedInfo->labelsize(35);
		    }
		    
		    // m/s -> km/h
		    if (mui->gpsData.fix.speed < 0.55) // less than 2km/h
		    {
			 snprintf(gpsSpeed, sizeof(gpsSpeed), "< 2.0");
		    }
		    else
		    {
			 snprintf(gpsSpeed, sizeof(gpsSpeed), "%.1f", (mui->gpsData.fix.speed * MPS_TO_KPH));
		    }
	       }
	       else
	       {
		    if ((hasFIX == -1) || (hasFIX == 1))
		    {
			 hasFIX = 0;
			 snprintf(gpsSpeed, sizeof(gpsSpeed), "%s", "[No FIX]");
			 speedInfo->labelcolor(Fl_Color(31));
			 speedInfo->labelsize(20);
			 updateSpeed = true;
		    }
	       }
	  }

	  if (updateSpeed)
	  {
	       speedInfo->copy_label(gpsSpeed);
	  }
     }
     else
     {
	  if ((hasGPS == -1) || (hasGPS == 1))
	  {
	       hasGPS = 0;
	       hasFIX = 0;
	       speedInfo->copy_label("[No GPS]");
	       speedInfo->labelcolor(Fl_Color(31));
	       speedInfo->labelsize(20);
	  }
     }

     // Get time
     tzset(); // Follow the system timezone
     gettimeofday(&tv, NULL);
     currTime = tv.tv_sec;
     localtime_r(&currTime, &timeInfo);
     sprintf(timeData, "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
     clockInfo->copy_label(timeData);

     //
     // Automatic Day/Night mode.
     // Daytime may be overriden by user, for 30 minutes
     //
     if (mui->overrideDaytime <= currTime)
     {
	  UpdateTheme(positionIsValid, currTime);
     }

     if (mui->isRunning)
     {
	  Fl::flush();
	  Fl::repeat_timeout(0.5, UpdateTimeAndSpeed_Task);
     }
}

//
// Handles navigation button click events (raises Navit window)
//
static void Navigation_CB(Fl_Widget *w, void *data)
{
     if(Shell(string(ScriptsDir + "ToggleMediaNavit.sh")) < 0)
     {
	  printf("Could not Toggle to Navigation.\n");
     }
     else
     {

     }
}

//
// Handles strength button click events (toggles fullscreen/windowed mode)
//
static void StrengthIcon_CB(Fl_Widget *w, void *data)
{
     if (win->fullscreen_active() != 0)
     {
	  win->fullscreen_off();
     }
     else
     {
	  win->fullscreen();
     }
}

//
// Handles click on DAB browser entries
//
static void DABBrowser_CB(Fl_Widget *w, void *data)
{
     Fl_Browser  *b = (Fl_Browser *)w;
     size_t       index = b->value();

     // Out of bounds (like clicking is an empty browser)
     if ((index == 0) || (index > (size_t)b->size()))
     {
	  return;
     }

     ClearInfos();
     if (index == 1)
     {
	  DABBrowser->labelfont(MAC_FONT);
	  DABBrowser->textfont(MAC_FONT);
	  ClearBrowser(DABBrowser);
	  ClearInfos();
	  ClearDABVector();
	  SendToMONKEY("D");
     }
     else if (index > 1)
     {
	  if (mui->DABVector.size() && ((index - 1) <= mui->DABVector.size()))
	  {
	       MPDPlay(false);
	       UpdateInfosAfterChannelSelection(mui->DABVector[(index - 2)]);
	       mui->timeChannelSelection = time(NULL);
	       SendToMONKEY("u");
	       sleep(1);
	       SendToMONKEY("%d", mui->DABVector[(index - 2)]->Index);
	  }
     }
}

//
// Handles click on FM browser entries
//
static void FMBrowser_CB(Fl_Widget *w, void *data)
{
     Fl_Browser  *b = (Fl_Browser *)w;
     size_t       index = b->value();

     // Out of bounds (like clicking is an empty browser)
     if ((index == 0) || (index > (size_t)b->size()))
     {
	  return;
     }

     ClearInfos();

     if (index == 1)
     {

	  ClearBrowser(FMBrowser);
	  ClearInfos();
	  ClearFMVector();
	  SendToMONKEY("F");
     }
     else if (index > 1)
     {
	  if (mui->FMVector.size() && ((index - 1) <= mui->FMVector.size()))
	  {
	       if ((mui->FMVector[(index - 2)]->Index >= FMFREQ_MIN) && (mui->FMVector[(index - 2)]->Index <= FMFREQ_MAX))
	       {
		    MPDPlay(false);
		    UpdateInfosAfterChannelSelection(mui->FMVector[(index - 2)]);
		    mui->timeChannelSelection = time(NULL);
		    SendToMONKEY("u");
		    sleep(1);
		    SendToMONKEY("%d", mui->FMVector[(index - 2)]->Index);
	       }
	  }
     }
}

//
// Update the displayed icon in the MPD's play button (play/pause)
//
static void UpdatePlayerPlayButton(bool isPlaying)
{
     if (isPlaying)
     {
	  if (strcmp(PlayerKeys[PLAYER_KEY_PLAY]->label(), "@>") == 0)
	  {
	       PlayerKeys[PLAYER_KEY_PLAY]->copy_label(Player_Keys_Labels[PLAYER_KEY_MAX]);
	  }
     }
     else
     {
	  if (strcmp(PlayerKeys[PLAYER_KEY_PLAY]->label(), "@||") == 0)
	  {
	       PlayerKeys[PLAYER_KEY_PLAY]->copy_label(Player_Keys_Labels[PLAYER_KEY_PLAY]);
	  }
     }
}

//
// Reflect the current MPD status (play/pause/stop/song position/playlist content/etc...)
//
static void CurrentSong_Task(void *data)
{
     MPDBegin();
     if (mui->mpd)
     {
	  struct mpd_status *status;
	  bool isPlaying;

	  mpd_command_list_begin(mui->mpd, true);
	  mpd_send_status(mui->mpd);
	  mpd_send_current_song(mui->mpd);
	  mpd_command_list_end(mui->mpd);

	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	       if (mui->isRunning)
	       {
		    Fl::repeat_timeout(1.0, CurrentSong_Task);
	       }
	       return;
	  }

	  if (mpd_status_get_error(status) != NULL)
	  {
	       printf("MPD status error: %s\n", mpd_status_get_error(status));
	       MPDEnd();
	       if (mui->isRunning)
	       {
		    Fl::repeat_timeout(1.0, CurrentSong_Task);
	       }
	       mpd_status_free(status);
	       return;
	  }
	  MPDCheck();

	  uint32_t playlistCount = mpd_status_get_queue_length(status);
	  bool playlistEmpty = (playlistCount == 0);

	  // if in playlists mode, and queue is not empty, update
	  if ((mui->plListMode == PLMODE_PLAYLISTS) &&
	      ((mpd_status_get_state(status) == MPD_STATE_PLAY) ||
	       (mpd_status_get_state(status) == MPD_STATE_PAUSE) ||
	       (mpd_status_get_state(status) == MPD_STATE_STOP)) && (playlistEmpty == false))
	  {
	       PLBrowserUpdateTrackList(2, false);
	  } // if in tracks mode and playlist is empty, update
	  else if ((mui->plListMode == PLMODE_TRACKS) && (mpd_status_get_state(status) == MPD_STATE_STOP) && playlistEmpty)
	  {
	       int dummy = 1;

	       PLBrowser->value(1);
	       PLBrowser_CB(PLBrowser, &dummy);
	  } // if in tracks mode and playlist is non empty, but count mismatch
	  else if ((mui->plListMode == PLMODE_TRACKS) && (mpd_status_get_state(status) == MPD_STATE_STOP)
		   && ((playlistEmpty == false) && (playlistCount != uint32_t((PLBrowser->size() - 1)))))
	  {
	       mui->plListMode = PLMODE_PLAYLISTS;
	       PLBrowserUpdateTrackList(2, false);
	  }

	  isPlaying = (mpd_status_get_state(status) == MPD_STATE_PLAY);

	  if (mpd_status_get_state(status) != MPD_STATE_UNKNOWN)
	  {
	       // Update selected entry in the browser if the current selected song has changed
	       if (mpd_status_get_total_time(status) &&
		   (playlistEmpty == false) && ((PLBrowser->value() == 0) || ((PLBrowser->value() - 2) != mpd_status_get_song_pos(status))))
	       {
		    PLBrowser->select((mpd_status_get_song_pos(status) + 2), 1);
		    PlayerSeek->maximum(double(mpd_status_get_total_time(status)));
	       }
	  }

	  if (isPlaying)
	  {
	       PlayerSeek->value(double(mpd_status_get_elapsed_time(status)));
	  }
	  else
	  {
	       if (PlayerSeek->value() != double(mpd_status_get_elapsed_time(status)))
	       {
		    PlayerSeek->value(double(mpd_status_get_elapsed_time(status)));
	       }
	  }

	  bool random = mpd_status_get_random(status);
	  uint32_t crossfade = mpd_status_get_crossfade(status);

	  if (mui->playerRandom != random)
	  {
	       mui->playerRandom = random;
	       // widget
	       PlayerKeys[PLAYER_KEY_RANDOM]->value(random);
	  }

	  if (mui->playerCrossfade != crossfade)
	  {
	       mui->playerCrossfade = crossfade;
	       // widget
	       PlayerCrossfade->value(crossfade);
	  }
	  MPDEnd();

	  UpdatePlayerPlayButton(isPlaying);
	  mpd_status_free(status);
     }

     if (mui->isRunning)
     {
	  Fl::repeat_timeout(1.0, CurrentSong_Task);
     }
}

//
// Called when an item ( > 1 ) is clicked in the Playlist browser (PLAYLISTS/TRACKS modes)
//
static void PLBrowserUpdateTrackList(size_t index, bool startPlayback)
{
     // Reset slider position
     PlayerSeek->value(0);

     // Currently display the list of playlists
     if(mui->plListMode == PLMODE_PLAYLISTS)
     {
	  // We list all songs into a playlist text file:
	  MPDBegin();
	  if (mui->mpd)
	  {
	       struct mpd_entity *entity;

	       if (startPlayback)
	       {
		    // Clear the queue (current playlist)
		    if (!mpd_send_clear(mui->mpd))
		    {
			 MPDEnd();
			 return;
		    }
		    // Check clear
		    MPDCheck();

		    if (!mui->mpd || !mpd_send_load(mui->mpd, PLBrowser->text(index)))
		    {
			 MPDEnd();
			 return;
		    }
		    // Check load
		    MPDCheck();
	       }

	       if (!mui->mpd || !mpd_send_list_queue_range_meta(mui->mpd, 0, (unsigned int)-1))
	       {
		    MPDEnd();
		    return;
	       }

	       MSBrowser->clear();
	       ClearBrowser(PLBrowser);

	       while ((entity = mpd_recv_entity(mui->mpd)) != NULL)
	       {
		    const struct mpd_song *song;
		    const char            *uri = NULL;
		    const char            *artist = NULL;
		    const char            *title = NULL;
		    char                   buf[64];

		    // We are leaving
		    if (mui->isRunning == false)
		    {
			 return;
		    }

		    switch (mpd_entity_get_type(entity))
		    {
		    case MPD_ENTITY_TYPE_SONG:
			 song = mpd_entity_get_song(entity);
			 uri = mpd_song_get_uri(song);
			 artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
			 title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);

			 if (artist && title)
			 {
			      snprintf(buf, sizeof(buf) - 1, "@r%.20s \t %.31s", artist, title);
			 }
			 else
			 {
			      // No tag are available, just put the URI
			      snprintf(buf, sizeof(buf) - 1, "@c%.30s", uri);
			 }

			 //Fl::lock();
			 PLBrowser->add(buf);
			 //Fl::unlock();

			 break;

		    case MPD_ENTITY_TYPE_UNKNOWN:
		    case MPD_ENTITY_TYPE_DIRECTORY:
		    case MPD_ENTITY_TYPE_PLAYLIST:
			 // ignore
			 break;
		    }

		    mpd_entity_free(entity);
	       }
	       MPDEnd();

	       mui->plListMode = PLMODE_TRACKS;
	  }

     }
     else if (mui->plListMode == PLMODE_TRACKS)
     {
	  struct mpd_status *status;

	  /* MPD mode for gpiod to stop arecord for DAB / FM i2s */
	  SendToMONKEY("m");
	  sleep(1);
	  SendToGPIOD(GPIOD_SOURCE_MPD);
	  MPDPlay(true);

	  MPDBegin();

	  if (startPlayback)
	  {
	       if (!mui->mpd || !mpd_send_play_pos(mui->mpd, (index - 2)))
	       {
			 printf("mpd_send_play_pos(%d) failed\n", int(index - 2));
			 MPDEnd();
			 return;
	       }
	       MPDCheck();
	  }

	  if (!mui->mpd || !mpd_send_status(mui->mpd))
	  {
	       printf("mpd_send_status(%d) failed\n", int(index - 2));
	       MPDEnd();
	       return;
	  };

	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	       return;
	  }

	  if (mpd_status_get_state(status) == MPD_STATE_PLAY)
	  {
	       // Slider max to TotalTime
	       PlayerSeek->maximum(double(mpd_status_get_total_time(status)));
	       PlayerSeek->value(double(mpd_status_get_elapsed_time(status)));
	  }
	  MPDEnd();

	  mpd_status_free(status);
     }
}

//
// Handles click event on the playlist browser
//
static void PLBrowser_CB(Fl_Widget *w, void *data)
{
     Fl_Browser  *b = (Fl_Browser *) w;
     size_t       index = b->value();
     bool         rebuildAllSongs = (data == NULL) ? true : false;

     // Out of bounds (like clicking is an empty browser)
     if ((index == 0) || (index > (size_t)b->size()))
     {
	  return;
     }

     if (index == 1)
     {
	  struct mpd_playlist *playlist;

	  ClearBrowser(PLBrowser);
	  MSBrowser->clear();

	  // Disable player controls.
	  UpdatePlayerPlayButton(false);
	  UpdateActiveStateFromMPDStatus();

	  // Reset slider position
	  PlayerSeek->value(0);

	  // Rebuild the AllSongs playlist
	  if (rebuildAllSongs)
	  {
	       QueryMPDPlaylist(QUERY_MAX);
	  }

	  if (mui->isRunning == false)
	  {
	       return;
	  }

	  // Now get all the available playlists
	  MPDBegin();
	  if (!mui->mpd || !mpd_send_list_playlists(mui->mpd))
	  {
	       MPDEnd();
	       return;
	  }

	  while ((playlist = mpd_recv_playlist(mui->mpd)) != NULL)
	  {
	       // We are leaving
	       if (mui->isRunning == false)
	       {
		    return;
	       }

	       PLBrowser->add(mpd_playlist_get_path(playlist));

	       mpd_playlist_free(playlist);
	  }
	  MPDEnd();

	  mui->plListMode = PLMODE_PLAYLISTS; /* Playlists list has been created, and will be displayed */
     }
     else // index > 1
     {
	  PLBrowserUpdateTrackList(index);
     }
}

//
// Handles seek events in the player's slider
//
static void PlayerSeek_CB(Fl_Widget *w, void *data)
{
     Fl_Counter *b = (Fl_Counter *)w;

     MPDBegin();
     if (mui->mpd)
     {
	  struct mpd_status *status;

	  if (!mui->mpd || !mpd_send_current_song(mui->mpd))
	  {
	       printf("mpd_send_current_song() failed\n");
	       MPDEnd();
	       return;
	  }
	  MPDCheck();

	  if (!mui->mpd || !mpd_send_status(mui->mpd))
	  {
	       printf("mpd_send_status() failed\n");
	       MPDEnd();
	       return;
	  }

	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	       return;
	  }

	  if (mpd_status_get_error(status) != NULL)
	  {
	       printf("MPD status error: %s\n", mpd_status_get_error(status));
	       MPDEnd();
	       mpd_status_free(status);
	       return;
	  }

	  // Seek in current song
	  if (!mpd_run_seek_id(mui->mpd, mpd_status_get_song_id(status), int(b->value())))
	  {
	       printf("mpd_run_seek_id(%d) failed\n", int(b->value()));
	  }
	  MPDEnd();

	  mpd_status_free(status);
     }
}

//
// Handles events on crossfade counter widget
//
static void PlayerCrossfade_CB(Fl_Widget *w, void *data)
{
     Fl_Counter *b = (Fl_Counter *)w;

     MPDBegin();
     if (!mui->mpd || !mpd_send_crossfade(mui->mpd, int(b->value())))
     {
	  printf("mpd_send_crossfade(%d) failed.\n", int(b->value()));
     }
     MPDEnd();

     mui->playerCrossfade = (uint8_t) b->value();
}

//
// Handles click events in the Music Search browser
//
static void MSBrowser_CB(Fl_Widget *w, void *data)
{
     Fl_Browser        *b = (Fl_Browser*) w;
     size_t             index = b->value();
     struct mpd_status *status;


     if ((index == 0) || (index > (size_t)b->size()))
     {
	  return;
     }

     /* MPD mode for gpiod to stop arecord for DAB / FM i2s */
     SendToMONKEY("m");
     sleep(1);
     SendToGPIOD(GPIOD_SOURCE_MPD);
     MPDPlay(true);

     MPDBegin();
     if (!mui->mpd || !mpd_send_play_pos(mui->mpd, (index - 1)))
     {
	  printf("mpd_send_play_pos(%d) failed\n", int(index - 1));
	  MPDEnd();
	  return;
     }
     MPDCheck();

     if (!mui->mpd || !mpd_send_status(mui->mpd))
     {
	  printf("mpd_send_status(%d) failed\n", int(index - 1));
	  MPDEnd();
	  return;
     };

     if ((status = mpd_recv_status(mui->mpd)) == NULL)
     {
	  printf("mpd_recv_status() failed\n");
	  MPDEnd();
	  return;
     }

     if (mpd_status_get_state(status) == MPD_STATE_PLAY)
     {
	  // Slider max to TotalTime
	  PlayerSeek->maximum(double(mpd_status_get_total_time(status)));
	  PlayerSeek->value(double(mpd_status_get_elapsed_time(status)));
     }
     MPDEnd();

     mpd_status_free(status);
}

//
// Handles click events on the preset buttons
//
static void EQPreset_CB(Fl_Widget *w, void *data)
{
     //Fl_Button  *b = (Fl_Button *)w;
     EQPreset_t *preset = (EQPreset_t *)data;

     if (preset)
     {
	  SendToMONKEY("%s", preset->command);
     }
}

//
// Handles click events on the random/prev/play/next buttons
//
static void PlayerRandomPlayNextPrev_CB(Fl_Widget *w, void *data)
{
     if (w == PlayerKeys[PLAYER_KEY_PLAY])
     {
	  struct mpd_status *status;

	  MPDBegin();
	  if (!mui->mpd || !mpd_send_toggle_pause(mui->mpd))
	  {
	       printf("mpd_send_toggle_pause() failed.\n");
	  }
	  MPDCheck();

	  if (!mui->mpd || !mpd_send_status(mui->mpd))
	  {
	       printf("mpd_send_status() failed\n");
	       MPDEnd();
	       return;
	  };

	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	       return;
	  }

	  SendToMONKEY((mpd_status_get_state(status) == MPD_STATE_PLAY) ? "m" : "u");
	  if (mpd_status_get_state(status) == MPD_STATE_PLAY)
	  {
	       sleep(1);
	       SendToGPIOD(GPIOD_SOURCE_MPD);
	  }
	  UpdatePlayerPlayButton((mpd_status_get_state(status) == MPD_STATE_PLAY));

	  MPDEnd();
	  mpd_status_free(status);
     }
     else if (w == PlayerKeys[PLAYER_KEY_PREV])
     {
	  struct mpd_status *status;

	  MPDBegin();
	  if (!mui->mpd || !mpd_send_status(mui->mpd))
	  {
	       printf("mpd_send_status() failed\n");
	       MPDEnd();
	       return;
	  };

	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	       return;
	  }

	  if (mpd_status_get_state(status) != MPD_STATE_PLAY)
	  {
	       if (!mpd_send_play(mui->mpd))
	       {
		    MPDEnd();
		    mpd_status_free(status);
		    return;
	       }
	       MPDCheck();
	  }

	  if (!mui->mpd || !mpd_send_previous(mui->mpd))
	  {
	       printf("mpd_send_previous() failed.\n");
	  }
	  MPDCheck();

	  if (!mui->mpd || !mpd_send_status(mui->mpd))
	  {
	       printf("mpd_send_status() failed\n");
	       MPDEnd();
	       mpd_status_free(status);
	       return;
	  };

	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	       mpd_status_free(status);
	       return;
	  }
	  MPDEnd();

	  SendToMONKEY((mpd_status_get_state(status) == MPD_STATE_PLAY) ? "m" : "u");
	  if (mpd_status_get_state(status) == MPD_STATE_PLAY)
	  {
	       sleep(1);
	       SendToGPIOD(GPIOD_SOURCE_MPD);
	  }
	  mpd_status_free(status);
     }
     else if (w == PlayerKeys[PLAYER_KEY_NEXT])
     {
	  struct mpd_status *status;

	  MPDBegin();
	  if (!mui->mpd || !mpd_send_status(mui->mpd))
	  {
	       printf("mpd_send_status() failed\n");
	       MPDEnd();
	       return;
	  };

	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	       return;
	  }

	  if (mpd_status_get_state(status) != MPD_STATE_PLAY)
	  {
	       if (!mpd_send_play(mui->mpd))
	       {
		    MPDEnd();
		    mpd_status_free(status);
		    return;
	       }
	       MPDCheck();
	  }

	  if (!mui->mpd || !mpd_send_next(mui->mpd))
	  {
	       printf("mpd_send_next() failed.\n");
	  }
	  MPDCheck();

	  if (!mui->mpd || !mpd_send_status(mui->mpd))
	  {
	       printf("mpd_send_status() failed\n");
	       MPDEnd();
	       mpd_status_free(status);
	       return;
	  };

	  mpd_status_free(status);
	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	       return;
	  }
	  MPDEnd();

	  SendToMONKEY((mpd_status_get_state(status) == MPD_STATE_PLAY) ? "m" : "u");
	  if (mpd_status_get_state(status) == MPD_STATE_PLAY)
	  {
	       sleep(1);
	       SendToGPIOD(GPIOD_SOURCE_MPD);
	  }
	  mpd_status_free(status);
     }
     else if (w == PlayerKeys[PLAYER_KEY_RANDOM])
     {
	  Fl_Button *b = (Fl_Button *)w;

	  MPDBegin();
	  if (!mui->mpd || !mpd_send_random(mui->mpd, b->value() ? true : false))
	  {
	       printf("mpd_send_random(%d) failed.\n", b->value());
	  }
	  MPDEnd();

	  mui->playerRandom = b->value();
     }


}

//
// Handles click event on the bluetooth (phone tab) keys.
//
static void BluetoothKey_CB(Fl_Widget *w, void *data)
{
     Fl_Button *b = (Fl_Button *)w;
     BluetoothKey_t *key = (BluetoothKey_t *)data;

     if (key == NULL)
     {
	  return;
     }

     switch (key->key)
     {
     	case BLUETOOTH_KEY_0:
        case BLUETOOTH_KEY_1:
        case BLUETOOTH_KEY_2:
        case BLUETOOTH_KEY_3:
        case BLUETOOTH_KEY_4:
        case BLUETOOTH_KEY_5:
        case BLUETOOTH_KEY_6:
        case BLUETOOTH_KEY_7:
        case BLUETOOTH_KEY_8:
        case BLUETOOTH_KEY_9:
        case BLUETOOTH_KEY_A:
        case BLUETOOTH_KEY_B:
        case BLUETOOTH_KEY_C:
        case BLUETOOTH_KEY_D:
        case BLUETOOTH_KEY_E:
        case BLUETOOTH_KEY_F:
        case BLUETOOTH_KEY_COLON:
	     MACQueryText->replace(MACQueryText->position(), MACQueryText->mark(), b->label(), 1);

	     if (MACQueryText->position() == 17) /* save the current MAC address to  */
	     {
		  mui->bluetoothMAC = MACQueryText->value();
		  // Tell GPIOD
		  SendToGPIOD(GPIOD_BLUETOOTH_MAC, mui->bluetoothMAC.c_str());
		  sockErr("   - New bluetooth address: %s\n", mui->bluetoothMAC.c_str());
	     }
	     break;

     	case BLUETOOTH_KEY_DEL:
	     if (MACQueryText->mark() != MACQueryText->position())
	     {
		  MACQueryText->cut();
	     }
	     else
	     {
		  MACQueryText->cut(-1);
	     }
	     break;

     	case BLUETOOTH_KEY_MEDIA:
	     /* A2DP mode for gpiod to stop arecord for DAB / FM i2s and MPD */
	     MPDPlay(false);
	     SendToGPIOD(GPIOD_SOURCE_AD2P);
	     break;

     	case BLUETOOTH_KEY_CALL:
	     /* SCO mode for gpiod to stop arecord for DAB / FM i2s and MPD */
	     MPDPlay(false);
	     SendToGPIOD(GPIOD_SOURCE_SCO);
	     break;

        case BLUETOOTH_KEY_STOP:
	     MPDPlay(false);
	     SendToGPIOD(GPIOD_PLAYBACK_STOP);
	     break;

        case BLUETOOTH_KEY_PAIR:
	     SendToGPIOD(GPIOD_BLUETOOTH_MAC);
	     break;

        case BLUETOOTH_KEY_TRUST:
	     SendToGPIOD(GPIOD_BLUETOOTH_TRUST);
	     break;

        case BLUETOOTH_KEY_CONN:
	     SendToGPIOD(GPIOD_BLUETOOTH_CONNECT);
	     break;

        case BLUETOOTH_KEY_MUTE:
	     SendToGPIOD(GPIOD_BLUETOOTH_MUTE_TOGGLE);
	     break;

        case BLUETOOTH_KEY_VOL_MORE:
	     SendToGPIOD(GPIOD_BLUETOOTH_VOLUME_UP);
	     break;

        case BLUETOOTH_KEY_VOL_LESS:
	     SendToGPIOD(GPIOD_BLUETOOTH_VOLUME_DOWN);
	     break;

        case BLUETOOTH_KEY_PREV:
	     SendToGPIOD(GPIOD_BLUETOOTH_KEY_PREVIOUS);
	     break;

        case BLUETOOTH_KEY_NEXT:
	     SendToGPIOD(GPIOD_BLUETOOTH_KEY_NEXT);
	     break;

        case BLUETOOTH_KEY_DISC:
	     SendToGPIOD(GPIOD_BLUETOOTH_DISCONNECT);
	     break;

        case BLUETOOTH_MAX_KEYS:
	     break;
	  }


     if (strcmp(b->label(), "Del") == 0)
     {
	  if (MACQueryText->mark() != MACQueryText->position())
	  {
	       MACQueryText->cut();
	  }
	  else
	  {
	       MACQueryText->cut(-1);
	  }
     }
     else  /* Other normal keys */
     {
     }

}

//
// Handles click events on the Query type buttons (music search tab)
//
static void MSQuery_CB(Fl_Widget *w, void *data)
{
     Fl_Round_Button *b = (Fl_Round_Button *)w;
     QueryCategory_t  qcat = *(QueryCategory_t *)data;

     if (b->value() == 1)
     {
	  mui->mpdQueryCategory = qcat;

	  for (uint8_t i = QUERY_ARTIST; i < QUERY_MAX; i++)
	  {
	       if (MSQueryCheckboxes[i] != b)
	       {
		    MSQueryCheckboxes[i]->value(0);
	       }
	  }
     }
     else
     {
	  b->value(1);
     }
}

//
// Build/send/process MPD database queries
//
static void QueryMPDPlaylist(QueryCategory_t category)
{
     // clear playlist
     MPDBegin();
     if (mui->mpd)
     {
	  vector<char *> allSongs;
	  struct mpd_entity *entity;
	  char playlistName[64];
	  char playlistNameNew[64 + 5];

	  memset(playlistName, 0, sizeof(playlistName));
	  allSongs.clear();

	  if (category < QUERY_MAX)
	  {
	       snprintf(playlistName, (sizeof(playlistName) - 1), "Last-%s-Search", MSQueryButtonsLabels[category].label);
	  }
	  else
	  {
	       snprintf(playlistName, (sizeof(playlistName) - 1), "%s", MSQueryButtonsLabels[category].label);
	  }
	  snprintf(playlistNameNew, (sizeof(playlistNameNew) - 1), "%s.new", playlistName);

	  // Clear the queue (current playlist)
	  if (!mpd_send_clear(mui->mpd))
	  {
	       printf("mpd_send_clear() failed\n");
	       MPDEnd();
	       return;
	  }
	  // Check clear
	  MPDCheck();

	  // update playlist
	  if (!mui->mpd || !mpd_send_update(mui->mpd, NULL))
	  {
	       printf("mpd_send_update() failed.\n");
	       MPDEnd();
	       return;
	  }

	  // Wait update to finish
	  // Thanks to mpc code ;-)
	  uint32_t id = 0;
	  while (true)
	  {
	       uint32_t next_id = mpd_recv_update_id(mui->mpd);

	       if (mui->isRunning == false)
	       {
		    return;
	       }

	       if (next_id == 0)
	       {
		    break;
	       }
	       id = next_id;
	  }

	  while (true)
	  {
	       // idle until an update finishes
	       enum mpd_idle idle = mpd_run_idle_mask(mui->mpd, MPD_IDLE_UPDATE);
	       struct mpd_status *status;
	       uint32_t current_id;

	       if (mui->isRunning == false)
	       {
		    return;
	       }

	       if (idle == 0)
	       {
		    MPDEnd();
		    return;
	       }

               // determine the current "update id"
	       if ((status = mpd_run_status(mui->mpd)) == NULL)
	       {
		    MPDEnd();
		    return;
	       }

	       current_id = mpd_status_get_update_id(status);

	       mpd_status_free(status);

                // is our last queued update finished now?
	       if (current_id == 0 || current_id > id || (id > (1 << 30) && id < 1000)) /* wraparound */
	       {
		    break;
	       }
	  }
	  // Now the playlist has been updated

	  if (category == QUERY_MAX)
	  {
	       const char *tmp = "";

	       if (mui->isRunning == false)
	       {
		    return;
	       }

	       if (!mui->mpd || !mpd_send_list_all_meta(mui->mpd, tmp))
	       {
		    printf("mpd_send_list_all_meta() failed\n");
		    MPDEnd();
		    return;
	       }

	       if (mui->isRunning == false)
	       {
		    return;
	       }

	       MSBrowser->clear();
	       ClearBrowser(PLBrowser);
	  }
	  else // This is a search
	  {

	       //filter -> into new queue + add result to MSBrowser.
	       if (!mui->mpd || !mpd_search_add_db_songs(mui->mpd, false))
	       {
		    printf("mpd_search_db_songs() failed\n");
		    MPDEnd();
		    return;
	       }

	       enum mpd_tag_type tagType = MPD_TAG_UNKNOWN;
	       switch(category)
	       {
	       case QUERY_ARTIST:
		    tagType = MPD_TAG_ARTIST;
		    break;

	       case QUERY_ALBUM:
		    tagType = MPD_TAG_ALBUM;
		    break;

	       case QUERY_TITLE:
		    tagType = MPD_TAG_TITLE;
		    break;

	       case QUERY_ANY:
	       case QUERY_FILE:
	       case QUERY_MAX:
		    break;
	       }


	       switch(category)
	       {
	       case QUERY_ARTIST:
	       case QUERY_ALBUM:
	       case QUERY_TITLE:
		    if (!mui->mpd || !mpd_search_add_tag_constraint(mui->mpd, MPD_OPERATOR_DEFAULT, tagType, MSQueryText->value()))
		    {
			 printf("mpd_search_add_tag_constraint() failed\n");
			 MPDEnd();
			 return;
		    }
		    break;

	       case QUERY_ANY:
		    if (!mui->mpd || !mpd_search_add_any_tag_constraint(mui->mpd, MPD_OPERATOR_DEFAULT, MSQueryText->value()))
		    {
			 printf("mpd_search_add_any_tag_constraint() failed\n");
			 MPDEnd();
			 return;
		    }
		    break;

	       case QUERY_FILE: // URI
		    if (!mui->mpd || !mpd_search_add_uri_constraint(mui->mpd, MPD_OPERATOR_DEFAULT, MSQueryText->value()))
		    {
			 printf("mpd_search_add_uri_constraint() failed\n");
			 MPDEnd();
			 return;
		    }
		    break;

	       case QUERY_MAX:
		    break;
	       }

	       if (!mui->mpd || !mpd_search_commit(mui->mpd))
	       {
		    printf("mpd_search_commit() failed\n");
		    MPDEnd();
		    return;
	       }

	       MPDCheck();

	       // Clear browsers
	       MSBrowser->clear();
	       ClearBrowser(PLBrowser);

	       if (!mui->mpd || !mpd_send_list_queue_range_meta(mui->mpd, 0, (unsigned int)-1))
	       {
		    printf("mpd_send_list_queue_range_meta() failed\n");
		    MPDEnd();
		    return;
	       }

	  }

	  // Traverse the results
	  while ((entity = mpd_recv_entity(mui->mpd)) != NULL)
	  {
	       // We are leaving
	       if (mui->isRunning == false)
	       {
		    return;
	       }

	       if (category == QUERY_MAX)
	       {
		    const struct mpd_song *song;
		    const char            *uri = NULL;

		    // Build the Browaer entries
		    switch (mpd_entity_get_type(entity))
		    {
		    case MPD_ENTITY_TYPE_SONG:
			 song = mpd_entity_get_song(entity);
			 uri = mpd_song_get_uri(song);

			 // Push the URI to the AllSongs playlist
			 allSongs.push_back(strdup(uri));
			 break;

			 // Ignore these types
		    case MPD_ENTITY_TYPE_UNKNOWN:
		    case MPD_ENTITY_TYPE_DIRECTORY:
		    case MPD_ENTITY_TYPE_PLAYLIST:
			 // ignore
			 break;
		    }
	       }
	       else // ! QUERY_MAX
	       {
		    const struct mpd_song *song;
		    const char            *uri = NULL;
		    const char            *artist = NULL;
		    const char            *album = NULL;
		    const char            *title = NULL;
		    char                   buf[64];

		    // Build the Browaer entries
		    switch (mpd_entity_get_type(entity))
		    {
		    case MPD_ENTITY_TYPE_SONG:
			 song = mpd_entity_get_song(entity);

			 artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
			 album = mpd_song_get_tag(song, MPD_TAG_ALBUM, 0);
			 title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
			 uri = mpd_song_get_uri(song);

			 if (artist && title)
			 {
			      snprintf(buf, sizeof(buf) - 1, "@r%.20s \t %.31s", artist, title);
			 }
			 else
			 {
			      // No tag are available, just put the URI
			      snprintf(buf, sizeof(buf) - 1, "@c%.30s", uri);
			 }

			 PLBrowser->add(buf);

			 // Build the query browser entries
			 switch(mui->mpdQueryCategory)
			 {
			 case QUERY_ARTIST:
			      if (artist)
			      {
				   snprintf(buf, sizeof(buf) - 1, "@c%.50s", artist);
			      }
			      else
			      {
				   snprintf(buf, sizeof(buf) - 1, "@c%.50s", uri);
			      }
			      MSBrowser->add(buf);
			      break;

			 case QUERY_ALBUM:
			      if (album)
			      {
				   snprintf(buf, sizeof(buf) - 1, "@c%.50s", album);
			      }
			      else
			      {
				   snprintf(buf, sizeof(buf) - 1, "@c%.50s", uri);
			      }
			      MSBrowser->add(buf);
			      break;

			 case QUERY_TITLE:
			      if (title)
			      {
				   snprintf(buf, sizeof(buf) - 1, "@c%.50s", title);
			      }
			      else
			      {
				   snprintf(buf, sizeof(buf) - 1, "@c%.50s", uri);
			      }
			      MSBrowser->add(buf);
			      break;

			 case QUERY_ANY:
			      if (artist && title)
			      {
				   snprintf(buf, sizeof(buf) - 1, "@r%.20s \t %.31s", artist, title);
			      }
			      else
			      {
				   // No tag are available, just put the URI
				   snprintf(buf, sizeof(buf) - 1, "@c%.50s", uri);
			      }
			      MSBrowser->add(buf);
			      break;

			 case QUERY_FILE:
			      snprintf(buf, sizeof(buf) - 1, "@c%.50s", uri);
			      MSBrowser->add(buf);
			      break;

			 case QUERY_MAX:
			      break;
			 }
			 break;

			 // Ignore these types
		    case MPD_ENTITY_TYPE_UNKNOWN:
		    case MPD_ENTITY_TYPE_DIRECTORY:
		    case MPD_ENTITY_TYPE_PLAYLIST:
			 // ignore
			 break;
		    }
	       } // ! QUERY_MAX

	       mpd_entity_free(entity);
	  }

	  struct mpd_status *status;
	  if (!mui->mpd || !mpd_send_status(mui->mpd))
	  {
	       printf("mpd_send_list_queue_range_meta() failed\n");
	       MPDEnd();
	       return;
	  }

	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	       return;
	  }

	  if (mpd_status_get_error(status) != NULL)
	  {
	       printf("MPD status error: %s\n", mpd_status_get_error(status));
	       MPDEnd();
	       mpd_status_free(status);
	       return;
	  }

	  if (category == QUERY_MAX)
	  {
	       vector<char *>::iterator it = allSongs.begin();

	       if (mui->isRunning == false)
	       {
		    mpd_status_free(status);
		    return;
	       }

	       if (!mpd_run_playlist_clear(mui->mpd, playlistName))
	       {
		    // If the playlist doesn't exist, mpd_run_playlist_clear() fails.
		    mpd_run_clearerror(mui->mpd);
		    //printf("mpd_run_playlist_clear() failed\n");
		    MPDEnd(false);
		    MPDBegin();
	       }

	       while(it != allSongs.end())
	       {
		    // We are leaving
		    if (mui->isRunning == false)
		    {
			 mpd_status_free(status);
			 return;
		    }

		    if (mui->mpd)
		    {
			 // We need to go to the end of the vector, to release memory
			 if (!mpd_run_playlist_add(mui->mpd, playlistName, (*it)))
			 {
			      printf("mpd_run_playlist_add() failed\n");
			      MPDEnd();
			 }
		    }

		    free((*it));
		    it++;
	       }

	       allSongs.clear();
	  }

	  if (mpd_status_get_queue_length(status) > 0)
	  {
	       // Remove destination playlst
	       if (!mpd_run_rm(mui->mpd, playlistName))
	       {
		    mpd_run_clearerror(mui->mpd);
		    //printf("mpd_run_rm() failed\n");
		    MPDEnd(false);
		    MPDBegin();
	       }

	       // Now save the new search results
	       if (!mui->mpd || !mpd_run_save(mui->mpd, playlistNameNew))
	       {
		    printf("mpd_run_save() failed\n");
		    MPDEnd();
		    mpd_status_free(status);
		    return;
	       }

	       // Now rename <playlist>.new to <playlist>
	       if (!mpd_run_rename(mui->mpd, playlistNameNew, playlistName))
	       {
		    printf("mpd_run_rename() failed\n");
		    MPDEnd();
		    mpd_status_free(status);
		    return;
	       }

	       mui->plListMode = PLMODE_TRACKS;
	  }

	  // switch PLBrowser to playlists list
	  MPDEnd();
	  mpd_status_free(status);
     }
}

//
// Handles click events on the keyboard keys (music search tab)
//
static void KeyboardChar_CB(Fl_Widget *w, void *data)
{
     Fl_Button     *b = (Fl_Button*)w;
     KeyboardKey_t *key = (KeyboardKey_t *)data;

     if (key == NULL)
     {
	  return;
     }

     switch (key->key)
     {
     	case KEY_DEL:
	     if (MSQueryText->mark() != MSQueryText->position())
	     {
		  MSQueryText->cut();
	     }
	     else
	     {
		  MSQueryText->cut(-1);
	     }
	     break;

     	case KEY_SPACE:
	     MSQueryText->replace(MSQueryText->position(), MSQueryText->mark(), " ", 1);
	     break;

     	case KEY_SEEK:
	     QueryMPDPlaylist(mui->mpdQueryCategory);
	break;

     	case KEY_MAX_KEYS:
	     break;

     	default:  /* Other normal keys */
	     MSQueryText->replace(MSQueryText->position(), MSQueryText->mark(), b->label(), 1);
	     break;
     }
}

//
// Display current configuration
//
static void DisplayConfig()
{
     printf("   - Script Directory :            '%s'\n", ScriptsDir.c_str());
     printf("   - Monkey Server Name :           %s\n", mui->monkeyServerName.c_str());
     printf("   - Monkey Server Port :           %d\n", mui->monkeyServerPort);
     printf("   - GPIOD Server Port :            %d\n", mui->gpiodServerPort);
     printf("   - GPS Server Name :              %s\n", mui->gpsServerName.c_str());
     printf("   - GPS Server Port :              %s\n", mui->gpsServerPort.c_str());
     printf("   - MPD Server Name :              %s\n", mui->mpdServerName.c_str());
     printf("   - MPD Server Port :              %d\n", mui->mpdServerPort);
}

//
// Display opt arg list.
//
static void DisplayUsage()
{
     printf("Usage:\n");
     printf("\n");
     printf("        -h,         --help                         display this help text.\n");
     printf("                    --config                       display current configuration then exit.\n");
     printf("        -N        , --network-default              reset network configuration to the default values.\n");
     printf("        -W,         --windowed                     starts in windowed mode instead of fullscreen.\n");
     printf("        -s <value>, --monkey-server=<value>        define the Monkey server name (default is \"%s\").\n", DEFAULT_SERVER_NAME);
     printf("        -p <value>, --monkey-port=<value>          define the Monkey server port (default is %d).\n", DEFAULT_SERVER_PORT);
     printf("                    --gpiod-port=<value>           define the GPIOD server port (default is %d).\n", GPIOD_DEFAULT_PORT);
     printf("                    --no-gpiod                     do not use GPIOD server.\n");
     printf("                    --gps-server=<value>           define the GPS server name (default is \"localhost\").\n");
     printf("                    --gps-port=<value>             define the GPS server port (default is %s).\n", DEFAULT_GPSD_PORT);
     printf("                    --position-default             reset coordinates to default values (Lat: %g Long: %g)\n", DEFAULT_POSITION_LATITUDE, DEFAULT_POSITION_LONGITUDE);
     printf("                    --latitude<=value>             set latitude coodinate.\n");
     printf("                    --longitude<=value>            set longitude coodinate.\n");
     printf("                    --mpd-server=<value>           define the MPD server name (default is \"localhost\").\n");
     printf("                    --mpd-port=<value>             define the MPD server port (default is 6600).\n");
     printf("\n");

}

//
// This is called when closing the main window (close button or <Alt>-F4)
// It saves the config and call the FlTk normal callback.
//
static void MainWindowExit_CB(Fl_Widget *w, void *data)
{
     if (mui->isReady)
     {
	  mui->SaveConfig();
     }
     Fl::default_atclose(win, NULL);
}

//
// Postponed end of the program (needed, as everything is async, with socket connections)
//
static void Suicide_Task(void *data)
{
     if (mui->isReady && win)
     {
	  delete win;
	  win = NULL;
     }
     else
     {
	  Fl::repeat_timeout(1.0, Suicide_Task);
     }
}

//
// Display message from the signal handler function (see below)
//
static ssize_t sigErr(const char *fmt, ...)
{
     char buffer[2048] = {0};
     va_list args;

     va_start(args, fmt);
     vsnprintf(buffer, (sizeof(buffer) - 2), fmt, args);
     va_end(args);

     // Each line sent is '\n' terminated
     if((buffer[strlen(buffer)] == '\0') && (buffer[strlen(buffer) - 1] != '\n'))
     {
	  strlcat(buffer, "\n", sizeof(buffer));
     }

     return write(STDERR_FILENO, buffer, strlen(buffer));
}

//
// Handles system signals
//
static void SignalHandler (int sig)
{
     switch(sig)
     {
     	case SIGQUIT:
	case SIGINT:
     	case SIGTERM:
	     sigErr("\nSIGQUIT | SIGINT | SIGTERM signal received, will exit soon...");
	     mui->isRunning = false;
	     if (mui->isReady)
	     {
		  SendToGPIOD(GPIOD_BYE);
		  mui->SaveConfig();
	     }
	     Fl::repeat_timeout(1.0, Suicide_Task);
	     break;

     	case SIGUSR1:
	     sigErr("%s", "   - SIGUSR1 - doing nothing");
	     break;

     	case SIGUSR2:
	     sigErr("%s", "   - SIGUSR2 - doing nothing");
	     break;

     	case SIGHUP:
	     sigErr("%s", "   - SIGHUP - doing nothing");
	     break;

     }
}

//
// Main function (obviously :-D )
//
int main(int argc, char **argv)
{
     struct sigaction   action;
     int                c = '?';
     int                option_index = 0;
     bool               startFullscreen = true;

     setlocale(LC_ALL, "");
     setlocale(LC_NUMERIC, "C");
     setlocale(LC_CTYPE, "C");
     setlocale(LC_MONETARY, "C");

     mui = new MediaUI;

     ScriptsDir             = string(getenv("HOME")) + "/flhu/headunit/media_ui/scripts/";

     mui->isRunning = true;

#ifdef DEBUG
     printf("\nMedia UI v%d.%d.%d - DEBUG\n", VERSION_MAJOR, VERSION_MINOR, VERSION_SUB);
#else
     printf("\nMedia UI v%d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_SUB);
#endif
     
     action.sa_handler = SignalHandler;
     sigemptyset(&(action.sa_mask));
     action.sa_flags = 0;
     if(sigaction(SIGINT, &action, NULL) != 0)
     {
	  fprintf(stderr, "sigaction(SIGINT) failed: %s\n", strerror(errno));
     }

     action.sa_handler = SignalHandler;
     sigemptyset(&(action.sa_mask));
     action.sa_flags = 0;
     if(sigaction(SIGTERM, &action, NULL) != 0)
     {
	  fprintf(stderr, "sigaction(SIGTERM) failed: %s\n", strerror(errno));
     }

     action.sa_handler = SignalHandler;
     sigemptyset(&(action.sa_mask));
     action.sa_flags = 0;
     if(sigaction(SIGQUIT, &action, NULL) != 0) {
	  fprintf(stderr, "sigaction(SIGQUIT) failed: %s\n", strerror(errno));
     }

     action.sa_handler = SignalHandler;
     sigemptyset(&(action.sa_mask));
     action.sa_flags = 0;
     if(sigaction(SIGUSR1, &action, NULL) != 0) {
	  fprintf(stderr, "sigaction(SIGUSR1) failed: %s\n", strerror(errno));
     }

     action.sa_handler = SignalHandler;
     sigemptyset(&(action.sa_mask));
     action.sa_flags = 0;
     if(sigaction(SIGUSR2, &action, NULL) != 0) {
	  fprintf(stderr, "sigaction(SIGUSR2) failed: %s\n", strerror(errno));
     }

     action.sa_handler = SignalHandler;
     sigemptyset(&(action.sa_mask));
     action.sa_flags = 0;
     if(sigaction(SIGHUP, &action, NULL) != 0) {
	  fprintf(stderr, "sigaction(SIGHUP) failed: %s\n", strerror(errno));
     }

     // Ignore SIGPIPE
     signal(SIGPIPE, SIG_IGN);

     opterr = 0;
     while((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != EOF)
     {
	  switch(c)
	  {
	  case 'h': /* Display usage */
	  case '?':
	       DisplayUsage();
	       return EXIT_SUCCESS;
	       break;

	  case 'N':
	       mui->monkeyServerName = DEFAULT_SERVER_NAME;
	       mui->monkeyServerPort = DEFAULT_SERVER_PORT;
	       mui->gpiodServerPort = GPIOD_DEFAULT_PORT;
	       mui->mpdServerName = "localhost";
	       mui->mpdServerPort = 6600;
	       mui->gpsServerName = "localhost";
	       mui->gpsServerPort = DEFAULT_GPSD_PORT;
	       break;

	  case 's': // Server Name
	       if(optarg != NULL)
	       {
		    mui->monkeyServerName = optarg;
	       }
	       break;

	  case 'p': // Server port
	       if(optarg != NULL)
	       {
		    mui->monkeyServerPort = atoi(optarg);

		    if (mui->monkeyServerPort < 1024)
		    {
			 sockErr("   ERROR: Monkey Server Port : %d is invalid\n\n", mui->monkeyServerPort);
			 return EXIT_FAILURE;
		    }
	       }
	       break;

	  case OPTION_GPS_SERVER: // GPS Server Name
	       if(optarg != NULL)
	       {
		    mui->gpsServerName = optarg;
	       }
	       break;


	  case OPTION_GPS_PORT: // GPS Server Port
	       if(optarg != NULL)
	       {
		    mui->gpsServerPort = optarg;
	       }
	       break;

	  case OPTION_MPD_SERVER:
	       if(optarg != NULL)
	       {
		    mui->mpdServerName = optarg;
	       }
	       break;

	  case OPTION_MPD_PORT:
	       if(optarg != NULL)
	       {
		    mui->mpdServerPort = atoi(optarg);
	       }
	       break;

	  case OPTION_GPIOD_PORT:
	       if(optarg != NULL)
	       {
		    mui->gpiodServerPort = atoi(optarg);

		    if (mui->gpiodServerPort < 1024)
		    {
			 sockErr("   ERROR: GPIOD Server Port : %d is invalid\n\n", mui->gpiodServerPort);
			 return EXIT_FAILURE;
		    }
	       }
	       break;

	  case OPTION_NO_GPIOD:
	       mui->noGPIOD = true;
	       break;

	  case OPTION_CONFIG:
	       printf("\n* Current configuration:\n");
	       DisplayConfig();
	       printf("\n");
	       return EXIT_SUCCESS;
	       break;

	  case 'W': // Windowed mode
	       startFullscreen = false;
	       break;

	  case OPTION_POS_DEFAULT:
	       mui->gpsLatitude = DEFAULT_POSITION_LATITUDE;
	       mui->gpsLongitude = DEFAULT_POSITION_LONGITUDE;
	       break;
	       
	  case OPTION_POS_LAT:
	       if(optarg != NULL)
	       {
		    mui->gpsLatitude = strtod(optarg, &optarg);
	       }
	       break;
	       
	  case OPTION_POS_LONG:
	       if(optarg != NULL)
	       {
		    mui->gpsLongitude = strtod(optarg, &optarg);
	       }
	       break;

	  default:
	       DisplayUsage();
	       fprintf(stderr, "invalid argument %d => exit\n", c);
	       return EXIT_FAILURE;
	  }
     }

     DisplayConfig();

     MPDBegin();
     if (mui->mpd)
     {
	  printf("   - MPD Protocol :                 ");
	  for(uint8_t i = 0; i < 3; i++)
	  {
	       printf("%i", mpd_connection_get_server_version(mui->mpd)[i]);
	       if (i < 2)
	       {
		    printf(".");
	       }
	  }
	  printf("\n");
     }
     printf("\n");

     if (system(NULL) == 0)
     {
	  fprintf(stderr, "There is no shell on this system, bailing out!\n");
	  return EXIT_FAILURE;
     }

     // Change the current working directory
     if ((chdir("/tmp")) < 0)
     {
	  perror("chdir");
	  return EXIT_FAILURE;
     }

     Fl::set_font(MAC_FONT, "Chicago");

     // Restore Random and Crossfade settings
     MPDBegin();
     if (mui->mpd)
     {
	  if (!mui->mpd || !mpd_send_random(mui->mpd, mui->playerRandom))
	  {
	       printf("mpd_send_random() failed.\n");
	  }
	  MPDCheck();

	  if (!mui->mpd || !mpd_send_crossfade(mui->mpd, mui->playerCrossfade))
	  {
	       printf("mpd_send_crossfade() failed.\n");
	  }
	  MPDCheck();
     }

     if (mui->noGPIOD == false)
     {
	  if (pthread_create(&mui->gpiodThread, NULL, ThreadGPIOD, mui) != 0)
	  {
	       perror("pthread_create");
	       mui->isRunning = false;
	       return EXIT_FAILURE;
	  }

	  if (pthread_create(&mui->gpiodPingThread, NULL, ThreadPingGPIOD, NULL) != 0)
	  {
	       perror("pthread_create");
	       mui->isRunning = false;
	       return EXIT_FAILURE;
	  }
     }

     if (pthread_create(&mui->monkeyThread, NULL, ThreadMONKEY, mui) != 0)
     {
     	  perror("pthread_create");
     	  mui->isRunning = false;
     	  return EXIT_FAILURE;
     }


     Fl::option(Fl::OPTION_VISIBLE_FOCUS, false);

     win = new Fl_Double_Window( 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, "Media UI" );
     win->callback(MainWindowExit_CB, NULL);
     win->begin();
     win->color(FL_BLACK); // win->color(FL_BLACK, mui->foregroundColor); /* color(Fl_Color bg, Fl_Color sel); */
     win->color2(FL_BLACK);
     win->selection_color(SELECTION_COLOR);
     // Icon and Class
     Fl_Pixmap appIcon(flhu_icon_xpm);
     Fl_RGB_Image appIconImage(&appIcon, Fl_Color(0));
     win->icon(&appIconImage);
     win->xclass("Media UI");

//     strengthIcon = new Fl_Button(5, 4, 35, 35); // to display applelogo for strength of current station
     strengthIcon = new Fl_Button(ELB, ETB, 35, 35); // to display applelogo for strength of current station
     strengthIcon->color(FL_BLACK);;
     if (LoadStrengthIconImages() == false)
     {
	  printf("Error while loading applelogo strength images.\n");
     }
     if (mui->strengthPMGs.size())
     {
	  strengthIcon->image(mui->strengthPMGs[(mui->strengthPMGs.size() - 1)]);
     }
     strengthIcon->callback(StrengthIcon_CB); // when pressing the apple strengthicon, the computer is rebooted.

//     playStatusInfo = new Fl_Box((strengthIcon->x() + strengthIcon->w() + 5), 2, 200, 18, "No Status");
     playStatusInfo = new Fl_Box((strengthIcon->x() + strengthIcon->w() + DBW), ETB, floor(win->w()/4), IBH, "No Status");
     playStatusInfo->labelfont(MAC_FONT);
     playStatusInfo->labelcolor(mui->foregroundColor);
     playStatusInfo->labelsize(IBLS);
     playStatusInfo->box(FL_NO_BOX);
     playStatusInfo->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);

     ensembleInfo = new Fl_Scrolling_Box((playStatusInfo->x() + playStatusInfo->w() + (5*DBW)), ETB, floor(win->w()/5), IBH, "");
     ensembleInfo->labelfont(MAC_FONT);
     ensembleInfo->labelcolor(mui->foregroundColor);
     ensembleInfo->labelsize(IBLS);
     ensembleInfo->box(FL_NO_BOX);
     ensembleInfo->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);

// *** Calculating widgets position from right border ***


     gpsOpenDevice();



//     speedInfo = new Fl_Box(675, 1, 124, 40, "-.-");
     speedInfo = new Fl_Box(floor(win->w()-SIBW), ETB, SIBW, BIBH, "-.-");
     speedInfo->labelfont(MAC_FONT);
     speedInfo->box(FL_NO_BOX);
     speedInfo->labelsize(BIBLS);
     speedInfo->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);
     
     navigationIcon = new Fl_Button((speedInfo->x() - strengthIcon->w() - DBW), ETB, strengthIcon->w(), strengthIcon->h());
     navigationIcon->color(FL_BLACK);
     navigationIcon->image(new Fl_PNG_Image(string(getenv("HOME") + FLHUDIR + "/icons/navigation_30x30.png").c_str()));;
     navigationIcon->callback(Navigation_CB);

//     clockInfo = new Fl_Box(540, 0, 99, 44, "88:88");
     clockInfo = new Fl_Box( (navigationIcon->x() - CIBW - DBW), ETB, CIBW, BIBH, "88:88");
     clockInfo->labelfont(MAC_FONT);
     clockInfo->labelcolor(mui->foregroundColor);
     clockInfo->labelsize(BIBLS);
     clockInfo->box(FL_NO_BOX);
     clockInfo->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);
     
//     rateInfo = new Fl_Box((ensembleInfo->x() + ensembleInfo->w() + 20), 2, 103, 18, "");
     rateInfo = new Fl_Box((clockInfo->x() - RIBW - DBW), ETB, RIBW, IBH, "");
     rateInfo->labelfont(MAC_FONT);
     rateInfo->labelcolor(mui->foregroundColor);
     rateInfo->labelsize(IBLS);
     rateInfo->box(FL_NO_BOX);
     rateInfo->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);     
     


     //-----------------------------------------
     currentStreamInfo = new Fl_Scrolling_Box(ELB, strengthIcon->h()+DBW, (playStatusInfo->y() + playStatusInfo->w() + DBW), IBH);
     currentStreamInfo->labelfont(MAC_FONT);
     currentStreamInfo->labelcolor(mui->foregroundColor);
     currentStreamInfo->labelsize(IBLS);
     currentStreamInfo->box(FL_NO_BOX);
     currentStreamInfo->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);


     tabs = new Fl_Tabs(0, currentStreamInfo->y(), DISPLAY_WIDTH, DISPLAY_HEIGHT - currentStreamInfo->y()  );
//     tabs = new Fl_Tabs(ELB, currentStreamInfo->y()+currentStreamInfo->h()+DBW, DISPLAY_WIDTH - ELB - ERB, DISPLAY_HEIGHT - ETB - EBB);
     tabs->labelfont(MAC_FONT);
     tabs->labelcolor(mui->foregroundColor);
     tabs->labelsize(BIBLS);
     tabs->color2(FL_BLACK);
     {
	  // DAB tab
//	  dab = new Fl_Group(3+3,
//			     floor(DISPLAY_HEIGHT/12)+floor(DISPLAY_HEIGHT/12)+3,
//			     DISPLAY_WIDTH-(5+5),
//			     DISPLAY_HEIGHT-(floor(DISPLAY_HEIGHT/12)+floor(DISPLAY_HEIGHT/12)+5),
//			     "   DAB  ");
	  dab = new Fl_Group(DBW,
	                     tabs->y() + TLS,
	                     tabs->w() - (2*DBW),
	                     tabs->h() - DBW,
			     "   DAB  ");
	  dab->labelfont(MAC_FONT);
	  dab->color(FL_BLACK);
	  dab->selection_color(SELECTION_COLOR);
	  dab->labelcolor(mui->foregroundColor);
	  dab->labelsize(TLS);
	  {
	       DABBrowser = new Fl_Browser(
	                                   tabs->x() + DBW,
					   tabs->y() + TLS + DBW,
					   tabs->w() - (2 * DBW),
					   tabs->h() - TLS - (2 * DBW));
	       {
		    DABBrowser->type(FL_HOLD_BROWSER);
		    DABBrowser->labelfont(MAC_FONT);
		    DABBrowser->textfont(MAC_FONT);
		    DABBrowser->column_widths(DABTabWidths);
		    DABBrowser->column_char('\t');

		    DABBrowser->color(FL_BLACK);
		    DABBrowser->selection_color(SELECTION_COLOR);
		    DABBrowser->labelcolor(mui->foregroundColor);
		    DABBrowser->textsize(BTS);
		    DABBrowser->textcolor(mui->foregroundColor);
		    DABBrowser->scrollbar_width(SBW);

		    // Tune V/H scrollbars
		    DABBrowser->scrollbar.box(FL_THIN_DOWN_BOX);
		    DABBrowser->scrollbar.slider(FL_PLASTIC_UP_BOX);
		    DABBrowser->hscrollbar.box(FL_THIN_DOWN_BOX);
		    DABBrowser->scrollbar.slider(FL_PLASTIC_UP_BOX);

		    DABBrowser->has_scrollbar(Fl_Browser_::VERTICAL);
		    ClearBrowser(DABBrowser);
		    DABBrowser->callback(DABBrowser_CB);
		    dab->resizable(DABBrowser);
	       }
	       DABBrowser->end();
	  }
	  dab->end();
     }
     {
	  // FM tab
	  fm = new Fl_Group(DBW,
	                     tabs->y() + TLS,
	                     tabs->w() - (2*DBW),
	                     tabs->h() - DBW,
			     "   FM  ");	  
//	  fm = new Fl_Group(3+3,
//			    floor(DISPLAY_HEIGHT / 12) + floor(DISPLAY_HEIGHT / 12) + 3,
//			    DISPLAY_WIDTH - (5 + 5),
//			    DISPLAY_HEIGHT -(floor(DISPLAY_HEIGHT / 12) + floor(DISPLAY_HEIGHT / 12) + 5),
//			    "   FM   ");
	  fm->labelfont(MAC_FONT);
	  fm->color(FL_BLACK);
	  fm->selection_color(SELECTION_COLOR);
	  fm->labelcolor(mui->foregroundColor);
	  fm->labelsize(TLS);
	  {
//	       FMBrowser = new Fl_Browser(dab->x(),
//					  dab->y() + 3,
//					  dab->w() - 2,
//					  dab->h() - 5);
	       FMBrowser = new Fl_Browser(
	                                   tabs->x() + DBW,
					   tabs->y() + TLS + DBW,
					   tabs->w() - (2 * DBW),
					   tabs->h() - TLS - (2 * DBW));
	       {
		    FMBrowser->type(FL_HOLD_BROWSER);
		    FMBrowser->column_widths(FMTabWidths);
		    FMBrowser->column_char('\t');

		    FMBrowser->color(FL_BLACK);
		    FMBrowser->selection_color(SELECTION_COLOR);
		    FMBrowser->labelcolor(mui->foregroundColor);
		    FMBrowser->textsize(BTS);
		    FMBrowser->labelfont(MAC_FONT);
		    FMBrowser->textfont(MAC_FONT);;
		    FMBrowser->textcolor(mui->foregroundColor);
		    FMBrowser->scrollbar_width(SBW);

		    // Tune V/H scrollbars
		    FMBrowser->scrollbar.box(FL_THIN_DOWN_BOX);
		    FMBrowser->scrollbar.slider(FL_PLASTIC_UP_BOX);
		    FMBrowser->hscrollbar.box(FL_THIN_DOWN_BOX);
		    FMBrowser->scrollbar.slider(FL_PLASTIC_UP_BOX);

		    FMBrowser->has_scrollbar(Fl_Browser_::VERTICAL);

		    ClearBrowser(FMBrowser);
		    FMBrowser->callback(FMBrowser_CB);
		    fm->resizable(FMBrowser);
	       }
	       FMBrowser->end();
	  }
	  fm->end();
     }
     {
	  // Phone tab
	  Phone = new Fl_Group(DBW,
	                     tabs->y() + TLS,
	                     tabs->w() - (2*DBW),
	                     tabs->h() - DBW,
			     " Phone ");	  
//	  Phone = new Fl_Group(3+3,
//			       floor(DISPLAY_HEIGHT / 12) + floor(DISPLAY_HEIGHT / 12) + 3,
//			       DISPLAY_WIDTH - (5 + 5),
//			       DISPLAY_HEIGHT - (floor(DISPLAY_HEIGHT / 12) + floor(DISPLAY_HEIGHT / 12) + 5),
//			       " Phone ");
	  Phone->labelfont(MAC_FONT);
	  Phone->color(FL_BLACK);
	  Phone->selection_color(SELECTION_COLOR);
	  Phone->labelcolor(mui->foregroundColor);
	  Phone->labelsize(TLS);
	  {
	       int sbb = 19; // space between buttons

	       // Presets
	       for (uint8_t i = EQ_PRESET_OFF; i < EQ_MAX_PRESETS; i++)
	       {
		    EQPresetKeys[i] = new Fl_Button(sbb,
						     floor(DISPLAY_HEIGHT / 12) + ((1 + i) * (floor(DISPLAY_HEIGHT / 8))),
						     floor(DISPLAY_WIDTH / 10),
						     floor(DISPLAY_HEIGHT / 9),
						     EQPresets[i].label);
		    EQPresetKeys[i]->box(FL_ROUND_UP_BOX);
		    EQPresetKeys[i]->labelfont(MAC_FONT);
		    EQPresetKeys[i]->color(FL_BLACK);
		    EQPresetKeys[i]->labelcolor(mui->foregroundColor);
		    EQPresetKeys[i]->labelsize(IBLS);
		    EQPresetKeys[i]->callback(EQPreset_CB, (void *)&EQPresets[i]);
	       }

	       // Bluetooth Keys
	       int bt_mac_width = 170;
	       int bt_mac_heigth = 30;

	       MACQueryText = new Fl_Input((sbb * 2) + floor(DISPLAY_WIDTH / 10),
					   floor(DISPLAY_HEIGHT / 12) + (1 * (floor(DISPLAY_HEIGHT / 8))),
					   bt_mac_width,
					   bt_mac_heigth);
	       MACQueryText->labelfont(MAC_FONT);
	       MACQueryText->textfont(MAC_FONT);
	       MACQueryText->labelcolor(mui->foregroundColor);
	       MACQueryText->textsize(16);
	       MACQueryText->labelsize(16);
	       MACQueryText->textcolor(FL_BLACK);
	       MACQueryText->maximum_size(17); /* MAC address maximum length including colon */

	       MACQueryText->value(mui->bluetoothMAC.c_str());

	       {
		    int32_t curCol = -1;
		    uint32_t btXPos = 0;
		    uint32_t btYPos = 0;

		    for (uint8_t i = BLUETOOTH_KEY_1; i < BLUETOOTH_MAX_KEYS; i++)
		    {
			 if (curCol != Bluetooth_Keys[i].col)
			 {
			      curCol = Bluetooth_Keys[i].col;
			      btXPos++;
			      btYPos = 0;
			 }

			 BluetoothKeys[i] = new Fl_Button((sbb * (3 + Bluetooth_Keys[i].col)) + (floor(DISPLAY_WIDTH / 10) * btXPos) + bt_mac_width,
							   floor(DISPLAY_HEIGHT / 12) + ((1 + btYPos) * (floor(DISPLAY_HEIGHT / 8))),
							   floor(DISPLAY_WIDTH / 10),
							   floor(DISPLAY_HEIGHT / 9),
							   Bluetooth_Keys[i].label);
			 BluetoothKeys[i]->box(FL_RSHADOW_BOX);
			 BluetoothKeys[i]->labelfont(MAC_FONT);
			 BluetoothKeys[i]->color(FL_BLACK);
			 BluetoothKeys[i]->labelcolor(mui->foregroundColor);
			 BluetoothKeys[i]->labelsize(IBLS);
			 BluetoothKeys[i]->callback(BluetoothKey_CB, (void *)&Bluetooth_Keys[i]);

			 btYPos++;
		    }

		    BluetoothKeys[BLUETOOTH_KEY_COLON]->box(FL_PLASTIC_UP_BOX);
		    BluetoothKeys[BLUETOOTH_KEY_DEL]->box(FL_PLASTIC_UP_BOX);
		    BluetoothKeys[BLUETOOTH_KEY_A]->box(FL_PLASTIC_UP_BOX);
		    BluetoothKeys[BLUETOOTH_KEY_B]->box(FL_PLASTIC_UP_BOX);
		    BluetoothKeys[BLUETOOTH_KEY_C]->box(FL_PLASTIC_UP_BOX);
		    BluetoothKeys[BLUETOOTH_KEY_D]->box(FL_PLASTIC_UP_BOX);
		    BluetoothKeys[BLUETOOTH_KEY_E]->box(FL_PLASTIC_UP_BOX);
		    BluetoothKeys[BLUETOOTH_KEY_F]->box(FL_PLASTIC_UP_BOX);
	       }


	       // DABImage = new Fl_PNG_Image(string(HOMEDIR + FLHUDIR + "/dab_plus.png").c_str());
	       // dabimagebox = new Fl_Box(   143,
	       // 				   93,
	       // 				   647,
	       // 				   499);
	       // // use 'convert -resize 573x499 /tmp/dabimage/* /tmp/dabimage.png' */
	       // dabimagebox->box(FL_NO_BOX);
	       // dabimagebox->color(FL_RED);
	       // dabimagebox->labelcolor(FL_RED);
	       // dabimagebox->image(DABImage);

	  }
	  Phone->end();
     }
     {
	  // PL tab
	  pl = new Fl_Group(DBW,
	                     tabs->y() + TLS,
	                     tabs->w() - (2*DBW),
	                     tabs->h() - DBW,
			     " Playlists ");	  
//	  pl = new Fl_Group(3+3,
//			    floor(DISPLAY_HEIGHT / 12) + floor(DISPLAY_HEIGHT / 12) + 3,
//			    DISPLAY_WIDTH - (5 + 5),
//			    DISPLAY_HEIGHT -(floor(DISPLAY_HEIGHT / 12) + floor(DISPLAY_HEIGHT / 12) + 5),
//			    " Playlists ");

	  pl->labelfont(MAC_FONT);
	  pl->color(FL_BLACK);
	  pl->selection_color(SELECTION_COLOR);
	  pl->labelcolor(mui->foregroundColor);
	  Fl::add_handler(EventHandler);
	  Fl::event_dispatch(EventDispatcher);
	  pl->labelsize(TLS);
	  {
//	       PLBrowser = new Fl_Browser(dab->x(),
//					  dab->y() + 3,
//					  dab->w() - 2,
//					  dab->h() - 40 - 6);
	       PLBrowser = new Fl_Browser(
	                                   tabs->x() + DBW,
					   tabs->y() + TLS + DBW,
					   tabs->w() - (2 * DBW),
					   tabs->h() - TLS - (3 * DBW) - SBW);					  
	       PLBrowser->type(FL_HOLD_BROWSER);
	       PLBrowser->column_widths(PLTabWidths);
	       PLBrowser->column_char('\t');

	       PLBrowser->labelfont(MAC_FONT);
	       PLBrowser->textfont(MAC_FONT);

	       PLBrowser->color(FL_BLACK);
	       PLBrowser->selection_color(SELECTION_COLOR);
	       PLBrowser->labelcolor(mui->foregroundColor);
	       PLBrowser->textsize(BTS);
	       PLBrowser->textcolor(mui->foregroundColor);
	       PLBrowser->scrollbar_width(SBW);

	       // Tune V/H scrollbars
	       PLBrowser->scrollbar.box(FL_THIN_DOWN_BOX);
	       PLBrowser->scrollbar.slider(FL_PLASTIC_UP_BOX);
	       PLBrowser->hscrollbar.box(FL_THIN_DOWN_BOX);
	       PLBrowser->scrollbar.slider(FL_PLASTIC_UP_BOX);

	       ClearBrowser(PLBrowser);
	       PLBrowser->callback(PLBrowser_CB, NULL);
	       pl->resizable(PLBrowser);

	       for (uint8_t i = PLAYER_KEY_RANDOM; i < PLAYER_KEY_MAX; i++)
	       {
		    if (i == 0)
		    {
			 PlayerKeys[i] = new Fl_Light_Button(SPB + (PBW * i),
							      (PLBrowser->y() + PLBrowser->h()) + DBW,// DISPLAY_HEIGHT - (floor(DISPLAY_HEIGHT / 12) + 3 + 3),
							      PBW,
							      SBH,
							      Player_Keys_Labels[i]);
			 PlayerKeys[i]->value(mui->playerRandom);
		    }
		    else
		    {
			 PlayerKeys[i] = new Fl_Button(PlayerKeys[i - 1]->x() + PlayerKeys[i - 1]->w() + DBW,
							(PLBrowser->y() + PLBrowser->h()) + DBW,// DISPLAY_HEIGHT - (floor(DISPLAY_HEIGHT / 12) + 3 + 3),
							PBW,
							SBH,
							Player_Keys_Labels[i]);
		    }

		    PlayerKeys[i]->box(FL_PLASTIC_UP_BOX);
		    PlayerKeys[i]->labelsize(IBLS);
		    PlayerKeys[i]->color(FL_BLACK);
		    PlayerKeys[i]->labelcolor(FL_BLACK);
		    PlayerKeys[i]->align(FL_ALIGN_CENTER);
		    PlayerKeys[i]->callback(PlayerRandomPlayNextPrev_CB);
	       }

	       PlayerSeek = new Fl_Slider(PlayerKeys[PLAYER_KEY_NEXT]->x() + PlayerKeys[PLAYER_KEY_NEXT]->w() + DBW, //249,
					    (PLBrowser->y() + PLBrowser->h()) + DBW, //DISPLAY_HEIGHT - (floor(DISPLAY_HEIGHT / 12) + 3 + 3),
					    //280,
					    PSW,
					    SBH);
	       PlayerSeek->box(FL_THIN_DOWN_BOX);
	       PlayerSeek->slider(FL_PLASTIC_UP_BOX);

	       PlayerSeek->bounds(0, 100);
	       PlayerSeek->type(FL_HORIZONTAL);
	       PlayerSeek->slider(FL_NO_BOX);
	       PlayerSeek->slider_size(0.1);
	       PlayerSeek->labelcolor(mui->foregroundColor);
	       PlayerSeek->labelsize(SLS);
	       PlayerSeek->callback(PlayerSeek_CB);
	       PlayerSeek->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);


	       PlayerCrossfade = new Fl_Counter(PLBrowser->x() + PLBrowser->w() - (CFSW + SCFS), //661,
					    (PLBrowser->y() + PLBrowser->h()) + DBW, //DISPLAY_HEIGHT - (floor(DISPLAY_HEIGHT / 12) + 3 + 3),
					    //150,//136,
					    CFSW,
					    SBH);
	       PlayerCrossfade->box(FL_PLASTIC_UP_BOX);
	       PlayerCrossfade->labelfont(MAC_FONT);
	       PlayerCrossfade->textfont(MAC_FONT);
	       PlayerCrossfade->bounds(0, 10);
	       PlayerCrossfade->step(1);
	       PlayerCrossfade->value(mui->playerCrossfade);
	       PlayerCrossfade->type(FL_SIMPLE_COUNTER);
	       PlayerCrossfade->labelcolor(FL_BLACK);//mui->foregroundColor);
	       PlayerCrossfade->color(FL_BLACK);
	       PlayerCrossfade->textcolor(FL_BLACK);
	       PlayerCrossfade->textsize(SLS);
	       PlayerCrossfade->when(FL_WHEN_CHANGED);
	       PlayerCrossfade->callback(PlayerCrossfade_CB);
	       PlayerCrossfade->align(FL_ALIGN_CENTER | FL_ALIGN_CLIP);


	       PlayerCrossfadeBox = new Fl_Box(PlayerCrossfade->x() - CFSW,//125,//531,
					   (PLBrowser->y() + PLBrowser->h()) + DBW,//DISPLAY_HEIGHT + 10 - (floor(DISPLAY_HEIGHT / 12) + 3 + 3),
					   //147,
					   CFSW,
					   SBH,
					   "X-Fade");
	       PlayerCrossfadeBox->labelfont(MAC_FONT);
	       PlayerCrossfadeBox->labelcolor(mui->foregroundColor);
	       PlayerCrossfadeBox->labelsize(SLS);
	       PlayerCrossfadeBox->box(FL_NO_BOX);

	       PLBrowser->end();
	  }
	  pl->end();
     }
     {
	  // MP tab
	  ms = new Fl_Group(DBW,
	                     tabs->y() + TLS,
	                     tabs->w() - (2*DBW),
	                     tabs->h() - DBW,
			     " Music Search ");	  
//	  ms = new Fl_Group(3+3,
//			    40 + 40 + 3,
//			    DISPLAY_WIDTH - (5 + 5),
//			    DISPLAY_HEIGHT - (40 + 40 + 5),
//			    " Music Search ");
	  ms->labelfont(MAC_FONT);
	  ms->color(FL_BLACK);
	  ms->selection_color(SELECTION_COLOR);
	  ms->labelcolor(mui->foregroundColor);
	  ms->labelsize(TLS);
	  {
	       MSQueryText = new Fl_Input( 
	                                   tabs->x() + MSFS + DBW,
	     				   tabs->y() + TLS + DBW,
	     				   MSFW,
                			   IBH + (2 * DBW),
	                                  //(3*DBW)+ MSFS,
			      	          //tabs->y() + DBW,
					  //MSFW,
					  //TLS,
					  "Search string: ");
	       MSQueryText->labelfont(MAC_FONT);
	       MSQueryText->textfont(MAC_FONT);
	       MSQueryText->labelcolor(mui->foregroundColor);
	       MSQueryText->textsize(IBLS);
	       MSQueryText->labelsize(IBLS);
	       MSQueryText->textcolor(FL_BLACK);

	       QueryType = new Fl_Group(
	                                MSQueryText->x() + MSFW + DBW,
	     		                MSQueryText->y(), // + IBLS + DBW,
	     		       	        win->w() - (MSQueryText->x() + MSFW + (2*DBW)),
                			IBH + (2 * DBW),
	                                //398,
	                                //tabs->y() + DBW, /* 40+40+8+5 */
	                                //398,
	                                //TLS,
	                                ""
	                                );
	       QueryType->labelfont(MAC_FONT);
	       QueryType->box(FL_DOWN_BOX);
	       QueryType->color(FL_BLACK);
	       QueryType->labelcolor(mui->foregroundColor);
	       QueryType->labelsize(IBLS);
	       {
		    uint32_t catx = 0;
		    for (uint8_t i = QUERY_ARTIST; i < QUERY_MAX; i++)
		    {
			 MSQueryCheckboxes[i] = new Fl_Round_Button((QueryType->x() + DBW) + catx,
								    // 40 + 40 + 8 + 5,
   			      	                                    QueryType->y(),								    
								    MSQueryButtonsLabels[i].width,
								    IBH + (2 * DBW),
								    MSQueryButtonsLabels[i].label);
			 MSQueryCheckboxes[i]->labelfont(MAC_FONT);
			 MSQueryCheckboxes[i]->labelcolor(mui->foregroundColor);
			 MSQueryCheckboxes[i]->labelsize(IBLS);
			 MSQueryCheckboxes[i]->down_box(FL_ROUND_DOWN_BOX);
			 MSQueryCheckboxes[i]->callback(MSQuery_CB, (void *)&MSQueryButtonsLabels[i].query);
			 //catx += MSQueryButtonsLabels[i].width;
			 catx += floor(QueryType->w()/5);
		    }

		    // tick the Filename checkbox
		    MSQueryCheckboxes[QUERY_FILE]->setonly();
	       }
	       QueryType->end();

		    // *** QWERTY keyboard ***///	       
		    int tx = floor((DISPLAY_WIDTH - 4*DBW)/ 14); //floor( ( DISPLAY_WIDTH - (xi) )/13) ; /* X pitch */
		    //int xi = 2*DBW; /* x initial */
		    int x  =  floor(tx/2) + DBW; /* x counter */
		    int dx = floor( (DISPLAY_WIDTH - (4*DBW))/ 14 )-DBW; //floor(DISPLAY_WIDTH)/10;//48; // DISPLAY_HEIGHT/ 10; /* x size of button */


		    int dy = KBH; /* y size of button  */
		    int ty = dy + DBW;//floor( ( DISPLAY_HEIGHT - (xi) ) /8 ) ; /* Y pitch */  
		    int y = floor(DISPLAY_HEIGHT - ((3*dy) + floor(ty/2) + DBW) ) ;
		  	       

	       KeyBoard = new Fl_Group(
	                               DBW,//3 + 3,
				       DISPLAY_HEIGHT - (DBW + (4 * KBH) ),//DISPLAY_HEIGHT - ((dx * 3) + EBB + DBW), //DISPLAY_HEIGHT - (48 * 3), // 3 lines of dx * dy (48 x 48) buttons
				       DISPLAY_WIDTH - (2 *  DBW),//DISPLAY_WIDTH - (ELB + ERB + (2*DBW)),
				       4 * KBH,//dx * 3,
				       "");
	       KeyBoard->labelfont(MAC_FONT);
	       KeyBoard->box(FL_DOWN_BOX);
	       KeyBoard->color(FL_BLACK);
	       KeyBoard->selection_color(SELECTION_COLOR);
	       KeyBoard->labelcolor(mui->foregroundColor);
	       KeyBoard->labelsize(IBLS);
	       
	       
	       {
	       
		    // First row
		    for (uint8_t i = KEY_Q; i <= KEY_9; i++)
		    {
			 KeyboardKeys[i] = new Fl_Button(x, y, dx, dy, Keyboard_Keys[i].label);
			 //KeyboardKeys[i] = new Fl_Button(20, 20, 20, 20, Keyboard_Keys[i].label);
			 KeyboardKeys[i]->box(FL_RSHADOW_BOX);
			 KeyboardKeys[i]->color(FL_BLACK);
			 KeyboardKeys[i]->labelfont(MAC_FONT);
			 KeyboardKeys[i]->selection_color(SELECTION_KEYBOARD_COLOR);
			 KeyboardKeys[i]->labelcolor(mui->foregroundColor);
			 KeyboardKeys[i]->labelsize(IBLS);
			 KeyboardKeys[i]->callback(KeyboardChar_CB, (void *)&Keyboard_Keys[i]);

			 x += tx;
		    }

		    x = floor(tx/2) + DBW;
		    y += ty;

		    // Second row
		    for (uint8_t i = KEY_A; i <= KEY_6; i++)
		    {
			 KeyboardKeys[i] = new Fl_Button(x, y, dx, dy, Keyboard_Keys[i].label);
			 //KeyboardKeys[i] = new Fl_Button(20, 20, 20, 20, Keyboard_Keys[i].label);
			 KeyboardKeys[i]->box(FL_RSHADOW_BOX);
			 KeyboardKeys[i]->color(FL_BLACK);
			 KeyboardKeys[i]->labelfont(MAC_FONT);
			 KeyboardKeys[i]->selection_color(SELECTION_KEYBOARD_COLOR);
			 KeyboardKeys[i]->labelcolor(mui->foregroundColor);
			 KeyboardKeys[i]->labelsize(IBLS);
			 KeyboardKeys[i]->callback(KeyboardChar_CB, (void *)&Keyboard_Keys[i]);

			 x += tx;
		    }

		    x = floor(tx/2) + DBW;
		    y += ty;

		    // Third row
		    for (uint8_t i = KEY_SPACE; i <= KEY_3; i++)
		    {
			 KeyboardKeys[i] = new Fl_Button(x, y, dx, dy, Keyboard_Keys[i].label);
			 //KeyboardKeys[i] = new Fl_Button(20, 20, 20, 20, Keyboard_Keys[i].label);
			 KeyboardKeys[i]->box(FL_RSHADOW_BOX);
			 KeyboardKeys[i]->color(FL_BLACK);
			 KeyboardKeys[i]->labelfont(MAC_FONT);
			 KeyboardKeys[i]->selection_color(SELECTION_KEYBOARD_COLOR);
			 KeyboardKeys[i]->labelcolor(mui->foregroundColor);
			 KeyboardKeys[i]->labelsize(IBLS);
			 KeyboardKeys[i]->callback(KeyboardChar_CB, (void *)&Keyboard_Keys[i]);

			 x += tx;
		    }

		    KeyboardKeys[KEY_SPACE]->box(FL_PLASTIC_UP_BOX);
		    KeyboardKeys[KEY_SEEK]->box(FL_PLASTIC_UP_BOX);
		    KeyboardKeys[KEY_DEL]->box(FL_PLASTIC_UP_BOX);


	       } // END QUERTY KEYBOARD
	       KeyBoard->end();



	       MSBrowser = new Fl_Browser(
	                                   tabs->x() + DBW,
	     				   MSQueryText->y() + IBH + (DBW *3),
	     				   tabs->w() - (2 * DBW),
                			   win->h() - 
                			   (MSQueryText->y() + IBH + (DBW *3)) -
                			   (DBW + (4 * KBH)) -
                			   DBW
                			   );
             
//	       MSBrowser = new Fl_Browser(
//	                                  tabs->x() + DBW,
//	                                  3 + 3,
//	                                  //tabs->y() + TLS + DBW,
//	                                  tabs->y() + (TLS*2) + DBW,
//	                                  //MSQueryText->y()+TLS,
//	     				  40 + 40 + 8 + 5 + 30 + 5, // 3*(floor(DISPLAY_HEIGHT/12))+3+3,
//	     				  //tabs->w() - (2 * DBW),
//	     				  DISPLAY_WIDTH - (5 + 5),
//	     				  //win->y() - 305);
//	     				  DISPLAY_HEIGHT - 305);
	      
	     //    MSBrowser = new Fl_Browser( (2*DBW),
	     //                              tabs->x() + DBW,
	     //				   tabs->y() + TLS + DBW,
             //			   tabs->w() - (2 * DBW),
	     //				   tabs->h() - TLS - (2 * DBW) - SBH);					  
	       {
		    MSBrowser->column_widths(PLTabWidths);
		    MSBrowser->column_char('\t');
		    MSBrowser->labelfont(MAC_FONT);
		    MSBrowser->textfont(MAC_FONT);
		    MSBrowser->type(FL_HOLD_BROWSER/*FL_SELECT_BROWSER*/);
		    MSBrowser->color(FL_BLACK);
		    MSBrowser->selection_color(SELECTION_COLOR);
		    MSBrowser->labelcolor(mui->foregroundColor);
		    MSBrowser->textsize(BTS);
		    MSBrowser->textcolor(mui->foregroundColor);
		    MSBrowser->scrollbar_width(40);

		    // Tune V/H scrollbars
		    MSBrowser->scrollbar.box(FL_THIN_DOWN_BOX);
		    MSBrowser->scrollbar.slider(FL_PLASTIC_UP_BOX);
		    MSBrowser->hscrollbar.box(FL_THIN_DOWN_BOX);
		    MSBrowser->scrollbar.slider(FL_PLASTIC_UP_BOX);

		    MSBrowser->callback(MSBrowser_CB);
		    ms->resizable(MSBrowser);
	       }
	       MSBrowser->end();
	       

	       // Seek on ENTER key
	       MSQueryText->when(FL_WHEN_ENTER_KEY | FL_WHEN_NOT_CHANGED);
	       MSQueryText->callback(KeyboardChar_CB, (void *)&Keyboard_Keys[KEY_SEEK]);

	  }
	  ms->end();

     }
     tabs->end();

     if (LoadWaitImages() == false)
     {
	  printf("Error while loading bat images.\n");
     }

     waitImage = new Fl_Box(0, 0, mui->waitPNGs.size() ? mui->waitPNGs[0]->w() : 2, mui->waitPNGs.size() ? mui->waitPNGs[0]->h() : 2); // to display the bat.

     win->resizable(tabs);

     if (startFullscreen)
     {
	  win->fullscreen(); // use win->fullscreen_off() to undo this
     }
     else
     {
	  win->position((Fl::w() - win->w()) / 2, (Fl::h() - win->h()) / 2);
     }

     win->end();

     // RMB: just to check the layout
     win->size_range(DISPLAY_WIDTH, DISPLAY_HEIGHT);

     Fl::add_timeout(0.5, UpdateTimeAndSpeed_Task);
     Fl::add_timeout(3.0, GetRadioStatus_Task);
     Fl::add_timeout(1.0, DisplayWaitAnim_Task);

     Fl::lock();

     Fl::visual(FL_DOUBLE | FL_INDEX);
     win->show();
     win->wait_for_expose();

     SendToGPIOD(GPIOD_HELLO);
     // SendToGPIOD(GPIOD_BLUETOOTH_MAC, mui->bluetoothMAC.c_str());

     // if MPD is playing, grab and display the queue
     if (mui->mpd)
     {
	  struct mpd_status *status;

	  mpd_command_list_begin(mui->mpd, true);
	  mpd_send_status(mui->mpd);
	  mpd_send_current_song(mui->mpd);
	  mpd_command_list_end(mui->mpd);

	  if ((status = mpd_recv_status(mui->mpd)) == NULL)
	  {
	       printf("mpd_recv_status() failed\n");
	       MPDEnd();
	  }
	  else
	  {
	       if (mpd_status_get_error(status) != NULL)
	       {
		    printf("error: %s\n", mpd_status_get_error(status));
	       }
	       MPDCheck();

	       mui->plListMode = PLMODE_PLAYLISTS;
	       PLBrowserUpdateTrackList(2, false);
	       mui->plListMode = PLMODE_TRACKS;

	       if (mpd_status_get_elapsed_time(status) > 0)
	       {
		    PlayerSeek->maximum(double(mpd_status_get_total_time(status)));
		    PlayerSeek->value(double(mpd_status_get_elapsed_time(status)));
	       }

	       if ((mpd_status_get_state(status) == MPD_STATE_PLAY) ||
		   (mpd_status_get_state(status) == MPD_STATE_PAUSE) ||
		   (mpd_status_get_state(status) == MPD_STATE_STOP))
	       {
		    if (mpd_status_get_state(status) == MPD_STATE_PLAY)
		    {
			 SendToMONKEY("m");
			 sleep(1);
			 SendToGPIOD(GPIOD_START_MPD);
		    }
	       }
	       else
	       {
		    SendToMONKEY("u");
		    sleep(1);
		    SendToGPIOD(GPIOD_START_RADIO);

		    // Update the playlists
		    PLBrowser->value(1); /* index is 1: "Update Playlists List" requested */
		    PLBrowser_CB(PLBrowser, NULL);
		    PLBrowser->value(0);
	       }

	       mpd_status_free(status);
	  }
     }
     else
     {
	  SendToGPIOD(GPIOD_START_RADIO);

	  PLBrowser->value(1); /* index is 1: "Update Playlists List" requested */
	  PLBrowser_CB(PLBrowser, NULL);
	  PLBrowser->value(0);
     }


     Fl::add_timeout(1.0, CurrentSong_Task);

     // Color theme
     SetDaytimeColors(DAY);

     mui->isReady = true;

     UpdateActiveStateFromMPDStatus();

     // FlTk event loop
     int retValue = Fl::run();

     mui->isRunning = false;
     mui->isReady = false;

     // Will never be sent, but that unlock the GPIOD thread
     SendToGPIOD(GPIOD_BYE);

     // When exiting cleanly...
     if (mui->gpsAvailable)
     {
	  gps_stream(&mui->gpsData, WATCH_DISABLE, NULL);
	  gps_close (&mui->gpsData);
     }

     pthread_cond_signal(&mui->monkeyCond);
     pthread_join(mui->monkeyThread, NULL);

     pthread_cancel(mui->gpiodPingThread);
     pthread_cond_signal(&mui->gpiodCond);
     pthread_join(mui->gpiodThread, NULL);

     if (mui->mpd)
     {
	  mpd_connection_free(mui->mpd);
     }

     delete mui;

     if (win)
     {
	  delete win;
     }

     return retValue;
}
