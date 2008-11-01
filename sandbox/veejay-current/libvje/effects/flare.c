/*
 * VeeJay
 *
 * Copyright(C)2002-2005 Niels Elburg <nelburg@looze.net>
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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "flare.h"
#include "common.h"

/* blend by blurred mask.
	derived from radial blur and chromamagick effect.

 */

vj_effect *flare_init(int w, int h)
{
	vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
	ve->num_params = 3;
	ve->defaults =  (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
	ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
	ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
	ve->defaults[0] = 3;
	ve->defaults[1] = 25;
	ve->defaults[2] = 100;
	ve->description = "Flare (Glow)";
	ve->limits[0][0] = 0; /* p0 = type, p2 = opacity, p3 = radius */
	ve->limits[1][0] = 5;
	ve->limits[0][1] = 0;
	ve->limits[1][1] = 255;
	ve->limits[0][2] = 0;
	ve->limits[1][2] = 100;
	ve->extra_frame = 0;
	ve->sub_format = 1;
	ve->has_user = 0;

	return ve;
}

static uint8_t *flare_buf[3];

int	flare_malloc(int w, int h)
{
	flare_buf[0] = (uint8_t*)vj_yuvalloc(w,h);
	if(!flare_buf[0])
		return 0;
	flare_buf[1] = flare_buf[0] + (w*h);
	flare_buf[2] = flare_buf[1] + (w*h);
	return 1;
}

void	flare_free(void)
{
	if(flare_buf[0]) free(flare_buf[0]);
	flare_buf[0] = NULL;
	flare_buf[1] = NULL;
	flare_buf[2] = NULL;
}

void flare_exclusive(VJFrame *frame, VJFrame *frame2, int width, int height, int op_a) {

	unsigned int i;
	const int len = frame->len;
 	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];
	uint8_t *Y2 = frame2->data[0];
	uint8_t *Cb2 = frame2->data[1];
	uint8_t *Cr2 = frame2->data[2];

	int a=0, b=0, c=0;
	const unsigned int o1 = op_a;
	const unsigned int o2 = 255 - a;


	for (i = 0; i < len; i++)
	{
		a = Y[i];
		b = Y2[i];

		a *= o1;
		b *= o2;
		a = a >> 8;
		b = b >> 8;	
		Y[i] = (a+b) - ((a * b) >> 8);
		// CLAMP_Y(c);
	
		a = Cb[i]-128;
		b = Cb2[i]-128;
		c = (a + b) - (( a * b) >> 8);
		c += 128;
		Cb[i] = CLAMP_UV(c);

		a = Cr[i]-128;
		b = Cr2[i]-128;
		c = (a + b) - ((a*b) >> 8);
		c += 128;
		Cr[i] = CLAMP_UV(c);
    	}
}

void flare_additive(VJFrame *frame, VJFrame *frame2, int width,
		int height, int op_a) {

	unsigned int i;
	const int len = frame->len;
 	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];
	uint8_t *Y2 = frame2->data[0];
	uint8_t *Cb2 = frame2->data[1];
	uint8_t *Cr2 = frame2->data[2];

	int a,b;	
	const unsigned int o1 = op_a;
 	const unsigned int o2 = 255 - op_a;

	for(i=0; i < len; i++)
	{
		a = (Y[i]*o1) >> 7;
		b = (Y2[i]*o2) >> 7;
		Y[i] = a + (( 2 * b ) - 255);

		a = Cb[i];
		b = Cb2[i];

		Cb[i] = a + ( ( 2 * b ) - 255 );

		a = Cr[i] ;
		b = Cr2[i] ;

		Cr[i] = a + ( ( 2 * b ) - 255 );
	}
}

void flare_unfreeze( VJFrame *frame, VJFrame *frame2, int w, int h, int op_a ) {
	unsigned int i;
	const int len = frame->len;
 	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];
	uint8_t *Y2 = frame2->data[0];
	uint8_t *Cb2 = frame2->data[1];
	uint8_t *Cr2 = frame2->data[2];

	int a,b;

	for(i=0; i < len; i++)
	{
		a = Y[i];
		b = Y2[i];
		if ( a < 1 ) a = 1;
		Y[i] = 255 - (( op_a - b) * (op_a - b)) / a;
		//Y[i] = CLAMP_Y(c);
		
		a = Cb[i];
		b = Cb2[i];
		if ( a < 1) a = 1;
		Cb[i] = 255 - (( 255 - b) * ( 255 - b )) / a;
		//Cb[i] = CLAMP_UV(c);
		
		a = Cr[i];
		b = Cr2[i];
		if ( a < 1 ) a = 1;
		Cr[i] = 255 - ((255 -b ) * (255 - b)) /a ;
		//Cr[i] = CLAMP_UV(c);
	}
}


