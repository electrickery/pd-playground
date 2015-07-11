/* wavesel, cheap waveform knock-off, based on gcanvas from Guenter Geiger
 * Fred Jan Kraan, fjkraan@xs4all.nl, 2015-06 */


#include "m_pd.h"
#include "g_canvas.h"
#include "hammer/gui.h"  // just for the mouse-up, silly!

/* ------------------------ wavesel ----------------------------- */


#define DEFAULTSIZE 80
#define TAGBUFFER   64
#define MILLIS      0.001

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

// setmodes
#define MODE_NONE   0  // does nothing
#define MODE_SELECT 1  // x-axis selects left or right of clicked position
#define MODE_LOOP   2  // click centers the selected area around the clicked position. x-axis moves it, y-axis widens/narrows it
#define MODE_MOVE   3  // not implemented. y-axis zooms in/out, x-axis moves zoomed area. Selected area remains unchanged.
#define MODE_DRAW   4  // not implemented. writes to array.

// objects defaults and minimum dimensions
#define WAVESEL_DEFWIDTH     322
#define WAVESEL_DEFHEIGHT    82
#define WAVESEL_SELBDWIDTH   3.0
#define WAVESEL_SELCOLOR     "#8080ff"
#define WAVESELHANDLE_WIDTH  10
#define WAVESELHANDLE_HEIGHT 10
#define WAVESEL_GRIDWIDTH    0.9
#define WAVESEL_MINWIDTH     42
#define WAVESEL_MINHEIGHT    22
#define WAVESEL_MAXWIDTH     2050
#define WAVESEL_BUFREFRESH   0  // Max 5 default is 500
#define WAVESEL_XLETHEIGHT   2     // Pd-vanilla is 1, Pd-extended is 2
#define MAXELEM              2048 
// MAXELEM is the maximum element array size. First element is the
// enclosing rectangle, second is the selected background rectangle. The
//~ // remainder is for the sample representing columns.

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
    int        x_width;
    int        x_height;
    int        x_x;
    int        x_y;
    t_element* x_element[MAXELEM];
    int        x_numelem;
    t_symbol  *x_selector;
    int        x_startcursor;  // relative to left of canvas
    int        x_endcursor;    // relative to left of canvas
    int        x_clickOrigin;
    int        x_startfg;      // absolute in element array
    int        x_endfg;        // absolute in element array
    int        x_startbg;      // absolute in element array
    int        x_endbg;        // not used anymore
    t_symbol  *x_arrayname;
    int        x_last_selected;
    int        x_arraysize;
    t_garray  *x_array;
    t_float    x_samplesPerPixel;
    t_float    x_ksr;
    t_float    x_currentElementTop;
    t_float    x_currentElementBottom;
    t_symbol  *x_fgcolor;
    t_symbol  *x_bgcolor;
    t_symbol  *x_fgselcolor;
    t_symbol  *x_bgselcolor;
    t_pd      *x_handle;  
    char       x_fgtag[TAGBUFFER];
    char       x_bgtag[TAGBUFFER];
    char       x_gridtag[TAGBUFFER];
    char       x_tag[TAGBUFFER];
    unsigned char  x_fgred;
    unsigned char  x_fggreen;
    unsigned char  x_fgblue;
    unsigned char  x_bgred;
    unsigned char  x_bggreen;
    unsigned char  x_bgblue;
    t_canvas  *x_canvas;
    int        x_mode;
    t_float    x_outputFactor;
    double   x_bufRefresh;
    t_clock   *x_clock;
    int        x_clockRunning;
    int        x_mouseDown;
    t_float    x_linePosition;
    int        x_clipmode;
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

static void wavesel_drawbase(t_wavesel* x);
void wavesel_drawme(t_wavesel *x, t_glist *glist, int firsttime);
static void wavesel_draw_foreground(t_wavesel* x);
static void wavesel_draw_columns(t_wavesel* x, t_float f);
static void wavesel_setarray(t_wavesel *x, t_symbol *s);


static void wavesel_setmode(t_wavesel *x, t_float mode)
{
    if (mode < 0 || mode > 4) return;
    x->x_mode = (int)mode;
    if (x->x_mode == 3 || x->x_mode == 4)
        post("wavesel: mode %d not implemented yet", x->x_mode);
}

static t_canvas *wavesel_getcanvas(t_wavesel *x, t_glist *glist)
{
    if (glist != x->x_glist)
    {
	post("wavesel_getcanvas: glist error");
	x->x_glist = glist;
    }
    return (x->x_canvas = glist_getcanvas(glist));
}

static t_canvas *wavesel_isvisible(t_wavesel *x)
{
    return (glist_isvisible(x->x_glist) ? x->x_canvas : 0);
}

static void wavesel_revis(t_wavesel *x)
{
    sys_vgui(".x%lx.c delete %s\n", x->x_canvas, x->x_tag);

    t_waveselhandle *wsh = (t_waveselhandle *)x->x_handle;

    wavesel_drawme(x, x->x_glist, 0);
post("revis");
}

// add "-outline #color#" for selection rectangle only
static void wavesel_create_rectangle(void* cv, void* o, int c, int x, 
    int y, int w, int h, char* color, char* outlinecolor) 
{
//    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lx%d -fill %s\n",
//                cv,                    x, y, x+w,y+h,    o, c,   color);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lx%d -fill %s -outline %s\n",
                cv,                    x, y, x+w,y+h,    o, c,   color, outlinecolor);
}
//    wavesel_move_object(glist_getcanvas(x->x_glist), x, x->x_startbg,
//        x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
//        e->w, e->h);
static void wavesel_move_object(void* cv, void* x, int c, int xx, int y,
    int w,int h) {
    sys_vgui(".x%lx.c coords %lx%d %d %d %d %d\n",
                cv,          x, c, xx, y, xx+w,y+h);
}

