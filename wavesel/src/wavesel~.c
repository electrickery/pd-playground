/* wavesel, cheap waveform knock-off, based on gcanvas from Guenter Geiger
 * Fred Jan Kraan, fjkraan@xs4all.nl, 2015-06 */


#include "m_pd.h"
#include "g_canvas.h"
#include "s_stuff.h"
#include "hammer/gui.h"  // just for the mouse-up, silly!

/* ------------------------ wavesel ----------------------------- */


#define DEFAULTSIZE 80
#define TAGBUFFER   64
#define MILLIS      0.001
#define ZOOMFACTOR  0.05f
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
#define MAX5BACKGROUND "#d7bd6e"
#define MAX5SELECTEDBACKGROUND "6b5e37"

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
#define WAVESELHANDLE_COLOR  ""
#define WAVESEL_GRIDWIDTH    0.9
#define WAVESEL_MINWIDTH     42
#define WAVESEL_MINHEIGHT    22
#define WAVESEL_MAXWIDTH     2050
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
static void wavesel_drawbase(t_wavesel* x);
void wavesel_drawme(t_wavesel *x, t_glist *glist, int firsttime);
static void wavesel_draw_foreground(t_wavesel* x);
static void wavesel_draw_columns(t_wavesel* x);
static void wavesel_setarray(t_wavesel *x, t_symbol *s);
static void wavesel_vis(t_gobj *z, t_glist *glist, int vis);

static void wavesel_setmode(t_wavesel *x, t_float mode)
{
    if (mode < 0 || mode > 4) return;
    x->mode = (int)mode;
    if (x->mode == 4)
        post("wavesel: mode %d not implemented yet", x->mode);
}

static t_canvas *wavesel_getcanvas(t_wavesel *x, t_glist *glist)
{
    if (glist != x->x_glist)
    {
	post("wavesel_getcanvas: glist error");
	x->x_glist = glist;
    }
    return (x->canvas = glist_getcanvas(glist));
}

static t_canvas *wavesel_isvisible(t_wavesel *x)
{
    return (glist_isvisible(x->x_glist) ? x->canvas : 0);
}

static void wavesel_revis(t_wavesel *x)
{
    sys_vgui(".x%lx.c delete %s\n", x->canvas, x->canvas_allTag);

    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_create_rectangle(void* cv, void* o, int c, int x, 
    int y, int w, int h, char* color, char* outlinecolor) 
{
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lx%d -fill %s -outline %s\n",
                cv,                    x, y, x+w,y+h,    o, c,   color, outlinecolor);
}

static void wavesel_move_object(void* cv, void* x, int c, int xx, int y,
    int w,int h) {
    sys_vgui(".x%lx.c coords %lx%d %d %d %d %d\n",
                cv,          x, c, xx, y, xx+w,y+h);
    sys_pollgui();
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
    t_element* e = x->canvas_element[num];
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
    t_element* e = x->canvas_element[num];
    wavesel_move_object(
        glist_getcanvas(x->x_glist), x, num,
        x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
        e->w, e->h);
}

static void wavesel_set_canvas_list(t_wavesel *x)
{
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_VIEW_START],   (t_float)x->canvas_startForeground);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_VIEW_END],     (t_float)x->canvas_endForeground);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_SELECT_START], (t_float)x->canvas_startcursor);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_SELECT_END],   (t_float)x->canvas_endcursor);
}

static void wavesel_set_mouse_list(t_wavesel *x)
{
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_X],   (t_float)x->canvas_x);
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_Y],   (t_float)x->canvas_y);
//    SETFLOAT(&x->outlet_mouse[MOUSE_STATE],   0.f);
}
static void wavesel_move_background_selection(t_wavesel *x, int xx, 
    int w)
{
    t_element* e = x->canvas_element[x->canvas_selectBackground];
    e->x = xx;
    e->w = w;
    wavesel_move_object(glist_getcanvas(x->x_glist), x, x->canvas_selectBackground,
        x->x_obj.te_xpix + e->x, x->x_obj.te_ypix + e->y,
        e->w, e->h);
}


static void wavesel_delete_element(t_wavesel *x, int num)
{
    wavesel_delete_object(glist_getcanvas(x->x_glist),x,num); 
}


static void wavesel_color_element(t_wavesel* x, int num, char* color)
{
    t_element* e = x->canvas_element[num];
    e->color = color;
    wavesel_color_object(glist_getcanvas(x->x_glist), x, num, color); 
}


static void wavesel_color_rect_element(t_wavesel* x, int num, char* color,
    char* outlinecolor)
{
    t_element* e = x->canvas_element[num];
    e->color = color;
    e->outlinecolor = outlinecolor;
    wavesel_color_rect_object(glist_getcanvas(x->x_glist), x, num, color, outlinecolor); 
}

/* widget helper functions */