void flare_darken(VJFrame *frame, VJFrame *frame2, int w, int h, int op_a)
{

	unsigned int i;
	const int len = frame->len;
 	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];
	uint8_t *Y2 = frame2->data[0];
	uint8_t *Cb2 = frame2->data[1];
	uint8_t *Cr2 = frame2->data[2];

	const unsigned int o1 = op_a;
	const unsigned int o2 = 255 - op_a;

	for(i=0; i < len; i++)
	{
		if(Y[i] > Y2[i])
		{
			Y[i] = ((Y[i] * o1) + (Y2[i] * o2)) >> 8; 
			Cb[i] = ((Cb[i] * o1) + (Cb2[i] * o2)) >> 8;
			Cr[i] = ((Cr[i] * o1) + (Cr2[i] * o2)) >> 8;
		} 
	}
}

void	flare_simple( VJFrame *frame, VJFrame *frame2, int w, int h, int op_a )
{
	unsigned int i;
 	unsigned int len = w* h;
	uint8_t *Y = frame->data[0];
	uint8_t *Y2 = frame2->data[0];

	const uint8_t solid = 255 - op_a;
	uint8_t premul;
	for (i = 0; i < len; i++)
	{
		premul = ((Y2[i] * op_a) + (solid * 0xff )) >> 8; 
		Y[i] = ( (Y[i] >> 1) + (premul >> 1 ) );
    	}

}

void flare_lighten(VJFrame *frame, VJFrame *frame2, int w, int h, int op_a)
{

	unsigned int i;
	const int len = frame->len;
 	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];
	uint8_t *Y2 = frame2->data[0];
	uint8_t *Cb2 = frame2->data[1];
	uint8_t *Cr2 = frame2->data[2];

	const unsigned int o1 = op_a;
	const unsigned int o2 = 255 - op_a;

	for(i=0; i < len; i++)
	{
		if(Y[i] < Y2[i])
		{
			Y[i] = ((Y[i] * o1) + (Y2[i] * o2)) >> 8; 
			Cb[i] = ((Cb[i] * o1) + (Cb2[i] * o2)) >> 8;
			Cr[i] = ((Cr[i] * o1) + (Cr2[i] * o2)) >> 8;
		} 
	}
}


void flare_apply(VJFrame *A, 
			int width, int height, int type, int op_a, int radius)
{
	int y,x;   
	int plane = 0;
	/* clone A */
	VJFrame B;
	veejay_memcpy( &B, A, sizeof(VJFrame));
	/* data is local */
	B.data[0] = flare_buf[0];
	B.data[1] = flare_buf[1];
	B.data[2] = flare_buf[2];

	/* copy image data */
	veejay_memcpy( B.data[0], A->data[0], A->len );
	veejay_memcpy( B.data[1], A->data[1], A->len );
	veejay_memcpy( B.data[2], A->data[2], A->len );

	/* apply blur on Image, horizontal and vertical
	   (blur2 is from xine, see radial blur */
	
	for( plane = 0; plane < 2; plane ++ )
	{
		for( y = 0; y < height; y ++ ) 
			blur2( A->data[plane] + (y * width),
				      B.data[plane] + (y * width),
					width,
					radius,
					2,
					1,	
					1 );

		for( x = 0; x < width; x ++ )
			blur2( A->data[plane] + x ,
				B.data[plane] + x,
				height,
				radius,
				2,
				width,
				width );

	}
	/* overlay original on top of blurred image */
	switch( type )
	{
    		case 1:
			flare_exclusive(A,&B,width,height,op_a);
		break;
		case  2:
			flare_additive(A,&B,width,height,op_a);	
		break;
		case  3:
			flare_unfreeze(A,&B,width,height,op_a);
		break;
		case  4:
			flare_darken(A,&B,width,height,op_a);
		break;
		case 5:
			flare_lighten( A, &B, width, height, op_a );
		break;

		default:
			flare_simple( A, &B, width,height, op_a );
			break;

	}
}