static void wavesel_color_object(void* cv, void* o, int c, char* color) 
{
     sys_vgui(".x%lx.c itemconfigure %lx%d -fill %s\n", cv, 
	     o, c, color);
}

static void wavesel_color_rect_object(void* cv, void* o, int c, char* color,
    char* outlinecolor) 
{
     sys_vgui(".x%lx.c itemconfigure %lx%d -fill %s -outline %s\n", cv, 
	     o, c, color, outlinecolor);
}

static void wavesel_delete_object(void* cv,void* o, int c) {
     sys_vgui(".x%lx.c delete %lx%d\n",
	      cv, o,c);
}

static void wavesel_create_column(void* cv, void* o, int c, int x, int y, 
    int w, int h, char* color) {
     sys_vgui(".x%lx.c create line %d %d %d %d -tags %lx%d -fill %s\n",
     cv, x, y, x+w, y+h, o, c, color);
}

static void wavesel_draw_element(t_wavesel *x, int num)
{
    t_element* e = x->x_element[num];
    if (!e) post("wavesel_draw_element assertion failed");
    switch (e->type) {
    case RECT:
        wavesel_create_rectangle(glist_getcanvas(x->x_glist), x, num,
            x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
            e->w, e->h, e->color, e->outlinecolor);
        break;
    case LINE:
        wavesel_create_column(glist_getcanvas(x->x_glist), x, num,
             x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
             e->w, e->h, e->color);                            
            break;
    default:
        post("wavesel: unknown element");
    }
}

static void wavesel_move_element(t_wavesel *x, int num)
{
    t_element* e = x->x_element[num];
    wavesel_move_object(
        glist_getcanvas(x->x_glist), x, num,
        x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
        e->w, e->h);
}

static void wavesel_move_background_selection(t_wavesel *x, int xx, 
    int w)
{
    t_element* e = x->x_element[x->x_startbg];
    e->x = xx;
    e->w = w;
    wavesel_move_object(glist_getcanvas(x->x_glist), x, x->x_startbg,
        x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
        e->w, e->h);
}


static void wavesel_delete_element(t_wavesel *x, int num)
{
    wavesel_delete_object(glist_getcanvas(x->x_glist),x,num); 
}


static void wavesel_color_element(t_wavesel* x, int num, char* color)
{
    t_element* e = x->x_element[num];
    e->color = color;
    wavesel_color_object(glist_getcanvas(x->x_glist), x, num, color); 
}


static void wavesel_color_rect_element(t_wavesel* x, int num, char* color,
    char* outlinecolor)
{
    t_element* e = x->x_element[num];
    e->color = color;
    e->outlinecolor = outlinecolor;
    wavesel_color_rect_object(glist_getcanvas(x->x_glist), x, num, color, outlinecolor); 
}

/* widget helper functions */

void wavesel_drawme(t_wavesel *x, t_glist *glist, int firsttime)
{
    int i;

    for (i = 0; i < x->x_numelem; i++)
    {
        if (firsttime)
            wavesel_draw_element(x, i);
        else
            wavesel_move_element(x, i);
    }
        
    /* outlets */
	int n = 6;
	int nplus;
	nplus = (n == 1 ? 1 : n-1);
	for (i = 0; i < n; i++)
	{
	    int onset = x->x_obj.te_xpix + (x->x_width - IOWIDTH) * i / 
            nplus;
	    if (firsttime)
		sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxo%d\n",
		    glist_getcanvas(glist),
		    onset, x->x_obj.te_ypix + x->x_height - WAVESEL_XLETHEIGHT,
		    onset + IOWIDTH, x->x_obj.te_ypix + x->x_height,
		    x, i);
	    else
		sys_vgui(".x%lx.c coords %lxo%d %d %d %d %d\n",
		     glist_getcanvas(glist), x, i,
		     onset, x->x_obj.te_ypix + x->x_height - WAVESEL_XLETHEIGHT,
		     onset + IOWIDTH, x->x_obj.te_ypix + x->x_height);
	}
	/* inlets */
	n = 1; 
	nplus = (n == 1 ? 1 : n-1);
	for (i = 0; i < n; i++)
	{
	    int onset = x->x_obj.te_xpix + (x->x_width - IOWIDTH) * i / 
            nplus;
	    if (firsttime)
		sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxi%d\n",
		    glist_getcanvas(glist),
		    onset, x->x_obj.te_ypix,
		    onset + IOWIDTH, x->x_obj.te_ypix + WAVESEL_XLETHEIGHT,
		    x, i);
	    else
		sys_vgui(".x%lx.c coords %lxi%d %d %d %d %d\n",
		    glist_getcanvas(glist), x, i,
		    onset, x->x_obj.te_ypix,
		    onset + IOWIDTH, x->x_obj.te_ypix + WAVESEL_XLETHEIGHT);
    }
}

// called when an object is destroyed
void wavesel_erase(t_wavesel* x, t_glist* glist)
{
    int n, i;

    for (i = 0; i < x->x_numelem; i++)
         wavesel_delete_element(x, i);

     n = 3;
     while (n--) {
         sys_vgui(".x%lx.c delete %lxo%d\n", 
             glist_getcanvas(glist), x, n);
     }
     x->x_numelem = 0;
     if (x->x_clockRunning)
         clock_free(x->x_clock);
     hammergui_unbindmouse((t_pd *)x);
}

static void wavesel_reset_selection(t_wavesel* x) ;

/* ------------------ wavesel widgetbehaviour----------------------- */


