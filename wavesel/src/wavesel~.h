
/* ------------------------ wavesel ----------------------------- */

#define MAJORVERSION 0
#define MINORVERSION 8
#define BUGFIXVERSION 0

#define DEFAULTSIZE 80
#define TAGBUFFER   64
//#define XLETTAGBUFFER 8
#define MILLIS      0.001
#define ZOOMFACTOR  0.02f
#define MAXZOOM     0.1f // columns per sample

static t_class *wavesel_class;
static t_class *waveselhandle_class;

// element array object types
#define RECT 1
#define LINE 2

// default colors
#define BACKDROP   "white"
#define BORDER     "black"
#define BGCOLOR    "white"
#define BGSELCOLOR "black"
#define FGCOLOR    "gray"
#define FGSELCOLOR "green"
#define MAX5FOREGROUND "black"
#define MAX5BACKGROUND "#d7bd6e"
#define MAX5SELECTEDFOREGROUND "black"
#define MAX5SELECTEDBACKGROUND "6b5e37"

// setmodes
#define MODE_NONE   0  // does nothing
#define MODE_SELECT 1  // x-axis selects left or right of clicked position
#define MODE_LOOP   2  // click centers the selected area around the clicked position. x-axis moves it, y-axis widens/narrows it
#define MODE_MOVE   3  // y-axis zooms in/out, x-axis moves zoomed area. Selected area remains unchanged. Centered around clickpoint
#define MODE_DRAW   4  // not implemented. writes to array.

// objects defaults and minimum dimensions
#define WAVESEL_DEFWIDTH     322
#define WAVESEL_DEFHEIGHT    82
#define WAVESEL_SELBDWIDTH   3.0
#define WAVESEL_SELCOLOR     "#8080ff"
#define WAVESELHANDLE_WIDTH  10
#define WAVESELHANDLE_HEIGHT 10
#define WAVESELHANDLE_COLOR  ""
#define WAVESEL_GRIDWIDTH    0.9
#define WAVESEL_MINWIDTH     42
#define WAVESEL_MINHEIGHT    22
#define WAVESEL_MAXWIDTH     2050
#define WAVESEL_MAXHEIGHT    1500
#define WAVESEL_BUFREFRESH   0  // Max 5 default is 500
#define WAVESEL_XLETHEIGHT   2     // Pd-vanilla is 1, Pd-extended is 2
#define MAXELEM              2048 
// MAXELEM is the maximum element array size. First element is the
// enclosing rectangle, second is the selected background rectangle. The
// remainder is for the sample representing columns.
#define MOUSE_LIST_X     0
#define MOUSE_LIST_Y     1
#define MOUSE_LIST_STATE 2
#define CANVAS_LIST_VIEW_START   0
#define CANVAS_LIST_VIEW_END     1
#define CANVAS_LIST_SELECT_START 2
#define CANVAS_LIST_SELECT_END   3

typedef struct _elem {
    int x;
    int y;
    int w;
    int h;
    int g;
    int type;    
    char* color;
    char* outlinecolor;
} t_element;

typedef struct _wavesel
{
    t_object   x_obj;
    t_glist *  x_glist;
    t_outlet*  out1;
    t_outlet*  out2;
    t_outlet*  out3;
    t_outlet*  out4;
    t_outlet*  out5;
    t_outlet*  out6;
    int        canvas_width;
    int        canvas_height;
    t_float    canvas_x;
    t_float    canvas_y;
    t_element* canvas_element[MAXELEM];
    int        canvas_numelem;
//    t_symbol  *x_selector;
    int        canvas_startcursor;  // relative to left of canvas
    int        canvas_endcursor;    // relative to left of canvas
    int        canvas_clickOrigin;
    int        canvas_startForeground;      // absolute in element array
    int        canvas_endForeground;        // absolute in element array
    int        canvas_selectBackground;      // absolute in element array
//    int        x_endbg;        // not used anymore
    t_symbol  *array_name;
    t_symbol  *array_raw_name;
//    int        x_last_selected;
    int        array_size;
    t_garray  *array;
    t_float    canvas_samplesPerColumn;
    t_float    system_ksr;
    t_float    canvas_columnTop;
    t_float    canvas_columnBottom;
    t_symbol  *canvas_foregroundColor;
    t_symbol  *canvas_backgroundColor;
    t_symbol  *canvas_foregroundSelectedColor;
    t_symbol  *canvas_backgroundSelectedColor;
    t_pd      *editHandle;  
    char       canvas_foregroundTag[TAGBUFFER];
    char       canvas_backgroundTag[TAGBUFFER];
    char       canvas_gridTag[TAGBUFFER];
    char       canvas_allTag[TAGBUFFER]; 
//    char       canvas_inletTag[XLETTAGBUFFER];
//    char       canvas_outtletTag[XLETTAGBUFFER];
    t_canvas  *canvas;
    int        mode;
    t_float    outlet_outputFactor;
    double     array_viewRefresh;
    t_clock   *system_clock;
    int        system_clockRunning;
    int        canvas_mouseDown;
    t_float    canvas_linePosition;
    int        canvas_clipmode;
    t_atom     outlet_mouse[3];
    t_atom     outlet_canvas[4];
    int        canvas_arrayViewStart;
    int        canvas_arrayViewEnd;
    t_float    canvas_columnGain;
    int        array_startSelectSample;
    int        array_endSelectSample;
    int        mouseBound;
} t_wavesel;

// handle start
typedef struct _waveselhandle
{
    t_pd       h_pd;
    t_wavesel *h_master;
    t_symbol  *h_bindsym;
    char       h_pathname[TAGBUFFER];
    char       h_outlinetag[TAGBUFFER];
    int        h_dragon;
    int        h_dragx;
    int        h_dragy;
    int        h_selectedmode;
} t_waveselhandle;
// handle end

/*
  cv .. canvas
  o  .. object identifier
  c  .. element id
  x,y,w,h .. coordinates
*/

static void wavesel_arrayZoom(t_wavesel *x);
static void wavesel_initbase(t_wavesel* x);
void wavesel_drawme(t_wavesel *x, t_glist *glist, int firsttime);
static void wavesel_draw_foreground(t_wavesel* x);
static void wavesel_draw_columns(t_wavesel* x);
static void wavesel_setarray(t_wavesel *x, t_symbol *s);
static void wavesel_vis(t_gobj *z, t_glist *glist, int vis);
static void wavesel_reset_selection(t_wavesel* x) ;