void wavesel_drawme(t_wavesel *x, t_glist *glist, int firsttime)
{
    int i;

    for (i = 0; i < x->canvas_numelem; i++)
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
	    int onset = x->x_obj.te_xpix + (x->canvas_width - IOWIDTH) * i / 
            nplus;
	    if (firsttime)
		sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxo%d\n",
		    glist_getcanvas(glist),
		    onset, x->x_obj.te_ypix + x->canvas_height - WAVESEL_XLETHEIGHT,
		    onset + IOWIDTH, x->x_obj.te_ypix + x->canvas_height,
		    x, i);
	    else
		sys_vgui(".x%lx.c coords %lxo%d %d %d %d %d\n",
		     glist_getcanvas(glist), x, i,
		     onset, x->x_obj.te_ypix + x->canvas_height - WAVESEL_XLETHEIGHT,
		     onset + IOWIDTH, x->x_obj.te_ypix + x->canvas_height);
	}
	/* inlets */
	n = 1; 
	nplus = (n == 1 ? 1 : n-1);
	for (i = 0; i < n; i++)
	{
	    int onset = x->x_obj.te_xpix + (x->canvas_width - IOWIDTH) * i / 
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

    for (i = 0; i < x->canvas_numelem; i++)
         wavesel_delete_element(x, i);

     n = 3;
     while (n--) {
         sys_vgui(".x%lx.c delete %lxo%d\n", 
             glist_getcanvas(glist), x, n);
     }
     x->canvas_numelem = 0;
     if (x->system_clockRunning)
         clock_free(x->system_clock);
     hammergui_unbindmouse((t_pd *)x);
}

static void wavesel_reset_selection(t_wavesel* x) ;

/* ------------------ wavesel widgetbehaviour----------------------- */


static void wavesel_getrect(t_gobj *z, t_glist *owner,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    int width, height;
    t_wavesel* s = (t_wavesel*)z;

    width = s->canvas_width;
    height = s->canvas_height;
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
//        wavesel_erase(x, x->x_glist);
        
        wavesel_drawbase(x);
        wavesel_draw_foreground(x);
        wavesel_setarray(x, x->array_name);
        wavesel_draw_columns(x);
}

static void wavesel_deselect(t_wavesel* x, t_canvas *cv, 
    t_waveselhandle *wsh)
{
    sys_vgui(".x%lx.c itemconfigure %s -outline black -width %f\
        -fill #%2.2x000000\n", cv, x->canvas_backgroundTag, WAVESEL_GRIDWIDTH);
    sys_vgui("destroy %s\n", wsh->h_pathname);
    wsh->h_selectedmode = 0;  
}