static void wavesel_getrect(t_gobj *z, t_glist *owner,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    int width, height;
    t_wavesel* s = (t_wavesel*)z;

    width = s->x_width;
    height = s->x_height;
    *xp1 = s->x_obj.te_xpix;
    *yp1 = s->x_obj.te_ypix;
    *xp2 = s->x_obj.te_xpix + width;
    *yp2 = s->x_obj.te_ypix + height;
} 

static void wavesel_displace(t_gobj *z, t_glist *glist,
    int dx, int dy)
{
    t_wavesel *x = (t_wavesel *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    wavesel_drawme(x, glist, 0);
    canvas_fixlinesfor(glist,(t_text*) x);
}

static void wavesel_redraw_all(t_wavesel* x)
{
        wavesel_erase(x, x->x_glist);
        
        wavesel_drawbase(x);
        wavesel_draw_foreground(x);
        wavesel_setarray(x, x->x_arrayname);
        wavesel_draw_columns(x, 1);
}

static void wavesel_deselect(t_wavesel* x, t_canvas *cv, 
    t_waveselhandle *wsh)
{
    sys_vgui(".x%lx.c itemconfigure %s -outline black -width %f\
        -fill #%2.2x%2.2x%2.2x\n", cv, x->x_bgtag, WAVESEL_GRIDWIDTH,
        x->x_bgred, x->x_bggreen, x->x_bgblue);
    sys_vgui("destroy %s\n", wsh->h_pathname);
    wsh->h_selectedmode = 0;  
}

static void wavesel_select(t_gobj *z, t_glist *glist, int state)
{
    t_wavesel *x = (t_wavesel *)z;
     
// handle start     
    t_canvas *cv = wavesel_getcanvas(x, glist);
    t_waveselhandle *wsh = (t_waveselhandle *)x->x_handle;
    if (state)
    {
        if (wsh->h_selectedmode) // prevent re-selection
            return;

        int x1, y1, x2, y2;
        wavesel_getrect(z, glist, &x1, &y1, &x2, &y2);

        sys_vgui(".x%lx.c itemconfigure %s -outline blue -width %f -fill %s\n",
            cv, x->x_bgtag, WAVESEL_SELBDWIDTH, WAVESEL_SELCOLOR);
        sys_vgui("canvas %s -width %d -height %d -bg #fedc00 -bd 0\n",
            wsh->h_pathname, WAVESELHANDLE_WIDTH, WAVESELHANDLE_HEIGHT);
            sys_vgui(".x%lx.c create window %f %f -anchor nw\
            -width %d -height %d -window %s -tags %s\n",
             cv, x2 - (WAVESELHANDLE_WIDTH - WAVESEL_SELBDWIDTH),
             y2 - (WAVESELHANDLE_HEIGHT - WAVESEL_SELBDWIDTH),
            WAVESELHANDLE_WIDTH, WAVESELHANDLE_HEIGHT,
            wsh->h_pathname, x->x_tag);
        sys_vgui("bind %s <Button> {pdsend [concat %s _click 1 \\;]}\n",
            wsh->h_pathname, wsh->h_bindsym->s_name);
        sys_vgui("bind %s <ButtonRelease> {pdsend [concat %s _click 0 \\;]}\n",
            wsh->h_pathname, wsh->h_bindsym->s_name);
        sys_vgui("bind %s <Motion> {pdsend [concat %s _motion %%x %%y \\;]}\n",
            wsh->h_pathname, wsh->h_bindsym->s_name);
        wsh->h_selectedmode = 1;
    }
    else
    {
        wavesel_deselect(x, cv, wsh);
        
        wavesel_redraw_all(x);
    }
// handle end     
}


static void wavesel_delete(t_gobj *z, t_glist *glist)
{
    t_text *x = (t_text *)z;
    canvas_deletelinesfor(glist, x);
}

#define FRAME 0       
static void wavesel_vis(t_gobj *z, t_glist *glist, int vis)
{
    t_wavesel* x = (t_wavesel*)z;
    t_canvas *cv = wavesel_getcanvas(x, glist);
    t_waveselhandle *wsh = (t_waveselhandle *)x->x_handle;
    
    if (wsh->h_selectedmode)
        return;
        
    if (!x->x_numelem)
        return;
    
    sprintf(wsh->h_pathname, ".x%lx.h%lx", (unsigned long)cv, 
        (unsigned long)wsh);
    if (vis)
    {
        x->x_element[FRAME]->h = x->x_height;
        x->x_element[FRAME]->w = x->x_width;
        wavesel_drawme(x, glist, 1);
        post("wavesel_vis: vis");
    }
    else
    {
        wavesel_erase(x, glist);
        post("wavesel_vis: erase");
    }
}

t_widgetbehavior   wavesel_widgetbehavior;

static void wavesel_color_columns(t_wavesel *x, int _s, int _e, 
    int select)
{
    int start;
    int end;
    int i;
    if (_s < _e)
    {
        start = _s;
        end = _e;
    }
    else
    {
        start = _e;
        end = _s;
    }
    for (i = start; i <= end; i++)
    {
        if (select)
             wavesel_color_element(x, i + x->x_startfg, 
                 x->x_fgselcolor->s_name);
        else
             wavesel_color_element(x, i + x->x_startfg, 
                 x->x_fgcolor->s_name);
    }
    // background selected rectangle
    wavesel_color_rect_element(x, x->x_startbg, x->x_bgselcolor->s_name,
        x->x_bgselcolor->s_name);
    wavesel_move_background_selection(x, x->x_startcursor, 
        x->x_endcursor - x->x_startcursor);

}

static void wavesel_color_all_columns(t_wavesel *x)
{
    int i;
    int size = x->x_endfg - x->x_startfg;
    if (x->x_endcursor == 0)
        wavesel_reset_selection(x);
    else {
        wavesel_color_columns(x, 0, x->x_startcursor, 0);
        wavesel_color_columns(x, x->x_startcursor, x->x_endcursor, 1);
        wavesel_color_columns(x, x->x_endcursor, size, 0);
    }
}

static void wavesel_selectmode_motion(t_wavesel *x)
{   
    if (x->x_x > x->x_clickOrigin) // right side
    {
        wavesel_color_columns(x, x->x_startcursor, x->x_clickOrigin, 0); 
        wavesel_color_columns(x, x->x_clickOrigin, x->x_x, 1); 
        wavesel_color_columns(x, x->x_x, x->x_endcursor, 0);
//        wavesel_move_background_selection(x, x->x_clickOrigin + 1, x->x_x - x->x_clickOrigin);
        x->x_startcursor = x->x_clickOrigin;
        x->x_endcursor   = x->x_x;
        outlet_float(x->out4, x->x_x * x->x_outputFactor);
    }
    else //left side
    { 
        wavesel_color_columns(x, x->x_clickOrigin, x->x_endcursor, 0);
        wavesel_color_columns(x, x->x_x, x->x_clickOrigin, 1);
        wavesel_color_columns(x, x->x_startcursor, x->x_x, 0);
//        wavesel_move_background_selection(x, x->x_x + 1,  x->x_clickOrigin - x->x_x);
        x->x_startcursor = x->x_x;
        x->x_endcursor   = x->x_clickOrigin;
        outlet_float(x->out3, x->x_x * x->x_outputFactor);
    }
    wavesel_move_background_selection(x, x->x_startcursor + 1,
        x->x_endcursor - x->x_startcursor);
    outlet_float(x->out5, 2);    
}

static void wavesel_loopmode_motion(t_wavesel *x, t_floatarg dx, 
    t_floatarg dy)
{
    int i;
    int size = x->x_endfg - x->x_startfg + 2; // rationalize this offset
    int dix = (int)dx;
    int diy = (int)dy;
    int dstart = dix + diy;
    int dend   = dix - diy;
    int oldStartCursor = x->x_startcursor;
    int oldEndCursor = x->x_endcursor;
    x->x_startcursor += dstart;
    x->x_endcursor += dend;

    // check boundaries and internal consistency    
    x->x_startcursor = (x->x_startcursor < 0)  ? 0 : x->x_startcursor;
    x->x_endcursor   = (x->x_endcursor > size) ? size : x->x_endcursor;
        
    if (x->x_startcursor > x->x_endcursor) {
        x->x_startcursor = x->x_x;
        x->x_endcursor   = x->x_x;
    }
    
    // minimize the range to update
    int startUpdateRange = (x->x_startcursor > oldStartCursor) ? 
        oldStartCursor : x->x_startcursor;
    int endUpdateRange   = (x->x_endcursor > oldEndCursor) ? 
        x->x_endcursor : oldEndCursor;
 
    for (i = startUpdateRange; i <= endUpdateRange; i++)
    {
        if (i < x->x_startcursor || i > x->x_endcursor)
            wavesel_color_element(x, i + x->x_startfg, 
                x->x_fgcolor->s_name);
        else
            wavesel_color_element(x, i + x->x_startfg, 
                x->x_fgselcolor->s_name);
    }
    
    wavesel_color_rect_element(x, x->x_startbg, x->x_bgselcolor->s_name,
        x->x_bgselcolor->s_name);
    wavesel_move_background_selection(x, x->x_startcursor, 
        x->x_endcursor - x->x_startcursor);
        
    outlet_float(x->out3, (x->x_startcursor * x->x_outputFactor));
    outlet_float(x->out4, (x->x_endcursor * x->x_outputFactor));
    outlet_float(x->out5, 2);
}

static void wavesel_motion(t_wavesel *x, t_floatarg dx, t_floatarg dy)
{
    x->x_x += dx;
    x->x_y += dy;

    if (x->x_x >= 0 && x->x_x <= x->x_width - 2)
    {
        switch (x->x_mode) {
        case MODE_NONE:
            break; 
        case MODE_SELECT:
            wavesel_selectmode_motion(x);
            break;
        case MODE_LOOP:
            wavesel_loopmode_motion(x, dx, dy);
            break;
        case MODE_MOVE:
//            wavesel_movemode_motion(x, dx, dy);
            break;
        case MODE_DRAW:
             break;
        }
    }
    wavesel_drawme(x, x->x_glist, 0);
}

void wavesel_key(t_wavesel *x, t_floatarg f)
{
  post("key");
}

static void wavesel_selectmode_click(t_wavesel *x)
{
    wavesel_reset_selection(x);
    
    outlet_float(x->out3, x->x_x * x->x_outputFactor);
    outlet_float(x->out4, x->x_x * x->x_outputFactor);
    
    wavesel_color_element(x, x->x_x + x->x_startfg, 
        x->x_fgselcolor->s_name);
        
    wavesel_color_rect_element(x, x->x_startbg, 
        x->x_bgselcolor->s_name, x->x_bgselcolor->s_name);
    wavesel_move_background_selection(x, x->x_x, 0);

    x->x_startcursor = x->x_x;
    x->x_endcursor   = x->x_x;
    x->x_clickOrigin = x->x_x;
}

static void wavesel_loopmode_click(t_wavesel *x)
{
    int selectedRange = (x->x_endcursor - x->x_startcursor) / 2;
    int columnCount = x->x_endfg - x->x_startfg;
    if (!x->x_endcursor) // if no selection
        wavesel_selectmode_click(x);
    else
    {
        wavesel_reset_selection(x);
        x->x_startcursor = x->x_x - selectedRange;
        x->x_endcursor = x->x_x + selectedRange;
        x->x_startcursor = (x->x_startcursor < 0) ? 0 : x->x_startcursor;
        x->x_endcursor = (x->x_endcursor > columnCount) ? 
            columnCount : x->x_endcursor;

        wavesel_color_all_columns(x);
    }
}

static void wavesel_click(t_wavesel *x,
    t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl,
    t_floatarg doit, int up)
{
    glist_grab(x->x_glist, &x->x_obj.te_g,
        (t_glistmotionfn)wavesel_motion, (t_glistkeyfn)NULL, xpos, ypos);

    x->x_x = xpos - x->x_obj.te_xpix;
    x->x_y = ypos - x->x_obj.te_ypix;

    outlet_float(x->out5, 0);
 
    if (x->x_x > 0 && x->x_x < x->x_width -2 && x->x_y > 0 && 
        x->x_y < x->x_height)
    {
        switch (x->x_mode) {
        case MODE_NONE:
            break; 
        case MODE_SELECT:
            wavesel_selectmode_click(x);
            break;
        case MODE_LOOP:
            wavesel_loopmode_click(x);
            break;
        case MODE_MOVE:
        case MODE_DRAW:
            break;
        }
    }
    wavesel_drawme(x, x->x_glist, 0);
}

static int wavesel_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    if (doit)
        wavesel_click((t_wavesel *)z, (t_floatarg)xpix, 
            (t_floatarg)ypix, (t_floatarg)shift, 0, (t_floatarg)alt,dbl);

    if (dbl) outlet_float(((t_wavesel*)z)->out5, 1);
   
    return (1);
}

