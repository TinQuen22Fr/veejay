/* 
 * Linux VeeJay
 *
 * Copyright(C)2002 Niels Elburg <elburg@hio.hen.nl>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License , or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307 , USA.
 */
#include <config.h>
#include <stdint.h>
#include <libvjmem/vjmem.h>
#include "common.h"
#include "chromascratcher.h"
#include "chromamagick.h"
static uint8_t *cframe[3];
static int cnframe = 0;
static int cnreverse = 0;
static int chroma_restart = 0;

static VJFrame *_tmp;

vj_effect *chromascratcher_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 4;
    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 25; /* uses the chromamagick effect for scratchign */
    ve->limits[0][1] = 0;
    ve->limits[1][1] = 255;
    ve->limits[0][2] = 0;
    ve->limits[1][2] = 25;
    ve->limits[0][3] = 0;
    ve->limits[1][3] = 1;

    ve->defaults[0] = 1;
    ve->defaults[1] = 150;
    ve->defaults[2] = 8;
    ve->defaults[3] = 1;
    ve->description = "Matte Scratcher";
    ve->sub_format = 1;
    ve->extra_frame = 0;
	ve->has_user =0;


      return ve;
}
// FIXME private 
int	chromascratcher_malloc(int w, int h)
{
    cframe[0] =
	(uint8_t *) vj_malloc(w * h * sizeof(uint8_t) * 25 * 3 );
    if(!cframe[0]) return 0;			   
    _tmp = (VJFrame*) vj_calloc(sizeof(VJFrame));

    cframe[1] = cframe[0] + ( w * h * 25 );
    cframe[2] = cframe[1] + ( w * h * 25 );

	veejay_memset( cframe[0], 0,  (w*h*25));
	veejay_memset( cframe[1], 128,(w*h*25));
	veejay_memset( cframe[2], 128,(w*h*25));
    
    return 1;
}

void chromascratcher_free() {
   if(cframe[0])
	   free(cframe[0]);
   if(_tmp) free(_tmp);
   cframe[0] = NULL;
   cframe[1] = NULL;
   cframe[2] = NULL;
   _tmp = NULL;
}

void chromastore_frame(uint8_t * yuv1[3], int w, int h, int n,
		       int no_reverse)
{
     if (cnreverse)
	cnframe--;
    else
	cnframe++;

 
  
    if (cnframe >= n) {
	if (no_reverse == 0) {
	    cnreverse = 1;
	    cnframe = n - 1;
	} else {
	    cnframe = 0;
	}
    }
    if (cnframe == 0)
	cnreverse = 0;

    if (!cnreverse) {
	veejay_memcpy(cframe[0] + (w * h * cnframe), yuv1[0], (w * h));
	veejay_memcpy(cframe[1] + (w * h * cnframe), yuv1[1], (w * h));
	veejay_memcpy(cframe[2] + (w * h * cnframe), yuv1[2], (w * h));
    } else {
	veejay_memcpy(yuv1[0], cframe[0] + (w * h * cnframe), (w * h));
	veejay_memcpy(yuv1[1], cframe[1] + (w * h * cnframe), (w * h));
	veejay_memcpy(yuv1[2], cframe[2] + (w * h * cnframe), (w * h));
    }

}

void chromascratcher_apply(VJFrame *frame,
			   int width, int height, int mode, int opacity,
			   int n, int no_reverse)
{
    int i;
    const int len = frame->len;
    const unsigned int op_a = (opacity > 255) ? 255 : opacity;
    const unsigned int op_b = 255 - op_a;
    const int offset = len * cnframe;
 	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];
    veejay_memcpy( _tmp, frame, (sizeof(VJFrame)));
	_tmp->data[0] = cframe[0];
	_tmp->data[1] = cframe[1];
	_tmp->data[2] = cframe[2];

    if(no_reverse != chroma_restart)
	{
		chroma_restart = no_reverse;
		cnframe = n;
	}

    if (cnframe == 0)
	{
		veejay_memcpy(cframe[0] + (len * cnframe), Y, len);
		veejay_memcpy(cframe[1] + (len * cnframe), Cb, len);
		veejay_memcpy(cframe[2] + (len * cnframe), Cr, len);
    }

    if(mode>3) {
		mode-=3;
   	   chromamagick_apply( frame,_tmp,width, height,mode,opacity);
    }
    else {


	    switch (mode) {		/* scratching with a sequence of frames (no scene changes) */


    case 0:
		/* moving parts will dissapear over time */
		for (i = 0; i < len; i++) {
		    if (cframe[0][offset + i] < Y[i]) {
				Y[i] = cframe[0][offset + i];
				Cb[i] = cframe[1][offset + i];
				Cr[i] = cframe[2][offset + i];
		    }
		}
	break;
    case 1:
		for (i = 0; i < len; i++) {
		    /* moving parts will remain visible */
		    if (cframe[0][offset + i] > Y[i]) {
				Y[i] = cframe[0][offset + i];
				Cb[i] = cframe[1][offset + i];
				Cr[i] = cframe[2][offset + i];
		    }
		}
	break;
    case 2:
	for (i = 0; i < len; i++) {
	    if ((cframe[0][offset + i] * op_a) < (Y[i] * op_b)) {
			Y[i] = cframe[0][offset + i];
			Cb[i] = cframe[1][offset + i];
			Cr[i] = cframe[2][offset + i];
	    }
	}
	break;
    case 3:
	for (i = 0; i < len; i++) {
	    /* moving parts will remain visible */
	    if ((cframe[0][offset + i] * op_a) > (Y[i] * op_b)) {
		Y[i] = cframe[0][offset + i];
		Cb[i] = cframe[1][offset + i];
		Cr[i] = cframe[2][offset + i];
	    }
	}
	break;
    
	}

	}   // else


    chromastore_frame(frame->data, width, height, n, no_reverse);
}