// canvas is selected in edit mode
static void wavesel_select(t_gobj *z, t_glist *glist, int state)
{
    t_wavesel *x = (t_wavesel *)z;
     
// handle start     
    t_canvas *cv = wavesel_getcanvas(x, glist);
    t_waveselhandle *wsh = (t_waveselhandle *)x->editHandle;
post("select: selected");        
    if (state)
    {
        if (wsh->h_selectedmode) // prevent re-selection
            return;

        int x1, y1, x2, y2;
        wavesel_getrect(z, glist, &x1, &y1, &x2, &y2);

        sys_vgui(".x%lx.c itemconfigure %s -outline blue -width %f -fill %s\n",
            cv, x->canvas_backgroundTag, WAVESEL_SELBDWIDTH, WAVESEL_SELCOLOR);
        sys_vgui("canvas %s -width %d -height %d -bg #fedc00 -bd 0\n",
            wsh->h_pathname, WAVESELHANDLE_WIDTH, WAVESELHANDLE_HEIGHT);
            sys_vgui(".x%lx.c create window %f %f -anchor nw\
            -width %d -height %d -window %s -tags %s\n",
             cv, x2 - (WAVESELHANDLE_WIDTH - WAVESEL_SELBDWIDTH),
             y2 - (WAVESELHANDLE_HEIGHT - WAVESEL_SELBDWIDTH),
            WAVESELHANDLE_WIDTH, WAVESELHANDLE_HEIGHT,
            wsh->h_pathname, x->canvas_allTag);
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
post("select: deselected");        
        wavesel_deselect(x, cv, wsh);
        wavesel_vis(x, x->x_glist, 1);
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
    t_waveselhandle *wsh = (t_waveselhandle *)x->editHandle;
    
    if (wsh->h_selectedmode)
        return;
        
    if (!x->canvas_numelem)
        return;
    
    sprintf(wsh->h_pathname, ".x%lx.h%lx", (unsigned long)cv, 
        (unsigned long)wsh);
    if (vis)
    {
        x->canvas_element[FRAME]->h = x->canvas_height;
        x->canvas_element[FRAME]->w = x->canvas_width;
        wavesel_drawme(x, glist, 1);
    }
    else
    {
        wavesel_erase(x, glist);
        post("vis: erase");
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
             wavesel_color_element(x, i + x->canvas_startForeground, 
                 x->canvas_foregroundSelectedColor->s_name);
        else
             wavesel_color_element(x, i + x->canvas_startForeground, 
                 x->canvas_foregroundColor->s_name);
    }
    // background selected rectangle
    wavesel_color_rect_element(x, x->canvas_selectBackground, x->canvas_backgroundSelectedColor->s_name,
        x->canvas_backgroundSelectedColor->s_name);
    wavesel_move_background_selection(x, x->canvas_startcursor, 
        x->canvas_endcursor - x->canvas_startcursor);
}

static void wavesel_color_all_columns(t_wavesel *x)
{
    int size = x->canvas_endForeground - x->canvas_startForeground;
    if (x->canvas_endcursor == 0)
        wavesel_reset_selection(x);
    else {
        wavesel_color_columns(x, 0, x->canvas_startcursor, 0);
        wavesel_color_columns(x, x->canvas_startcursor, x->canvas_endcursor, 1);
        wavesel_color_columns(x, x->canvas_endcursor, size, 0);
    }
}

static void wavesel_motion_selectmode(t_wavesel *x, t_floatarg dx)
{   
    x->canvas_x += dx;
    x->canvas_x = ((int)(x->canvas_x)  < 0) ? x->canvas_startcursor : x->canvas_x;
    x->canvas_x = ((int)(x->canvas_x + 0.5f) > x->canvas_endForeground) ? x->canvas_endForeground : x->canvas_x;

    if (x->canvas_x > x->canvas_clickOrigin) // right side
    {
        wavesel_color_columns(x, x->canvas_startcursor, x->canvas_clickOrigin, 0);
        wavesel_color_columns(x, x->canvas_clickOrigin, (int)(x->canvas_x), 1);
        wavesel_color_columns(x, (int)(x->canvas_x), x->canvas_endcursor, 0);
        x->canvas_startcursor = x->canvas_clickOrigin;
        x->canvas_endcursor   = (int)(x->canvas_x);
        outlet_float(x->out4, x->canvas_x * x->outlet_outputFactor);
    }
    else //left side
    { 
        wavesel_color_columns(x, x->canvas_clickOrigin, x->canvas_endcursor, 0);
        wavesel_color_columns(x, x->canvas_x, x->canvas_clickOrigin, 1);
        wavesel_color_columns(x, x->canvas_startcursor, x->canvas_x, 0);
        x->canvas_startcursor = (int)(x->canvas_x);
        x->canvas_endcursor   = x->canvas_clickOrigin;
        outlet_float(x->out3, x->canvas_x * x->outlet_outputFactor);
    }   
}

static void wavesel_motion_loopmode(t_wavesel *x, t_floatarg dx, 
    t_floatarg dy)
{
    int i;
    int size = x->canvas_endForeground - x->canvas_startForeground + 2; // rationalize this offset
    int dix = (int)dx;
    int diy = (int)dy;
    int dstart = dix + diy;
    int dend   = dix - diy;
    int oldStartCursor = x->canvas_startcursor;
    int oldEndCursor = x->canvas_endcursor;
    x->canvas_startcursor += dstart;
    x->canvas_endcursor   += dend;

    // check boundaries and internal consistency    
    if (x->canvas_startcursor > x->canvas_endcursor) {
        int tmp = (int)((x->canvas_startcursor + x->canvas_endcursor) / 2.f);
        x->canvas_startcursor = tmp;
        x->canvas_endcursor   = tmp;
    }
    
    x->canvas_startcursor = ((int)(x->canvas_startcursor)  < 0) ? 0: x->canvas_startcursor;
    x->canvas_startcursor = ((int)(x->canvas_startcursor + 0.f) > size) ? size : x->canvas_startcursor;
    x->canvas_endcursor = ((int)(x->canvas_endcursor)  < 0) ? 0 : x->canvas_endcursor;
    x->canvas_endcursor = ((int)(x->canvas_endcursor + 0.5f) > size) ? size : x->canvas_endcursor;

    // minimize the range to update
    int startUpdateRange = (x->canvas_startcursor > oldStartCursor) ? 
        oldStartCursor : x->canvas_startcursor;
    int endUpdateRange   = (x->canvas_endcursor > oldEndCursor) ? 
        x->canvas_endcursor : oldEndCursor;
 
    for (i = startUpdateRange; i <= endUpdateRange; i++)
    {
        if (i < x->canvas_startcursor || i > x->canvas_endcursor)
            wavesel_color_element(x, i + x->canvas_startForeground, 
                x->canvas_foregroundColor->s_name);
        else
            wavesel_color_element(x, i + x->canvas_startForeground, 
                x->canvas_foregroundSelectedColor->s_name);
    }
    wavesel_move_background_selection(x, x->canvas_startcursor, 
        x->canvas_endcursor - x->canvas_startcursor);

    outlet_float(x->out3, (x->canvas_startcursor * x->outlet_outputFactor));
    outlet_float(x->out4, (x->canvas_endcursor * x->outlet_outputFactor));
}

static void wavesel_motion_zoom_move_mode(t_wavesel *x, t_float dx, t_float dy)
{
    int range = x->canvas_arrayViewEnd - x->canvas_arrayViewStart;
    
    if (x->canvas_samplesPerColumn < MAXZOOM && dy < 0)
        return;
    
    x->canvas_arrayViewStart -= range * dy * ZOOMFACTOR;
    x->canvas_arrayViewEnd   += range * dy * ZOOMFACTOR;
    
    if (x->canvas_arrayViewStart > 0 && x->canvas_arrayViewEnd < x->array_size)
    {
        x->canvas_arrayViewStart -= range * dx * ZOOMFACTOR;
        x->canvas_arrayViewEnd   -= range * dx * ZOOMFACTOR;
    }

    x->canvas_arrayViewStart = (x->canvas_arrayViewStart < 0) 
        ? 0 : x->canvas_arrayViewStart;
    x->canvas_arrayViewStart = (x->canvas_arrayViewStart > x->array_size) 
        ? x->array_size : x->canvas_arrayViewStart;
    x->canvas_arrayViewEnd   = (x->canvas_arrayViewEnd > x->array_size) 
        ? x->array_size : x->canvas_arrayViewEnd;
    x->canvas_arrayViewEnd   = (x->canvas_arrayViewEnd < 0) 
        ? 0 : x->canvas_arrayViewEnd;
    
    wavesel_arrayZoom(x);
}

static void wavesel_motion(t_wavesel *x, t_floatarg dx, t_floatarg dy)
{
    if (x->canvas_x >= 0.f && x->canvas_x <= (t_float)(x->canvas_width - 2.f))
    {
        switch (x->mode) {
        case MODE_NONE:
            break; 
        case MODE_SELECT:
            wavesel_motion_selectmode(x, dx);
            break;
        case MODE_LOOP:
            wavesel_motion_loopmode(x, dx, dy);
            break;
        case MODE_MOVE:
            wavesel_motion_zoom_move_mode(x, dx, dy);
            break;
        case MODE_DRAW:
             break;
        }
        outlet_list(x->out6, 0, 4, x->outlet_canvas);
        wavesel_set_mouse_list(x);
        SETFLOAT(&x->outlet_mouse[MOUSE_LIST_STATE], 2.f);
        outlet_list(x->out5, 0, 3, x->outlet_mouse);
    }
    wavesel_drawme(x, x->x_glist, 0);
    wavesel_draw_columns(x);
    wavesel_set_canvas_list(x);
}

void wavesel_key(t_wavesel *x, t_floatarg f)
{
//  post("key");
  if (x || f) {}
}

static void wavesel_click_selectmode(t_wavesel *x)
{
    wavesel_reset_selection(x);
    
    outlet_float(x->out3, x->canvas_x * x->outlet_outputFactor);
    outlet_float(x->out4, x->canvas_x * x->outlet_outputFactor);
    
    wavesel_color_element(x, (int)x->canvas_x + x->canvas_startForeground, 
        x->canvas_foregroundSelectedColor->s_name);
        
    wavesel_color_rect_element(x, x->canvas_selectBackground, 
        x->canvas_backgroundSelectedColor->s_name, x->canvas_backgroundSelectedColor->s_name);
    wavesel_move_background_selection(x, (int)x->canvas_x, 0);

    x->canvas_startcursor = (int)(x->canvas_x + 0.5f);
    x->canvas_endcursor   = (int)(x->canvas_x + 0.5f);
    x->canvas_clickOrigin = (int)(x->canvas_x + 0.5f);
}

static void wavesel_click_loopmode(t_wavesel *x)
{
    int selectedRange = (x->canvas_endcursor - x->canvas_startcursor) / 2;
    int columnCount = x->canvas_endForeground - x->canvas_startForeground;
    if (!x->canvas_endcursor) // if no selection
        wavesel_click_selectmode(x);
    else
    {
        wavesel_reset_selection(x);
        x->canvas_startcursor = x->canvas_x - selectedRange;
        x->canvas_endcursor = x->canvas_x + selectedRange;
        x->canvas_startcursor = (x->canvas_startcursor < 0) ? 0 : x->canvas_startcursor;
        x->canvas_endcursor = (x->canvas_endcursor > columnCount) ? 
            columnCount : x->canvas_endcursor;

        wavesel_color_all_columns(x);
    }
}

static void wavesel_click(t_wavesel *x,
    t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl,
    t_floatarg doit, int up)
{
    glist_grab(x->x_glist, &x->x_obj.te_g,
        (t_glistmotionfn)wavesel_motion, (t_glistkeyfn)NULL, xpos, ypos);

    x->canvas_x = xpos - x->x_obj.te_xpix;
    x->canvas_y = ypos - x->x_obj.te_ypix;
 
    if (x->canvas_x > 0 && x->canvas_x < x->canvas_width -2 && x->canvas_y > 0 && 
        x->canvas_y < x->canvas_height)
    {
        switch (x->mode) {
        case MODE_NONE:
            break; 
        case MODE_SELECT:
            wavesel_click_selectmode(x);
            break;
        case MODE_LOOP:
            wavesel_click_loopmode(x);
            break;
        case MODE_MOVE:
        case MODE_DRAW:
            break;
        }
        wavesel_set_canvas_list(x);
        outlet_list(x->out6, 0, 4, x->outlet_canvas);
        wavesel_set_mouse_list(x);
        SETFLOAT(&x->outlet_mouse[MOUSE_LIST_STATE], 1.f);
        outlet_list(x->out5, 0, 3, x->outlet_mouse);
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
    if (z || glist || state) {} // eliminate compiler warnings
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
    x->canvas_element[x->canvas_numelem] = e;
    
    e->type = RECT;
    e->x = xp;
    e->y = y;
    e->w = w;
    e->h = h;
    e->color = c->s_name;
    wavesel_draw_element(x, x->canvas_numelem);
    x->canvas_numelem++;
}

static void wavesel_column(t_wavesel* x, t_symbol* c, float xp, float y, 
    float w, float h)
{
    if (x->canvas_numelem < MAXELEM-1) {
        t_element* e = getbytes(sizeof(t_element));
        x->canvas_element[x->canvas_numelem] = e;
        
        e->type = LINE;
        e->x = xp;
        e->y = y;
        e->w = w;
        e->h = h;
        e->g = 1;
        e->color = c->s_name;
        wavesel_draw_element(x, x->canvas_numelem);
        x->canvas_numelem++;
    }
}

static void wavesel_color(t_wavesel* x, t_symbol* c, float num)
{
    wavesel_color_element(x,(int)num,c->s_name);
}


void wavesel_deletenum(t_wavesel* x,float num)
{
    int i = (int) num;
    if (x->canvas_element[i]) {
        wavesel_delete_element(x,i);
        freebytes(x->canvas_element[i],sizeof(t_element));
        x->canvas_element[i] = NULL;
    }
}

// find extremes in the sample range represented by this column
static void wavesel_get_column_size(t_wavesel* x, int column, 
    t_word *vec)
{
    int sampleOffset = x->canvas_arrayViewStart + 
        (int)(column * x->canvas_samplesPerColumn);
    t_float upper = 0.f, lower = 0.f;
    if (x->canvas_samplesPerColumn > 1.f) {
        int sampleEndPoint = sampleOffset + (int)x->canvas_samplesPerColumn;
        int i;
        for (i = sampleOffset; i < sampleEndPoint; i++) 
        {
            upper = (upper < (vec[i].w_float)) ? vec[i].w_float : upper;
            lower = (lower > (vec[i].w_float)) ? vec[i].w_float : lower;
        }
    }
    else
    {
        if (vec[sampleOffset].w_float > 0)
            upper = vec[sampleOffset].w_float;
        else
            lower = vec[sampleOffset].w_float;
    }
    x->canvas_columnTop = upper;
    x->canvas_columnBottom = lower;
}

static void wavesel_draw_columns(t_wavesel* x)
{
    if (!x->array) 
        return;
    t_garray *ax = x->array;
    t_word *vec;
    int size;
    if (!garray_getfloatwords(ax, &size, &vec))
    {
        post("wavesel: couldn't read from array!");
        return;
    }
    int column;
    int middle = (int)(x->canvas_height / 2);
    int columnCount = x->canvas_endForeground - x->canvas_startForeground;
    for (column = 0; column <= columnCount; column++)
    {
        wavesel_get_column_size(x, column, vec);
        t_float upper = (int)(x->canvas_columnTop * x->canvas_height * x->canvas_columnGain);
        t_float lower = (int)(x->canvas_columnBottom * x->canvas_height * x->canvas_columnGain); // + 1 ??
        t_float maxSize = 0.5f * x->canvas_height;
        
        if (x->canvas_clipmode) 
        {
             upper = (upper >  maxSize) ?  maxSize : upper;
             lower = (lower < -maxSize) ? -maxSize : lower;
        }
        x->canvas_element[column + x->canvas_startForeground]->y = 
            (int)(middle - upper);
        x->canvas_element[column + x->canvas_startForeground]->h = 
            (int)(upper - lower);
    }
}

static void wavesel_draw_foreground(t_wavesel* x)
{
    int column;
    int e = x->canvas_numelem;
    int middle = (int)(x->canvas_height / 2);
    x->canvas_startForeground = x->canvas_numelem;
    for (column = 1; column < x->canvas_width; column++)
    {
        x->canvas_element[e] = getbytes(sizeof(t_element));
        x->canvas_element[e]->type = LINE;
        x->canvas_element[e]->x = column;
        x->canvas_element[e]->y = middle;
        x->canvas_element[e]->w = 0;
        x->canvas_element[e]->h = 1;
        x->canvas_element[e]->color = x->canvas_foregroundColor->s_name;
        x->canvas_numelem++;
        e++;
    }
    x->canvas_endForeground = x->canvas_width - 2;
}

static void wavesel_reset_selection(t_wavesel* x) 
{
    if (x->canvas_startForeground == 0) return;
    int i;
    for (i = x->canvas_startForeground; i <= x->canvas_endForeground; i++)
        wavesel_color_element(x, i, x->canvas_foregroundColor->s_name);
        
    wavesel_color_rect_element(x, x->canvas_selectBackground, x->canvas_backgroundColor->s_name, 
        x->canvas_backgroundColor->s_name);
    wavesel_move_background_selection(x, x->canvas_selectBackground, 0);
    x->canvas_startcursor = 0;
    x->canvas_endcursor = 0;
}

static void wavesel_drawbase(t_wavesel* x) 
{
    if (x->canvas_numelem)
        wavesel_erase(x, x->x_glist);
    t_symbol* selectColor = x->canvas_backgroundColor;
    int start = 1;
    int width = 0;
    if (x->canvas_endcursor)
    {
        selectColor = x->canvas_backgroundSelectedColor;
        start = x->canvas_startcursor;
        width = x->canvas_endcursor - x->canvas_startcursor;
    }

    // outline box
    int e = x->canvas_numelem;
    x->canvas_element[e] = getbytes(sizeof(t_element));
    x->canvas_element[e]->type = RECT;
    x->canvas_element[e]->x = 0;
    x->canvas_element[e]->y = 0;
    x->canvas_element[e]->w = x->canvas_width;
    x->canvas_element[e]->h = x->canvas_height;
    x->canvas_element[e]->color = x->canvas_backgroundColor->s_name;
    x->canvas_element[e]->outlinecolor = BORDER;
    x->canvas_numelem++;
    
    // background selection rectangle
    e = x->canvas_numelem;
    x->canvas_selectBackground = e;
    x->canvas_element[e] = getbytes(sizeof(t_element));
    x->canvas_element[e]->type = RECT;
    x->canvas_element[e]->x = start;
    x->canvas_element[e]->y = 1;
    x->canvas_element[e]->w = width;
    x->canvas_element[e]->h = x->canvas_height - 3;
    x->canvas_element[e]->color = selectColor->s_name;
    x->canvas_element[e]->outlinecolor = selectColor->s_name;
    x->canvas_numelem++;
}
  
static void wavesel_state(t_wavesel* x) 
{
    post(" --==## wavesel_state ##==--");
    post("canvas_width: %d",           x->canvas_width);
    post("canvas_height: %d",          x->canvas_height);
    post("canvas_numelem: %d",         x->canvas_numelem);
    post("canvas_arrayViewStart: %d",  x->canvas_arrayViewStart);
    post("canvas_arrayViewEnd: %d",    x->canvas_arrayViewEnd);
    post("canvas_startcursor (relative to canvas): %d",     x->canvas_startcursor);
    post("canvas_endcursor (relative to canvas): %d",       x->canvas_endcursor);
    post("selected bg width: %d", x->canvas_endcursor - x->canvas_startcursor);
    post("canvas_startfg (absolute in element array): %d",  x->canvas_startForeground);
    post("canvas_endfg (absolute in element array): %d",    x->canvas_endForeground);
    post("canvas_startbg (absolute in element array): %d",  x->canvas_selectBackground);
    post("array: %s",          (x->array) ? "defined" : "null");
    post("arrayname: %s",       x->array_name->s_name);
    post("arraysize: %d",       x->array_size);
    post("canvas_samplesPerColumn: %f", x->canvas_samplesPerColumn);
    post("canvas_vzoom: %f",           x->canvas_columnGain);
    post("system_ksr: %f",             x->system_ksr);
    post("outlet_outputFactor: %f",    x->outlet_outputFactor);
    post("canvas_mode: %d",            x->mode);
    post("array_bufRefresh: %lf",     x->array_viewRefresh);
    post("system_clockRunning: %d",    x->system_clockRunning);
    post("canvas_linePosition: %f",    x->canvas_linePosition);
    post("canvas_clipmode: %d",        x->canvas_clipmode);
    post("-- base rectangle:");
    post(" x: %d", x->canvas_element[0]->x);
    post(" y: %d", x->canvas_element[0]->y);
    post(" w: %d", x->canvas_element[0]->w);
    post(" h: %d", x->canvas_element[0]->h);
    post(" color: %s", x->canvas_element[0]->color);
    post(" outlinecolor: %s", x->canvas_element[0]->outlinecolor);
    post("-- background select rectangle:");
    post(" x: %d", x->canvas_element[x->canvas_selectBackground]->x);
    post(" y: %d", x->canvas_element[x->canvas_selectBackground]->y);
    post(" w: %d", x->canvas_element[x->canvas_selectBackground]->w);
    post(" h: %d", x->canvas_element[x->canvas_selectBackground]->h);
    post(" color: %s", x->canvas_element[x->canvas_selectBackground]->color);
    post(" outlinecolor: %s", x->canvas_element[x->canvas_selectBackground]->outlinecolor);
}

static void wavesel_setarray(t_wavesel *x, t_symbol *s)
{
    t_garray *array;
    x->array_name = s;
    
    if ((array = (t_garray *)pd_findbyclass(x->array_name, 
        garray_class)))
    {
        x->array = array;
        x->array_size = garray_npoints(x->array);
        x->canvas_samplesPerColumn = (t_float)x->array_size / 
            (t_float)x->canvas_width;
        x->outlet_outputFactor = x->canvas_samplesPerColumn / x->system_ksr;
        x->canvas_arrayViewStart = 0;
        x->canvas_arrayViewEnd   = x->array_size;
    } else {
        post("wavesel: no array \"%s\" (error %d)", 
            x->array_name->s_name, array);
        x->array = 0;
        x->array_size = 0;
        return;
    }
    outlet_float(x->out1, 0);
    outlet_float(x->out2, x->array_size / x->system_ksr);
    outlet_float(x->out3, x->canvas_startcursor * x->outlet_outputFactor);
    outlet_float(x->out4, x->canvas_endcursor   * x->outlet_outputFactor);
    wavesel_draw_columns(x);
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_arrayZoom(t_wavesel *x)
{
    int arrayViewSize = x->canvas_arrayViewEnd - x->canvas_arrayViewStart;
    
    x->canvas_samplesPerColumn = arrayViewSize / (t_float)x->canvas_width;
    x->outlet_outputFactor = x->canvas_samplesPerColumn / x->system_ksr;
    
    wavesel_draw_columns(x);
    wavesel_drawme(x, x->x_glist, 0);
    
    outlet_float(x->out1, x->canvas_arrayViewStart / x->system_ksr);
    outlet_float(x->out2, x->canvas_arrayViewEnd / x->system_ksr);
    outlet_float(x->out3, x->canvas_startcursor * x->outlet_outputFactor);
    outlet_float(x->out4, x->canvas_endcursor   * x->outlet_outputFactor);
    
    
}

static void wavesel_setForegroundColor(t_wavesel *x, t_symbol *s)
{
    x->canvas_foregroundColor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setBackgroundColor(t_wavesel *x, t_symbol *s)
{
    x->canvas_backgroundColor = s;
    x->canvas_element[0]->color = s->s_name;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setfForegroundSelectColor(t_wavesel *x, t_symbol *s)
{
    x->canvas_foregroundSelectedColor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_setBackgroundSelectColor(t_wavesel *x, t_symbol *s)
{
    x->canvas_backgroundSelectedColor = s;
    wavesel_drawme(x, x->x_glist, 0);
}

static void wavesel_buftime(t_wavesel* x, float fRefresh)
{
    x->array_viewRefresh = (double)(fRefresh < 0) ? WAVESEL_BUFREFRESH : 
        fRefresh;
    clock_delay(x->system_clock, x->array_viewRefresh);
}

static void wavesel_tick(t_wavesel *x)
{
    x->system_clockRunning = 0;
    if (x->array_viewRefresh == 0)
        return;
//    if (!x->x_mouseDown)
//    {
        wavesel_draw_columns(x);
//    }
    clock_delay(x->system_clock, x->array_viewRefresh);
    x->system_clockRunning = 1;
}

static void waveselhandle__clickhook(t_waveselhandle *wsh, t_floatarg f)
{
    int newstate = (int)f;
    if (wsh->h_dragon && newstate == 0)
    {
	t_wavesel *x = wsh->h_master;
	t_canvas *cv;
	x->canvas_width += wsh->h_dragx;
	x->canvas_height += wsh->h_dragy;
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
        x->canvas_width  = x2 - x1;
        x->canvas_height = y2 - y1;
    }
}

static void *wavesel_line(t_wavesel *x, t_float f)
{
    x->canvas_linePosition = f;
    wavesel_reset_selection(x);
    int size = x->canvas_endForeground - x->canvas_startForeground;
    int column = (int)((x->canvas_linePosition / x->canvas_samplesPerColumn) * 
        x->system_ksr + 0.5);
        
    if (column >= 0 && column <= size) {
        wavesel_color_element(x, column + x->canvas_startForeground, 
            x->canvas_foregroundSelectedColor->s_name);
            
        wavesel_move_background_selection(x, column, 1);
        wavesel_color_rect_element(x, x->canvas_selectBackground, 
            x->canvas_backgroundSelectedColor->s_name, x->canvas_backgroundSelectedColor->s_name);
    }
    return 0;
}

static void *wavesel_clipdraw(t_wavesel *x, t_float f)
{
    x->canvas_clipmode = (int)(f < 0) ? 0 : f;
    return 0;
}

static void *wavesel_doup(t_wavesel *x, t_float f)
{
    x->canvas_mouseDown = (int)(f > 0) ? 0 : 1;
//    wavesel_set_mouse_list(x);
//    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_STATE], 0.f);
//    outlet_list(x->out5, 0, 3, x->outlet_mouse);

    return 0;
}

static void *wavesel_vzoom(t_wavesel *x, t_float f)
{
    x->canvas_columnGain = (f > 0) ? f : 1;
    
    wavesel_drawme(x, x->x_glist, 0);
    wavesel_draw_columns(x);
}

static void *wavesel_loadbang(t_wavesel *x)
{
    if (!sys_noloadbang)
    {
        post("wavesel_loadbang: !sys_noloadbang");
        if (!x->array && x->array_name->s_name[0] != 0)
            wavesel_setarray(x, x->array_name);
    }
    return 0;
}

// arguments to add: setmode, buftime, fgcolor, bgcolor, fgselcolor, bgselcolor, 
static void *wavesel_new(t_symbol* s, t_float w, t_float h)
{
    int i;
    t_wavesel *x = (t_wavesel *)pd_new(wavesel_class);

    x->x_glist = (t_glist*) canvas_getcurrent();
    x->system_ksr   = sys_getsr() * MILLIS;
    x->outlet_outputFactor = 1.; // used for outlet unit calculation
    x->mode  = 0;
    x->canvas_mouseDown = 0;
    
    for (i=0;i<MAXELEM;i++)
        x->canvas_element[i] = NULL;
    x->canvas_numelem   = 0;
    x->array_name = s;
    x->array     = 0;
        
    x->array_viewRefresh = (double)WAVESEL_BUFREFRESH;
    x->system_clock = clock_new(x, (t_method)wavesel_tick); // get a clock
    clock_delay(x->system_clock, x->array_viewRefresh); // start it
    x->system_clockRunning = 1;

    x->canvas_width = DEFAULTSIZE;
    if ((int)w > WAVESEL_MINWIDTH) 
        x->canvas_width = w;

    x->canvas_height = DEFAULTSIZE;
    if ((int)h > WAVESEL_MINHEIGHT)
        x->canvas_height = h;

    x->canvas_arrayViewStart = 0;
    x->canvas_arrayViewEnd   = 0;
    x->canvas_startcursor = 0;
    x->canvas_endcursor   = 0;
    x->canvas_foregroundColor = gensym(FGCOLOR);
    x->canvas_backgroundColor = gensym(BGCOLOR);
    x->canvas_foregroundSelectedColor = gensym(FGSELCOLOR);
    x->canvas_backgroundSelectedColor = gensym(BGSELCOLOR);
    x->canvas_columnGain = 1;
    wavesel_drawbase(x);
    wavesel_draw_foreground(x);
    wavesel_draw_columns(x);

    x->out1 = outlet_new(&x->x_obj, &s_float);
    x->out2 = outlet_new(&x->x_obj, &s_float);
    x->out3 = outlet_new(&x->x_obj, &s_float);
    x->out4 = outlet_new(&x->x_obj, &s_float);
    x->out5 = outlet_new(&x->x_obj, &s_list);
    x->out6 = outlet_new(&x->x_obj, &s_list);
    
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_X],     0.f);
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_Y],     0.f);
    SETFLOAT(&x->outlet_mouse[MOUSE_LIST_STATE], 0.f);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_VIEW_START],   0.f);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_VIEW_END],     0.f);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_SELECT_START], 0.f);
    SETFLOAT(&x->outlet_canvas[CANVAS_LIST_SELECT_END],   0.f); 
    