static void wavesel_activate(t_gobj *z, t_glist *glist, int state)
{
    
}

// when is what called (also see g_canvas.h):
// wavesel_getrect  - mouse cursor inside patch window
// wavesel_displace - move canvas in edit mode ?
// wavesel_select   - wavesel object is selected in edit mode
// wavesel_activate - wavesel object is moved in edit mode
// wavesel_delete   - wavesel object is removed ?
// wavesel_vis      - patch window is visable (again)
// wavesel_newclick - mouse cursor inside wavesel canvas
static void wavesel_setwidget(void)
{
    wavesel_widgetbehavior.w_getrectfn  = wavesel_getrect;
    wavesel_widgetbehavior.w_displacefn = wavesel_displace;
    wavesel_widgetbehavior.w_selectfn   = wavesel_select;
    wavesel_widgetbehavior.w_activatefn = wavesel_activate;
    wavesel_widgetbehavior.w_deletefn   = wavesel_delete;
    wavesel_widgetbehavior.w_visfn      = wavesel_vis;
    wavesel_widgetbehavior.w_clickfn    = wavesel_newclick;
}

static void wavesel_rect(t_wavesel* x, t_symbol* c, float xp, float y, 
    float w, float h)
{
    t_element* e = getbytes(sizeof(t_element));
    x->x_element[x->x_numelem] = e;
    
    e->type = RECT;
    e->x = xp;
    e->y = y;
    e->w = w;
    e->h = h;
    e->color = c->s_name;
    wavesel_draw_element(x, x->x_numelem);
    x->x_numelem++;
}

