/*
This teeth~ is not yet complete:
- interpolation is not implemented. 
Otherwise it works nice, but strange things happen at extreme settings. 
Maybe this is to be expected with a filter like this, but this is my 
first filter. fjkraan@xs4all.nl, 2015-06-23
*/

#include "m_pd.h"
#include "sickle/sic.h"
//#include "shared.h"

static t_class *teeth_class;

#define MAXDELAYMS 20

typedef struct _teeth_tilde {
  t_object   x_obj;
  t_sample   f;
  t_sample  *x_bufFW;    /* pointer to forward buffer */
  t_sample  *x_bufFB;    /* pointer to feedback buffer */
  t_int      x_fwWrite;  /* relative write pointer to FW buffer*/
  t_int      x_fbWrite;  /* relative write pointer to FB buffer */
  t_int      x_bufsize;  /* buffer size  */
  t_float    x_ksr;      /* 'kilo-samplerate', like 44.1 */
} t_teeth;

/* The actual work is done here */
static t_int *teeth_perform(t_int *w)
{
    t_teeth  *x      =  (t_teeth *)(w[1]);   /* pointer to instance struct */
    t_sample *in1    = (t_sample *)(w[2]);   /* input vector/array */
    t_sample *fw_delay = (t_sample *)(w[3]); /* fw read delay in ms */
    t_sample *fb_delay = (t_sample *)(w[4]); /* fb read delay in ms */
    t_sample *a_gain = (t_sample *)(w[5]);   /* inlet gain */
    t_sample *b_gain = (t_sample *)(w[6]);   /* bufFW read gain */
    t_sample *c_gain = (t_sample *)(w[7]);   /* bufFB read gain */
    t_sample *out1   = (t_sample *)(w[8]);   /* first output vector/array */
    int       n      = (int)(w[9]);          /* vector/array size */
    int       p;
    int       fwRead;
    int       fbRead;
    int       fwWrite = x->x_fwWrite;
    int       fbWrite = x->x_fbWrite;
    int       bufsize = x->x_bufsize;
    t_float   ksr     = x->x_ksr;

    for (p = 0; p < n; p++) {
	fwRead = (int)(fw_delay[p] < 0) ? 0 : fw_delay[p] * ksr + 0.5;     // inlet safe guard
	fwRead = (int)(fwWrite - fwRead);            // relate to integer write position
	fwRead = (fwRead < 0) ? bufsize + fwRead : fwRead;       // compensate for circular buffer lower boundary
	fwRead = (fwRead > bufsize) ? fwRead - bufsize : fwRead; // compensate for circular buffer upper boundary
	fbRead = (int)(fb_delay[p] < 0) ? 0 : fb_delay[p] * ksr + 0.5;
	fbRead = (int)(fbWrite - fbRead);
	fbRead = (fbRead < 0) ? bufsize + fbRead : fbRead;
	fbRead = (fbRead > bufsize) ? fbRead - bufsize : fbRead;
	
	out1[p] = in1[p] * a_gain[p] + x->x_bufFW[fwRead] * b_gain[p] + 
	    x->x_bufFB[fbRead] * c_gain[p];
	    
	x->x_bufFW[fwWrite++] = in1[p];
	x->x_bufFB[fbWrite++] = out1[p];
	fwWrite = (fwWrite < bufsize) ? fwWrite : 0;
	fbWrite = (fbWrite < bufsize) ? fbWrite : 0;
    }
    x->x_fwWrite = fwWrite;
    x->x_fbWrite = fbWrite;
    return (w+10);
}

#define MILLI 0.001
static void teeth_dsp(t_teeth *x, t_signal **sp)
{
    x->x_ksr = sp[0]->s_sr * MILLI;
    int newBufsize = (int)(x->x_ksr * MAXDELAYMS);
    if (x->x_bufsize == 0 || x->x_bufsize != newBufsize)
    {
	t_float *bufFW;
	t_float *bufFB;
	
	x->x_bufsize = newBufsize;
	bufFW = (t_float *)getbytes(x->x_bufsize * sizeof(*bufFW));
	bufFB = (t_float *)getbytes(x->x_bufsize * sizeof(*bufFB));
	
	int i;
	for (i = 0; i < x->x_bufsize; i++) {
	    bufFW[i] = 0;
	    bufFB[i] = 0;
	}
	x->x_bufFW = bufFW;
	x->x_bufFB = bufFB;
	x->x_fwWrite = 0;
	x->x_fbWrite = 0;
    }

    dsp_add(teeth_perform, 	/* register the perform method */
        9, 			/* number of following parameters */
        x,			/* pointer to the object struct */
        sp[0]->s_vec, 	/* first inlet signal */
        sp[1]->s_vec, 	/* fw_delay */
        sp[2]->s_vec, 	/* fb_delay */
        sp[3]->s_vec, 	/* a_gain */
        sp[4]->s_vec, 	/* b_gain */
        sp[5]->s_vec, 	/* c_gain */
        sp[6]->s_vec, 	/* first outlet signal */
        sp[0]->s_n);		/* the size of the vectors/arrays */
}

static void *teeth_new(t_floatarg a_gain, t_floatarg b_gain, 
    t_floatarg c_gain, t_floatarg fw_delay, t_floatarg fb_delay)
{
    t_teeth *x = (t_teeth *)pd_new(teeth_class);  
    x->x_bufsize = 0;
  
    /* create signal inlets */
    sic_newinlet((t_sic *)x, fw_delay);
    sic_newinlet((t_sic *)x, fb_delay);
    sic_newinlet((t_sic *)x, a_gain);
    sic_newinlet((t_sic *)x, b_gain);
    sic_newinlet((t_sic *)x, c_gain);
    
    /* create a signal outlet */
    outlet_new(&x->x_obj, &s_signal);
  
    return (void *)x;
}

static void teeth_free(t_teeth *x)
{
    if (x->x_bufFW) 
        freebytes(x->x_bufFW, x->x_bufsize * sizeof(*x->x_bufFW));
    if (x->x_bufFB) 
        freebytes(x->x_bufFB, x->x_bufsize * sizeof(*x->x_bufFB));
}

void teeth_tilde_setup(void) 
{
  teeth_class = 
	class_new(gensym("teeth~"),	/* register the 'teeth~' symbol */
					/* declare initialization method */
        	(t_newmethod)teeth_new, /* method to initialize objects */	
        	(t_method)teeth_free, 			/* method to cleanup or 0 */	
		sizeof(t_teeth),	/* size of the object struct */
       	 	CLASS_DEFAULT, 	/* make it type CLASS_PATCHABLE */
        	A_DEFFLOAT, 		/* fw_delay */
        	A_DEFFLOAT, 		/* fb_delay */
               	A_DEFFLOAT, 		/* a_gain */
        	A_DEFFLOAT, 		/* b_gain */ 
		A_DEFFLOAT, 		/* c_gain */
		0);			/*  */

  class_addmethod(teeth_class,	/*  */
        	  (t_method)teeth_dsp, /*  */
		  gensym("dsp"),	/* declare this is a signal class */
		  0);			/*  */

  CLASS_MAINSIGNALIN(teeth_class, /* pointer to the class struct */
		     t_teeth,    /* pointer to the object struct */
		     f);	      /* dummy variable */
//    logpost(NULL, 4, "this is cyclone/teeth~ %s, %dth %s build",
//	CYCLONE_VERSION, CYCLONE_BUILD, CYCLONE_RELEASE);		     
}

