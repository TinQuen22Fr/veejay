/* 
 * Linux VeeJay
 *
 * Copyright(C)2004 Niels Elburg <elburg@hio.hen.nl>
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
#include <libvje/vje.h>
#include <stdlib.h>
#include <veejay/vj-global.h>
#include <ffmpeg/avutil.h>
#include <libyuv/yuvconv.h>
#include "rgbchannel.h"

vj_effect *rgbchannel_init(int w, int h)
{
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 3;

    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 1;
    ve->limits[0][1] = 0;
    ve->limits[1][1] = 1;
    ve->limits[0][2] = 0;
    ve->limits[1][2] = 1;

    ve->defaults[0] = 0;
    ve->defaults[0] = 0;
    ve->defaults[0] = 0;
    ve->description = "RGB Channel";
    ve->sub_format = 1;
    ve->extra_frame = 0;
    ve->has_user = 0;
    return ve;
}

static uint8_t *rgb_ = NULL;
int	rgbchannel_malloc( int w, int h )
{
	if(!rgb_)
		rgb_ = vj_malloc( sizeof(uint8_t) * w * h * 3 );
	if(!rgb_)
		return 0;
	return 1;
}

void	rgbchannel_free( )
{
	if(rgb_)
		free(rgb_);
	rgb_ = NULL;
}

void rgbchannel_apply( VJFrame *frame, int width, int height, int chr, int chg , int chb)
{
    unsigned int x,y,i;

	util_convertrgb24( frame->data,width,height, PIX_FMT_YUV444P,
		0, rgb_ );

	int row_stride = width * 3;

	if(chr)
	{
		for( y = 0; y < height; y ++ )
		{
			for( x = 0; x < row_stride; x += 3 )
			{
				rgb_[ y * row_stride + x ] = 0;
			}
		}
	}
	if(chg)
	{
		for( y = 0; y < height; y ++ )
		{
			for( x = 1; x < row_stride-1; x += 3 )
			{
				rgb_[ y * row_stride + x ] = 0;
			}
		}
	}
	if(chb)
	{
		for( y = 0; y < height; y ++ )
		{
			for( x = 2; x < row_stride-2; x += 3 )
			{
				rgb_[ y * row_stride + x ] = 0;
			}
		}
	}

	util_convertsrc( rgb_, width,height, 
			PIX_FMT_YUV444P,
			0,
			frame->data,
			FMT_RGB24 );
}