static void wavesel_column(t_wavesel* x, t_symbol* c, float xp, float y, 
    float w, float h)
{
    if (x->x_numelem < MAXELEM-1) {
        t_element* e = getbytes(sizeof(t_element));
        x->x_element[x->x_numelem] = e;
        
        e->type = LINE;
        e->x = xp;
        e->y = y;
        e->w = w;
        e->h = h;
        e->g = 1;
        e->color = c->s_name;
        wavesel_draw_element(x, x->x_numelem);
        x->x_numelem++;
    }
}

static void wavesel_color(t_wavesel* x, t_symbol* c, float num)
{
    wavesel_color_element(x,(int)num,c->s_name);
}


void wavesel_deletenum(t_wavesel* x,float num)
{
    int i = (int) num;
    if (x->x_element[i]) {
        wavesel_delete_element(x,i);
        freebytes(x->x_element[i],sizeof(t_element));
        x->x_element[i] = NULL;
    }
}

// find extremes in the sample range represented by this column
static void wavesel_get_column_size(t_wavesel* x, int column, 
    t_word *vec)
{
    int sampleOffset   = (int)(column * x->x_samplesPerPixel);
    int sampleEndPoint = sampleOffset + (int)x->x_samplesPerPixel;
    int i;
    t_float maxSize = 0.5;
    t_float upper = 0, lower = 0;
    for (i = sampleOffset; i < sampleEndPoint; i++) 
    {
        upper = (upper < (vec[i].w_float)) ? vec[i].w_float : upper;
        lower = (lower > (vec[i].w_float)) ? vec[i].w_float : lower;
    }
    if (x->x_clipmode) 
    {
         x->x_currentElementTop = (upper >  maxSize) ?  maxSize : upper;
         x->x_currentElementBottom = 
             (lower < -maxSize) ? -maxSize : lower;
    }
    else
    {
        x->x_currentElementTop = upper;
        x->x_currentElementBottom = lower;
    }
}

static void wavesel_draw_columns(t_wavesel* x, t_float f)
{
    int forceFirstTime = (f < 0) ? 0 : 1;
    if (!x->x_array) 
        return;
    t_garray *ax = x->x_array;
    t_word *vec;
    int size;
    if (!garray_getfloatwords(ax, &size, &vec))
    {
        post("wavesel: couldn't read from array!");
        return;
    }
    int column;
    int middle = (int)(x->x_height / 2);
    int columnCount = x->x_endfg - x->x_startfg;
    for (column = 0; column <= columnCount; column++)
    {
        wavesel_get_column_size(x, column, vec);
        x->x_element[column + x->x_startfg]->y = middle - 
            (int)(x->x_currentElementTop * x->x_height);
        x->x_element[column + x->x_startfg]->h = 
            (int)((x->x_currentElementTop - 
            x->x_currentElementBottom) * x->x_height + 1);
    }
    wavesel_drawme(x, x->x_glist, forceFirstTime);
}

