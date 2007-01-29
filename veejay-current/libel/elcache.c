/*
 * Copyright (C) 2002-2006 Niels Elburg <nelburg@looze.net>
 * 
 * This program is free software you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

/*
	Cache frames from file to memory
 */
#include <config.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <libvjmem/vjmem.h>
#include <libvjmsg/vj-common.h>
#include <libel/elcache.h>
#ifdef STRICT_CHECKING
#include <assert.h>
#endif

typedef struct
{
	int	size;
	int	fmt;
	long	num;
	void    *buffer;
} cache_slot_t;

typedef struct
{
	cache_slot_t	**cache;
	int		len;
	long		*index;
} cache_t;

static	int	cache_free_slot(cache_t *v)
{
	int i;
	for( i = 0; i < v->len; i ++ )
	 if( v->index[i] == -1 ) return i;
	return -1;
}

static	long	total_mem_used_ = 0;

static	void	cache_claim_slot(cache_t *v, int free_slot, uint8_t *linbuf, long frame_num,int buf_len, int decoder_id)
{
	// create new node
	cache_slot_t *data = (cache_slot_t*) vj_malloc(sizeof(cache_slot_t));
	data->size = buf_len;
	data->num  = frame_num;
	data->fmt  = decoder_id;
	data->buffer = vj_malloc( buf_len );
#ifdef STRICT_CHECKING
	assert( v->index[free_slot] != frame_num );
#endif
	// clear old buffer
	if( v->index[free_slot] >= 0 )
	{
		cache_slot_t *del_slot = v->cache[free_slot];
		total_mem_used_ -= del_slot->size;
		free( del_slot->buffer );
		free( del_slot );
		v->cache[free_slot] = NULL;
	}

	veejay_memcpy( data->buffer, linbuf, buf_len );
	v->index[ free_slot ] = frame_num;
	v->cache[ free_slot ] = data;
	total_mem_used_ += buf_len;
}

static	int	cache_find_slot( cache_t *v, long frame_num )
{
	int i;
	int k = 0;
	long n = 0;
	for( i = 0; i < v->len ; i ++ )
	{
		long d = abs( v->index[i] - frame_num );
		if( d > n )
		{	n = d;	k = i ; }
	}
	return k;
}

static	int	cache_locate_slot( cache_t *v, long frame_num)
{
	int i;
	for( i = 0; i < v->len ; i ++ )
		if( v->index[i] == frame_num ) 
			return i;
	return -1;
}


void	*init_cache( unsigned int n_slots )
{
	if(n_slots <= 0)
		return NULL;

	cache_t *v = (cache_t*) vj_malloc(sizeof(cache_t));
	v->len = n_slots;
	v->cache = (cache_slot_t**) vj_malloc(sizeof(cache_slot_t*) * v->len );
	veejay_memset( v->cache, 0, sizeof(cache_slot_t*) * v->len );

	v->index = (long*) malloc(sizeof(long) * v->len );
	veejay_memset( v->index, -1, sizeof(long) * v->len );

	return (void*) v;
}

void	reset_cache(void *cache)
{
	int i = 0;
	cache_t *v = (cache_t*) cache;

	veejay_memset( v->index, -1, sizeof(long) * v->len );
	for( i = 0; i < v->len; i ++ )
	{
		if( v->cache[i] )
		{
			total_mem_used_ -= v->cache[i]->size;
			if(v->cache[i]->buffer)
			 free(v->cache[i]->buffer);
			free(v->cache[i]);
			v->cache[i] = NULL;
		};
	}
}

int	cache_avail_mb()
{
	return ( total_mem_used_ == 0 ? 0 : total_mem_used_ / (1024 * 1024 ));
}


void	free_cache(void *cache)
{
	cache_t *v = (cache_t*) cache;	
	reset_cache( cache );	
	free(v->cache);
	free(v->index);
	free(v);
}

void	cache_frame( void *cache, uint8_t *linbuf, int buflen, long frame_num , int decoder_id)
{
	cache_t *v = (cache_t*) cache;
#ifdef STRICT_CHECKING
	assert( cache != NULL );
	assert( linbuf != NULL );
	assert( buflen > 0 );
	assert( frame_num >= 0 );
#else
	if( buflen <= 0 )
		return;
#endif	
	int slot_num = cache_free_slot( cache );
	if( slot_num == -1 )
		slot_num = cache_find_slot( v, frame_num );

#ifdef STRICT_CHECKING
	assert(slot_num >= 0 );
#endif
	cache_claim_slot(v, slot_num, linbuf, frame_num, buflen, decoder_id);
} 

uint8_t *get_cached_frame( void *cache, long frame_num, int *buf_len, int *decoder_id )
{
	cache_t *v = (cache_t*) cache;
	int slot = cache_locate_slot( v, frame_num );
	if( slot == -1 )
		return NULL;

	cache_slot_t *data = v->cache[ slot ];

	*buf_len 	= data->size;
	*decoder_id	= data->fmt;
#ifdef STRICT_CHECKING
	assert( data->num == frame_num );
#endif
	return (uint8_t*) data->buffer;
}