// handle start  
    t_waveselhandle *wsh;
    char handlebuf[TAGBUFFER];    
    x->editHandle = pd_new(waveselhandle_class);
    wsh = (t_waveselhandle *)x->editHandle;
    wsh->h_master = x;
    sprintf(handlebuf, "_h%lx", (unsigned long)wsh);
    pd_bind(x->editHandle, wsh->h_bindsym = gensym(handlebuf));
    sprintf(wsh->h_outlinetag, "h%lx", (unsigned long)wsh);
    wsh->h_dragon = 0;
    
    sprintf(x->canvas_allTag,     "all%lx", (unsigned long)x);
    sprintf(x->canvas_backgroundTag,   "bg%lx",  (unsigned long)x);
    sprintf(x->canvas_gridTag, "gr%lx",  (unsigned long)x);
    sprintf(x->canvas_foregroundTag,   "fg%lx",  (unsigned long)x);
    
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
    class_addmethod(wavesel_class, (t_method)wavesel_vzoom, 
        gensym("vzoom"), A_FLOAT, 0);

// wavesel specific
    class_addmethod(wavesel_class, (t_method)wavesel_setarray, 
        gensym("setarray"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setForegroundColor, 
        gensym("setfgcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setBackgroundColor, 
        gensym("setbgcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setfForegroundSelectColor, 
        gensym("setfgselcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_setBackgroundSelectColor, 
        gensym("setbgselcolor"), A_SYMBOL, 0);
    class_addmethod(wavesel_class, (t_method)wavesel_reset_selection, 
        gensym("reset_selection"), 0);
    class_addmethod(wavesel_class, (t_method)wavesel_draw_columns, 
        gensym("drawwf"), A_FLOAT, 0);

// internal
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
    class_addmethod(wavesel_class, (t_method)wavesel_loadbang, 
        gensym("loadbang"), 0);
    class_addmethod(wavesel_class, (t_method)wavesel_move_element, 
        gensym("move_element"), A_FLOAT, 0);
        
// hammergui
    class_addmethod(wavesel_class, (t_method)wavesel_doup, 
        gensym("_up"), A_FLOAT, 0);
//    class_addmethod(wavesel_class, (t_method)wavesel_doelse, 
//        gensym("_bang"), A_FLOAT, A_FLOAT, 0);
//    class_addmethod(wavesel_class, (t_method)wavesel_doelse,
//        gensym("_zero"), A_FLOAT, A_FLOAT, 0);
//    class_addmethod(wavesel_class, (t_method)wavesel_doelse,
//        gensym("_poll"), A_FLOAT, A_FLOAT, 0);

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