static void wavesel_draw_foreground(t_wavesel* x)
{
    int column;
    int e = x->x_numelem;
    int middle = (int)(x->x_height / 2);
    x->x_startfg = x->x_numelem;
    for (column = 1; column < x->x_width; column++)
    {
        x->x_element[e] = getbytes(sizeof(t_element));
        x->x_element[e]->type = LINE;
        x->x_element[e]->x = column;
        x->x_element[e]->y = middle;
        x->x_element[e]->w = 0;
        x->x_element[e]->h = 1;
        x->x_element[e]->color = x->x_fgcolor->s_name;
        x->x_numelem++;
        e++;
    }
    x->x_endfg = x->x_width - 2;
}

static void wavesel_reset_selection(t_wavesel* x) 
{
    if (x->x_startfg == 0) return;
    int i;
    for (i = x->x_startfg; i <= x->x_endfg; i++)
        wavesel_color_element(x, i, x->x_fgcolor->s_name);
        
    wavesel_color_rect_element(x, x->x_startbg, x->x_bgcolor->s_name, 
        x->x_bgcolor->s_name);
    wavesel_move_background_selection(x, x->x_startbg, 0);
    x->x_startcursor = 0;
    x->x_endcursor = 0;
}

static void wavesel_drawbase(t_wavesel* x) 
{
    if (x->x_numelem)
        wavesel_erase(x, x->x_glist);

    int e = x->x_numelem;
    x->x_element[e] = getbytes(sizeof(t_element));
    x->x_element[e]->type = RECT;
    x->x_element[e]->x = 0;
    x->x_element[e]->y = 0;
    x->x_element[e]->w = x->x_width;
    x->x_element[e]->h = x->x_height;
    x->x_element[e]->color = BACKDROP;
    x->x_element[e]->outlinecolor = BORDER;
    x->x_numelem++;
    
    e = x->x_numelem;
    x->x_startbg = e;
    x->x_element[e] = getbytes(sizeof(t_element));
    x->x_element[e]->type = RECT;
    x->x_element[e]->x = 1;
    x->x_element[e]->y = 1;
    x->x_element[e]->w = 0;
    x->x_element[e]->h = x->x_height - 3;
    x->x_element[e]->color = x->x_bgcolor->s_name;
    x->x_element[e]->outlinecolor = x->x_bgcolor->s_name;
    x->x_numelem++;
}
  
static void wavesel_state(t_wavesel* x) 
{
    post(" --==## wavesel_state ##==--");
    post("x_width: %d",           x->x_width);
    post("x_height: %d",          x->x_height);
    post("x_numelem: %d",         x->x_numelem);
    post("x_startcursor (relative to canvas): %d, (%f ms)",     x->x_startcursor, x->x_startcursor * x->x_outputFactor);
    post("x_endcursor (relative to canvas): %d, (%f ms)",       x->x_endcursor, x->x_endcursor * x->x_outputFactor);
    post("selected bg width: %d", x->x_endcursor - x->x_startcursor);
    post("x_startfg (absolute in element array): %d",  x->x_startfg);
    post("x_endfg (absolute in element array): %d",    x->x_endfg);
    post("x_startbg (absolute in element array): %d",  x->x_startbg);
    post("x_array: %s",          (x->x_array) ? "defined" : "null");
    post("x_arrayname: %s",       x->x_arrayname->s_name);
    post("x_arraysize: %d",       x->x_arraysize);
    post("x_samplesPerPixel: %f", x->x_samplesPerPixel);
    post("x_ksr: %f",             x->x_ksr);
    post("x_outputFactor: %f",    x->x_outputFactor);
    post("x_mode: %d",            x->x_mode);
    post("x_bufRefresh: %lf",     x->x_bufRefresh);
    post("x_clockRunning: %d",    x->x_clockRunning);
    post("x_linePosition: %f",    x->x_linePosition);
    post("x_clipmode: %d",        x->x_clipmode);
    post("-- base rectangle:");
    post(" x: %d", x->x_element[0]->x);
    post(" y: %d", x->x_element[0]->y);
    post(" w: %d", x->x_element[0]->w);
    post(" h: %d", x->x_element[0]->h);
    post(" color: %s", x->x_element[0]->color);
    post(" outlinecolor: %s", x->x_element[0]->outlinecolor);
    post("-- background select rectangle:");
    post(" x: %d", x->x_element[x->x_startbg]->x);
    post(" y: %d", x->x_element[x->x_startbg]->y);
    post(" w: %d", x->x_element[x->x_startbg]->w);
    post(" h: %d", x->x_element[x->x_startbg]->h);
    post(" color: %s", x->x_element[x->x_startbg]->color);
    post(" outlinecolor: %s", x->x_element[x->x_startbg]->outlinecolor);
}

static void wavesel_setarray(t_wavesel *x, t_symbol *s)
{
    t_garray *array;
    x->x_arrayname = s;
    
    if ((array = (t_garray *)pd_findbyclass(x->x_arrayname, 
        garray_class)))
    {
        x->x_array = array;
        x->x_arraysize = garray_npoints(x->x_array);
        x->x_samplesPerPixel = (t_float)x->x_arraysize / 
            (t_float)x->x_width;
        x->x_outputFactor = x->x_samplesPerPixel / x->x_ksr;
    } else {
        post("wavesel: no array \"%s\" (error %d)", 
            x->x_arrayname->s_name, array);
        x->x_array = 0;
        x->x_arraysize = 0;
        return;
    }
    outlet_float(x->out1, 0);
    outlet_float(x->out2, x->x_arraysize / x->x_ksr);
    outlet_float(x->out3, x->x_startcursor * x->x_outputFactor);
    outlet_float(x->out4, x->x_endcursor   * x->x_outputFactor);
    wavesel_draw_columns(x, 0);
}

static void wavesel_setfgcolor(t_wavesel *x, t_symbol *s)
{
    x->x_fgcolor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setbgcolor(t_wavesel *x, t_symbol *s)
{
    x->x_bgcolor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setfgselcolor(t_wavesel *x, t_symbol *s)
{
    x->x_fgselcolor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setbgselcolor(t_wavesel *x, t_symbol *s)
{
    x->x_bgselcolor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_buftime(t_wavesel* x, float fRefresh)
{
    x->x_bufRefresh = (double)(fRefresh < 0) ? WAVESEL_BUFREFRESH : 
        fRefresh;
    clock_delay(x->x_clock, x->x_bufRefresh);
}

static void wavesel_tick(t_wavesel *x)
{
    if (!x->x_array && x->x_arrayname->s_name[0] != 0)
        wavesel_setarray(x, x->x_arrayname);
//    post("tick: %s", x->x_arrayname->s_name);
    x->x_clockRunning = 0;
    if (x->x_bufRefresh == 0)
        return;
    if (!x->x_mouseDown)
    {
        wavesel_draw_columns(x, 0);
    }
    clock_delay(x->x_clock, x->x_bufRefresh);
    x->x_clockRunning = 1;
//    outlet_float(x->out3, 1);  // test clock-tick indicator
}

static void waveselhandle__clickhook(t_waveselhandle *wsh, t_floatarg f)
{
    int newstate = (int)f;
    if (wsh->h_dragon && newstate == 0)
    {
	t_wavesel *x = wsh->h_master;
	t_canvas *cv;
	x->x_width += wsh->h_dragx;
	x->x_height += wsh->h_dragy;
	if ((cv = wavesel_isvisible(x)))
	{
	    sys_vgui(".x%lx.c delete %s\n", cv, wsh->h_outlinetag);
	    wavesel_revis(x);
	    sys_vgui("destroy %s\n", wsh->h_pathname);
	    wavesel_select((t_gobj *)x, x->x_glist, 1);
	    canvas_fixlinesfor(x->x_glist, (t_text *)x);  /* 2nd inlet */
	}
    }
    else if (!wsh->h_dragon && newstate)
    {
	t_wavesel *x = wsh->h_master;
	t_canvas *cv;
	if ((cv = wavesel_isvisible(x)))
	{
	    int x1, y1, x2, y2;
	    wavesel_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
	    sys_vgui("lower %s\n", wsh->h_pathname);
	    sys_vgui(".x%lx.c create rectangle %d %d %d %d\
 -outline blue -width %f -tags %s\n",
		     cv, x1, y1, x2, y2, WAVESEL_SELBDWIDTH, wsh->h_outlinetag);
	}
	wsh->h_dragx = 0;
	wsh->h_dragy = 0;
    }
    wsh->h_dragon = newstate;
}

static void waveselhandle__motionhook(t_waveselhandle *wsh,
				    t_floatarg f1, t_floatarg f2)
{
    if (wsh->h_dragon)
    {
        t_wavesel *x = wsh->h_master;
        int dx = (int)f1, dy = (int)f2;
        int x1, y1, x2, y2, newx, newy;
        wavesel_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        newx = x2 + dx;
        newy = y2 + dy;
        if (newx > x1 + WAVESEL_MINWIDTH && 
            newy > y1 + WAVESEL_MINHEIGHT)
        {
            t_canvas *cv;
            if ((cv = wavesel_isvisible(x)))
                sys_vgui(".x%lx.c coords %s %d %d %d %d\n",
                    cv, wsh->h_outlinetag, x1, y1, newx, newy);
            wsh->h_dragx = dx;
            wsh->h_dragy = dy;
        }
        x->x_width  = x2 - x1;
        x->x_height = y2 - y1;
    }
}

static void *wavesel_line(t_wavesel *x, t_float f)
{
    x->x_linePosition = f;
    wavesel_reset_selection(x);
    int size = x->x_endfg - x->x_startfg;
    int column = (int)((x->x_linePosition / x->x_samplesPerPixel) * 
        x->x_ksr + 0.5);
        
    if (column >= 0 && column <= size) {
        wavesel_color_element(x, column + x->x_startfg, 
            x->x_fgselcolor->s_name);
            
        wavesel_move_background_selection(x, column, 1);
        wavesel_color_rect_element(x, x->x_startbg, 
            x->x_bgselcolor->s_name, x->x_bgselcolor->s_name);
    }
}

static void *wavesel_clipdraw(t_wavesel *x, t_float f)
{
    x->x_clipmode = (int)(f < 0) ? 0 : 1;
}

static void *wavesel_doup(t_wavesel *x, t_float f)
{
    x->x_mouseDown = (int)(f > 0) ? 0 : 1;
    outlet_float(x->out5, 3);
//    if (x->x_bufRefresh == 0)
//        wavesel_draw_columns(x, 0);
}

static void *wavesel_doelse(t_wavesel *x, t_float f1, t_float f2)
{
//    post("wavesel_doelse:");
}

// arguments to add: setmode, buftime, fgcolor, bgcolor, fgselcolor, bgselcolor, 
static void *wavesel_new(t_symbol* s, t_float w, t_float h)
{
    int i;
    t_wavesel *x = (t_wavesel *)pd_new(wavesel_class);

    x->x_glist = (t_glist*) canvas_getcurrent();
    x->x_ksr   = sys_getsr() * MILLIS;
    x->x_outputFactor = 1.; // used for outlet unit calculation
    x->x_mode  = 0;
    x->x_mouseDown = 0;
    
    for (i=0;i<MAXELEM;i++)
        x->x_element[i] = NULL;
    x->x_numelem   = 0;
    x->x_arrayname = s;
    x->x_array     = 0;
        
    x->x_bufRefresh = (double)WAVESEL_BUFREFRESH;
    x->x_clock = clock_new(x, (t_method)wavesel_tick); // get a clock
    clock_delay(x->x_clock, x->x_bufRefresh); // start it
    x->x_clockRunning = 1;

    x->x_width = DEFAULTSIZE;
    if ((int)w > WAVESEL_MINWIDTH) 
        x->x_width = w;

    x->x_height = DEFAULTSIZE;
    if ((int)h > WAVESEL_MINHEIGHT)
        x->x_height = h;

    x->x_startcursor = 0;
    x->x_endcursor   = 0;
    x->x_last_selected = 0;
    x->x_fgcolor = gensym(FGCOLOR);
    x->x_bgcolor = gensym(BGCOLOR);
    x->x_fgselcolor = gensym(FGSELCOLOR);
    x->x_bgselcolor = gensym(BGSELCOLOR);
    wavesel_drawbase(x);
    wavesel_draw_foreground(x);
    wavesel_draw_columns(x, 1);

    x->out1 = outlet_new(&x->x_obj, &s_float);
    x->out2 = outlet_new(&x->x_obj, &s_float);
    x->out3 = outlet_new(&x->x_obj, &s_float);
    x->out4 = outlet_new(&x->x_obj, &s_float);
    x->out5 = outlet_new(&x->x_obj, &s_float);
    x->out6 = outlet_new(&x->x_obj, &s_float);
    
// handle start  
    t_waveselhandle *wsh;
    char handlebuf[TAGBUFFER];    
    x->x_handle = pd_new(waveselhandle_class);
    wsh = (t_waveselhandle *)x->x_handle;
    wsh->h_master = x;
    sprintf(handlebuf, "_h%lx", (unsigned long)wsh);
    pd_bind(x->x_handle, wsh->h_bindsym = gensym(handlebuf));
    sprintf(wsh->h_outlinetag, "h%lx", (unsigned long)wsh);
    wsh->h_dragon = 0;
    
    sprintf(x->x_tag,     "all%lx", (unsigned long)x);
    sprintf(x->x_bgtag,   "bg%lx",  (unsigned long)x);
    sprintf(x->x_gridtag, "gr%lx",  (unsigned long)x);
    sprintf(x->x_fgtag,   "fg%lx",  (unsigned long)x);
    
    wsh->h_selectedmode = 0;
// handle end 

// init hammer for mouseUp (wavesel_doup)
    hammergui_bindmouse((t_pd *)x); 
    
    return (x);
}

void wavesel_tilde_setup(void)
{
    wavesel_class = class_new(gensym("wavesel~"), 
        (t_newmethod)wavesel_new, 0, sizeof(t_wavesel), 0, 
        A_DEFSYM, A_DEFFLOAT, A_DEFFLOAT, 0);

// Max/MSP messages
    class_addmethod(wavesel_class, (t_method)wavesel_buftime, 
        gensym("buftime"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_clipdraw, 
        gensym("clipdraw"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_line, 
        gensym("line"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setarray, 
        gensym("set"), A_SYMBOL, 0);

// wavesel specific
    class_addmethod(wavesel_class, (t_method)wavesel_setarray, 
        gensym("setarray"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setfgcolor, 
        gensym("setfgcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setbgcolor, 
        gensym("setbgcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setfgselcolor, 
        gensym("setfgselcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setbgselcolor, 
        gensym("setbgselcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_reset_selection, 
        gensym("reset_selection"), 0);
    class_addmethod(wavesel_class, (t_method)wavesel_draw_columns, 
        gensym("drawwf"), A_FLOAT, 0);

// internal
    class_addmethod(wavesel_class, (t_method)wavesel_click, 
        gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_column, 
        gensym("column"), A_SYMBOL, A_FLOAT, A_FLOAT, A_FLOAT,A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_rect, 
        gensym("rect"), A_SYMBOL, A_FLOAT, A_FLOAT, A_FLOAT,A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_color, 
        gensym("color"), A_SYMBOL, A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_deletenum, 
        gensym("delete"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setmode, 
        gensym("setmode"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_state, 
        gensym("state"), 0);
    class_addmethod(wavesel_class, (t_method)wavesel_redraw_all, 
        gensym("redraw_all"), 0);
    class_addmethod(wavesel_class, (t_method)wavesel_move_element, 
        gensym("move_element"), A_FLOAT, 0);
        
// hammergui
    class_addmethod(wavesel_class, (t_method)wavesel_doup, 
        gensym("_up"), A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_doelse, 
        gensym("_bang"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_doelse,
        gensym("_zero"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_doelse,
        gensym("_poll"), A_FLOAT, A_FLOAT, 0);

// GUI behaviour
    wavesel_setwidget();
    class_setwidget(wavesel_class, &wavesel_widgetbehavior);

// resize handle in edit mode    
    waveselhandle_class = class_new(gensym("_waveselhandle"), 0, 0,
        sizeof(t_waveselhandle), CLASS_PD, 0);
    class_addmethod(waveselhandle_class, 
        (t_method)waveselhandle__clickhook, gensym("_click"), A_FLOAT, 0);
    class_addmethod(waveselhandle_class, 
    (t_method)waveselhandle__motionhook, gensym("_motion"), 
        A_FLOAT, A_FLOAT, 0);
    
    post("wavesel~ 0.6 , a waveform~ wannabe, that isn't there yet...");
    post("fjkraan@xs4all.nl, 2015-07-08");
}


