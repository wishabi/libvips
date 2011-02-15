/* vips image class
 * 
 * 4/2/11
 * 	- hacked up from various places
 */

/*

    This file is part of VIPS.
    
    VIPS is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define VIPS_DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /*HAVE_SYS_FILE_H*/
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /*HAVE_UNISTD_H*/
#ifdef HAVE_IO_H
#include <io.h>
#endif /*HAVE_IO_H*/
#include <libxml/parser.h>
#include <errno.h>

#ifdef OS_WIN32
#include <windows.h>
#endif /*OS_WIN32*/

#include <vips/vips.h>
#include <vips/internal.h>
#include <vips/debug.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /*WITH_DMALLOC*/

/**
 * SECTION: image
 * @short_description: the VIPS image class
 * @stability: Stable
 * @see_also: <link linkend="libvips-region">region</link>
 * @include: vips/vips.h
 *
 * The image class and associated types and macros.
 */

/**
 * VIPS_MAGIC_INTEL:
 *
 * The first four bytes of a VIPS file in Intel byte ordering.
 */

/**
 * VIPS_MAGIC_SPARC:
 *
 * The first four bytes of a VIPS file in SPARC byte ordering.
 */

/** 
 * VipsDemandStyle:
 * @VIPS_DEMAND_STYLE_SMALLTILE: demand in small (typically 64x64 pixel) tiles
 * @VIPS_DEMAND_STYLE_FATSTRIP: demand in fat (typically 10 pixel high) strips
 * @VIPS_DEMAND_STYLE_THINSTRIP: demand in thin (typically 1 pixel high) strips
 * @VIPS_DEMAND_STYLE_ANY: demand geometry does not matter
 *
 * See im_demand_hint(). Operations can hint to the VIPS image IO system about
 * the kind of demand geometry they prefer. 
 *
 * These demand styles are given below in order of increasing
 * restrictiveness.  When demanding output from a pipeline, im_generate()
 * will use the most restrictive of the styles requested by the operations 
 * in the pipeline.
 *
 * VIPS_DEMAND_STYLE_THINSTRIP --- This operation would like to output strips 
 * the width of the image and a few pels high. This is option suitable for 
 * point-to-point operations, such as those in the arithmetic package.
 *
 * This option is only efficient for cases where each output pel depends 
 * upon the pel in the corresponding position in the input image.
 *
 * VIPS_DEMAND_STYLE_FATSTRIP --- This operation would like to output strips 
 * the width of the image and as high as possible. This option is suitable 
 * for area operations which do not violently transform coordinates, such 
 * as im_conv(). 
 *
 * VIPS_DEMAND_STYLE_SMALLTILE --- This is the most general demand format.
 * Output is demanded in small (around 100x100 pel) sections. This style works 
 * reasonably efficiently, even for bizzare operations like 45 degree rotate.
 *
 * VIPS_DEMAND_STYLE_ANY --- This image is not being demand-read from a disc 
 * file (even indirectly) so any demand style is OK. It's used for things like
 * im_black() where the pixels are calculated.
 *
 * See also: vips_demand_hint().
 */

/**
 * VipsType: 
 * @VIPS_TYPE_MULTIBAND: generic many-band image
 * @VIPS_TYPE_B_W: some kind of single-band image
 * @VIPS_TYPE_HISTOGRAM: a 1D image such as a histogram or lookup table
 * @VIPS_TYPE_FOURIER: image is in fourier space
 * @VIPS_TYPE_XYZ: the first three bands are colours in CIE XYZ colourspace
 * @VIPS_TYPE_LAB: pixels are in CIE Lab space
 * @VIPS_TYPE_CMYK: the first four bands are in CMYK space
 * @VIPS_TYPE_LABQ: implies #VIPS_CODING_LABQ
 * @VIPS_TYPE_RGB: generic RGB space
 * @VIPS_TYPE_UCS: a uniform colourspace based on CMC
 * @VIPS_TYPE_LCH: pixels are in CIE LCh space
 * @VIPS_TYPE_LABS: pixels are CIE LAB coded as three signed 16-bit values
 * @VIPS_TYPE_sRGB: pixels are sRGB
 * @VIPS_TYPE_YXY: pixels are CIE Yxy
 * @VIPS_TYPE_RGB16: generic 16-bit RGB
 * @VIPS_TYPE_GREY16: generic 16-bit mono
 *
 * How the values in an image should be interpreted. For example, a
 * three-band float image of type #VIPS_TYPE_LAB should have its pixels
 * interpreted as coordinates in CIE Lab space.
 *
 * These values are set by operations as hints to user-interfaces built on top 
 * of VIPS to help them show images to the user in a meaningful way. 
 * Operations do not use these values to decide their action.
 */

/**
 * VipsFormat: 
 * @VIPS_FORMAT_NOTSET: invalid setting
 * @VIPS_FORMAT_UCHAR: unsigned char format
 * @VIPS_FORMAT_CHAR: char format
 * @VIPS_FORMAT_USHORT: unsigned short format
 * @VIPS_FORMAT_SHORT: short format
 * @VIPS_FORMAT_UINT: unsigned int format
 * @VIPS_FORMAT_INT: int format
 * @VIPS_FORMAT_FLOAT: float format
 * @VIPS_FORMAT_COMPLEX: complex (two floats) format
 * @VIPS_FORMAT_DOUBLE: double float format
 * @VIPS_FORMAT_DPCOMPLEX: double complex (two double) format
 *
 * The format used for each band element. 
 *
 * Each corresponnds to a native C type for the current machine. For example,
 * #VIPS_FORMAT_USHORT is <type>unsigned short</type>.
 */

/**
 * VipsCoding: 
 * @VIPS_CODING_NONE: pixels are not coded
 * @VIPS_CODING_LABQ: pixels encode 3 float CIELAB values as 4 uchar
 * @VIPS_CODING_RAD: pixels encode 3 float RGB as 4 uchar (Radiance coding)
 *
 * How pixels are coded. 
 *
 * Normally, pixels are uncoded and can be manipulated as you would expect.
 * However some file formats code pixels for compression, and sometimes it's
 * useful to be able to manipulate images in the coded format.
 */

/** 
 * VipsProgress:
 * @run: Time we have been running 
 * @eta: Estimated seconds of computation left 
 * @tpels: Number of pels we expect to calculate
 * @npels: Number of pels calculated so far
 * @percent: Percent complete
 * @start: Start time 
 *
 * A structure available to eval callbacks giving information on evaluation
 * progress. See im_add_eval_callback().
 */

/**
 * VipsImage:
 *
 * An image. These can represent an image on disc, a memory buffer, an image
 * in the process of being written to disc or a partially evaluated image
 * in memory.
 */

/**
 * VIPS_IMAGE_SIZEOF_ELEMENT:
 * @I: a #VipsImage
 *
 * Returns: sizeof() a band element.
 */

/**
 * VIPS_IMAGE_SIZEOF_PEL:
 * @I: a #VipsImage
 *
 * Returns: sizeof() a pixel.
 */

/**
 * VIPS_IMAGE_SIZEOF_LINE:
 * @I: a #VipsImage
 *
 * Returns: sizeof() a scanline of pixels.
 */

/**
 * VIPS_IMAGE_N_ELEMENTS:
 * @I: a #VipsImage
 *
 * Returns: The number of band elements in a scanline.
 */

/**
 * VIPS_IMAGE_ADDR:
 * @I: a #VipsImage
 * @X: x coordinate
 * @Y: y coordinate
 *
 * This macro returns a pointer to a pixel in an image. It only works for
 * images which are fully available in memory, so memory buffers and small
 * mapped images only.
 * 
 * If VIPS_DEBUG is defined, you get a version that checks bounds for you.
 *
 * See also: VIPS_REGION_ADDR().
 *
 * Returns: The address of pixel (x,y) in the image.
 */

/** 
 * vips_open_local_array:
 * @IM: image to open local to
 * @OUT: array to fill with #VipsImage *
 * @N: array size
 * @NAME: filename to open
 * @MODE: mode to open with
 *
 * Just like vips_open(), but opens an array of images. Handy for creating a 
 * set of temporary images for a function.
 *
 * Example:
 *
 * |[
 * VipsImage *t[5];
 *
 * if( vips_open_local_array( out, t, 5, "some-temps", "p" ) ||
 *   vips_add( a, b, t[0] ) ||
 *   vips_invert( t[0], t[1] ) ||
 *   vips_add( t[1], t[0], t[2] ) ||
 *   vips_costra( t[2], out ) )
 *   return( -1 );
 * ]|
 *
 * See also: vips_open(), vips_open_local(), vips_local_array().
 *
 * Returns: 0 on sucess, or -1 on error
 */

/**
 * vips_open_local:
 * @IM: image to open local to
 * @NAME: filename to open
 * @MODE: mode to open with
 *
 * Just like vips_open(), but the #VipsImage will be closed for you 
 * automatically when @IM is closed.
 *
 * See also: vips_open(), vips_local().
 *
 * Returns: a new #VipsImage, or %NULL on error
 */

/* Properties.
 */
enum {
	PROP_WIDTH = 1,
	PROP_HEIGHT,
	PROP_BANDS,
	PROP_FORMAT,
	PROP_FILENAME,
	PROP_KILL,
	PROP_MODE,
	PROP_DEMAND,
	PROP_LAST
}; 

/* Our signals. 
 */
enum {
	SIG_PRECLOSE,		
	SIG_CLOSE,		
	SIG_POSTCLOSE,		
	SIG_PREEVAL,		
	SIG_EVAL,		
	SIG_POSTEVAL,		
	SIG_WRITTEN,		
	SIG_LAST
};

/* Progress feedback. Only really useful for testing, tbh.
 */
int im__progress = 0;

/* A string giving the image size (in bytes of uncompressed image) above which 
 * we decompress to disc on open.  Can be eg. "12m" for 12 megabytes.
 */
char *im__disc_threshold = NULL;

static guint vips_image_signals[SIG_LAST] = { 0 };

G_DEFINE_TYPE( VipsImage, vips_image, VIPS_TYPE_OBJECT );

static int
vips_image_preclose( VipsImage *image )
{
	VipsImageClass *image_class = VIPS_IMAGE_GET_CLASS( image );

	if( !image->preclose ) {
		image->preclose = TRUE;

#ifdef DEBUG
		printf( "vips_image_preclose: " );
		vips_object_print( object );
#endif /*DEBUG*/

		g_signal_emit( image, vips_image_signals[SIG_PRECLOSE], 0 );
	}
}

static int
vips_image_close( VipsImage *image )
{
	VipsImageClass *image_class = VIPS_IMAGE_GET_CLASS( image );

	if( !image->close ) {
		image->close = TRUE;

#ifdef DEBUG
		printf( "vips_image_close: " );
		vips_object_print( object );
#endif /*DEBUG*/

		g_signal_emit( image, vips_image_signals[SIG_CLOSE], 0 );
	}
}

static int
vips_image_postclose( VipsImage *image )
{
	VipsImageClass *image_class = VIPS_IMAGE_GET_CLASS( image );

	if( !image->postclose ) {
		image->postclose = TRUE;

#ifdef DEBUG
		printf( "vips_image_postclose: " );
		vips_object_print( object );
#endif /*DEBUG*/

		g_signal_emit( image, vips_image_signals[SIG_POSTCLOSE], 0 );
	}
}

static void
vips_image_finalize( GObject *gobject )
{
	VipsImage *image = VIPS_IMAGE( gobject );

	/* Should be no regions defined on the image, since they all hold a
	 * ref to their host image.
	 */
	g_assert( !image->regions );

	/* Therefore there should be no windows.
	 */
	g_assert( !image->windows );

	/* Junk generate functions. 
	 */
	image->start = NULL;
	image->generate = NULL;
	image->stop = NULL;
	image->client1 = NULL;
	image->client2 = NULL;

	/* No more upstream/downstream links.
	 */
	im__link_break_all( image );

	/* Any file mapping?
	 */
	if( image->baseaddr ) {
		/* MMAP file.
		 */
#ifdef DEBUG_IO
		printf( "im__close: unmapping file ..\n" );
#endif /*DEBUG_IO*/

		im__munmap( image->baseaddr, image->length );
		image->baseaddr = NULL;
		image->length = 0;

		/* This must have been a pointer to the mmap region, rather
		 * than a setbuf.
		 */
		image->data = NULL;
	}

	/* Is there a file descriptor?
	 */
	if( image->fd != -1 ) {
#ifdef DEBUG_IO
		printf( "im__close: closing output file ..\n" );
#endif /*DEBUG_IO*/

		if( image->dtype == IM_OPENOUT )
			(void) im__writehist( image );
		if( close( im->fd ) == -1 ) 
			im_error( "im_close", 
				_( "unable to close fd for %s" ), 
				image->filename );
		im->fd = -1;
	}

	/* Any image data?
	 */
	if( image->data ) {
		/* Buffer image. Only free stuff we know we allocated.
		 */
		if( image->dtype == IM_SETBUF ) {
#ifdef DEBUG_IO
			printf( "im__close: freeing buffer ..\n" );
#endif /*DEBUG_IO*/
			im_free( image->data );
			image->dtype = IM_NONE;
		}

		image->data = NULL;
	}

	vips_image_close( image );

	VIPS_FREE( image->filename );
	VIPS_FREE( image->mode );

	VIPS_FREEF( g_mutex_free, image->sslock );

	VIPS_FREE( image->Hist );
	VIPS_FREEF( im__gslist_gvalue_free, image->history_list );
	im__meta_destroy( image );
	im__time_destroy( image );

	vips_image_postclose( image );

	G_OBJECT_CLASS( vips_image_parent_class )->finalize( gobject );
}

static void
vips_image_dispose( GObject *gobject )
{
#ifdef VIPS_DEBUG
	VIPS_DEBUG_MSG( "vips_image_dispose: " );
	vips_object_print( VIPS_OBJECT( gobject ) );
#endif /*VIPS_DEBUG*/

	G_OBJECT_CLASS( vips_image_parent_class )->dispose( gobject );
}

static void
vips_image_destroy( VipsObject *object )
{
#ifdef VIPS_DEBUG
	VIPS_DEBUG_MSG( "vips_image_destroy: " );
	vips_object_print( VIPS_OBJECT( gobject ) );
#endif /*VIPS_DEBUG*/

	vips_image_preclose( image );

	VIPS_OBJECT_CLASS( vips_image_parent_class )->destroy( object );
}

static void
vips_image_info( VipsObject *object, VipsBuf *buf )
{
	VipsImage *image = VIPS_IMAGE( object );

	vips_buf_appendf( buf, "image->dtype = %d\n", image->dtype ); 
	vips_buf_appendf( buf, "image->demand = %s\n", 
		VIPS_ENUM_STRING( VIPS_TYPE_DEMAND, image->demand ) );
	vips_buf_appendf( buf, "image->magic = 0x%x\n", image->magic );
	vips_buf_appendf( buf, "image->fd = %d\n", image->fd );
	vips_buf_appendf( buf, "image->baseaddr = %p\n", image->baseaddr );
	vips_buf_appendf( buf, "image->length = %#" 
		G_GSIZE_MODIFIER "x\n", image->length );
	vips_buf_appendf( buf, "image->data = %p\n", image->data );
	vips_buf_appendf( buf, "image->sizeof_header = %d\n", 
		image->sizeof_header );

	VIPS_OBJECT_CLASS( parent_class )->info( object, buf );
}

static void
vips_image_generate_caption( VipsObject *object, VipsBuf *buf )
{
	VipsImage *image = VIPS_IMAGE( object );

	vips_buf_appendf( buf, 
		ngettext( 
			"%dx%d %s, %d band, %s", 
			"%dx%d %s, %d bands, %s", image->bands ),
		vips_image_get_width( image ),
		vips_image_get_height( image ),
		VIPS_ENUM_NICK( VIPS_TYPE_FORMAT, 
			vips_image_get_format( image ) ),
		vips_image_get_bands( image ),
		VIPS_ENUM_NICK( VIPS_TYPE_TYPE, 
			vips_image_get_type( image ) ) );
}

static gboolean
vips_format_is_vips( VipsFormatClass *format )
{
	return( strcmp( VIPS_OBJECT_CLASS( format )->nickname, "vips" ) == 0 );
}

/* Lazy open.
 */

/* What we track during a delayed open.
 */
typedef struct {
	VipsImage *out;

	VipsFormatClass *format;/* Read in pixels with this */
	gboolean disc;		/* Read via disc requested */
	VipsImage *image;	/* The real decompressed image */
} Lazy;

static void
lazy_free( Lazy *lazy )
{
	VIPS_UNREF( lazy->image );
}

static Lazy *
lazy_new( VipsImage *out, VipsFormatClass *format, gboolean disc )
{
	Lazy *lazy;

	if( !(lazy = IM_NEW( out, Lazy )) )
		return( NULL );
	lazy->out = out;
	lazy->format = format;
	lazy->disc = disc;
	lazy->image = NULL;
	g_signal_connect( out, "close", lazy_free, NULL );

	return( lazy );
}

typedef struct {
	const char unit;
	int multiplier;
} Unit;

static size_t
parse_size( const char *size_string )
{
	static Unit units[] = {
		{ 'k', 1024 },
		{ 'm', 1024 * 1024 },
		{ 'g', 1024 * 1024 * 1024 }
	};

	size_t size;
	int n;
	int i, j;
	char *unit;

	/* An easy way to alloc a buffer large enough.
	 */
	unit = g_strdup( size_string );
	n = sscanf( size_string, "%d %s", &i, unit );
	if( n > 0 )
		size = i;
	if( n > 1 ) {
		for( j = 0; j < IM_NUMBER( units ); j++ )
			if( tolower( unit[0] ) == units[j].unit ) {
				size *= units[j].multiplier;
				break;
			}
	}
	g_free( unit );

#ifdef DEBUG
	printf( "parse_size: parsed \"%s\" as %zd\n", size_string, size );
#endif /*DEBUG*/

	return( size );
}

static size_t
disc_threshold( void )
{
	static gboolean done = FALSE;
	static size_t threshold;

	if( !done ) {
		const char *env;

		done = TRUE;

		/* 100mb default.
		 */
		threshold = 100 * 1024 * 1024;

		if( (env = g_getenv( "IM_DISC_THRESHOLD" )) ) 
			threshold = parse_size( env );

		if( im__disc_threshold ) 
			threshold = parse_size( im__disc_threshold );

#ifdef DEBUG
		printf( "disc_threshold: %zd bytes\n", threshold );
#endif /*DEBUG*/
	}

	return( threshold );
}

static VipsImage *
lazy_image( Lazy *lazy ) 
{
	VipsImage *image;

	/* We open to disc if:
	 * - 'disc' is set
	 * - disc_threshold() has not been set to zero
	 * - the format does not support lazy read
	 * - the image will be more than a megabyte, uncompressed
	 */
	image = NULL;
	if( lazy->disc && 
		disc_threshold() && 
	        !(vips_format_get_flags( lazy->format, lazy->out->filename ) & 
			VIPS_FORMAT_PARTIAL) ) {
		size_t size = IM_IMAGE_SIZEOF_LINE( lazy->out ) * 
			lazy->out->Ysize;

		if( size > disc_threshold() ) {
			if( !(image = im__open_temp( "%s.v" )) )
				return( NULL );

#ifdef DEBUG
			printf( "lazy_image: opening to disc file \"%s\"\n",
				image->filename );
#endif /*DEBUG*/
		}
	}

	/* Otherwise, fall back to a "p".
	 */
	if( !image && 
		!(image = im_open( lazy->out->filename, "p" )) )
		return( NULL );

	return( image );
}

/* Our start function ... do the lazy open, if necessary, and return a region
 * on the new image.
 */
static void *
open_lazy_start( VipsImage *out, void *a, void *dummy )
{
	Lazy *lazy = (Lazy *) a;
	const char *filename = lazy->out->filename;

	if( !lazy->image ) {
		if( !(lazy->image = lazy_image( lazy )) || 
			lazy->format->load( filename, lazy->image ) ||
			im_pincheck( lazy->image ) ) {
			VIPS_UNREF( lazy->image );
			return( NULL );
		}
	}

	return( im_region_create( lazy->image ) );
}

/* Just copy.
 */
static int
open_lazy_generate( REGION *or, void *seq, void *a, void *b )
{
	REGION *ir = (REGION *) seq;

        Rect *r = &or->valid;

        /* Ask for input we need.
         */
        if( im_prepare( ir, r ) )
                return( -1 );

        /* Attach output region to that.
         */
        if( im_region_region( or, ir, r, r->left, r->top ) )
                return( -1 );

        return( 0 );
}

/* Lazy open ... init the header with the first OpenLazyFn, delay actually
 * decoding pixels with the second OpenLazyFn until the first generate().
 */
static int
vips_open_lazy( VipsImage *out, 
	VipsFormatClass *format, gboolean disc, IMAGE *out )
{
	Lazy *lazy;

	if( !(lazy = lazy_new( out, format, disc )) )
		return( -1 );

	/* Read header fields to init the return image. THINSTRIP since this is
	 * probably a disc file. We can't tell yet whether we will be opening
	 * to memory, sadly, so we can't suggest ANY.
	 */
	if( format->header( out->filename, out ) ||
		im_demand_hint( out, IM_THINSTRIP, NULL ) )
		return( -1 );

	/* Then 'start' creates the real image and 'gen' paints 'out' with 
	 * pixels from the real image on demand.
	 */
	if( im_generate( out, 
		open_lazy_start, open_lazy_generate, im_stop_one, 
		lazy, NULL ) )
		return( -1 );

	return( 0 );
}

/* Lazy save.
 */

/* If we write to (eg.) TIFF, actually do the write
 * to a "p" and on "written" do im_vips2tiff() or whatever. Track save
 * parameters here.
 */
typedef struct {
	int (*save_fn)();	/* Save function */
	char *filename;		/* Save args */
} SaveBlock;

/* From "written" callback: invoke a delayed save.
 */
static int
vips_image_save_cb( VipsImage *image, SaveBlock *sb )
{
	if( sb->save_fn( image, sb->filename ) )
		return( -1 );

	return( 0 );
}

static void
vips_attach_save( VipsImage *image, int (*save_fn)(), const char *filename )
{
	SaveBlock *sb;

	if( (sb = IM_NEW( image, SaveBlock )) ) {
		sb->save_fn = save_fn;
		sb->filename = im_strdup( image, filename );
		g_signal_connect( image, "written", vips_image_save_cb, sb );
	}
}

/* Progress feedback. 
 */

/* What we track during an eval.
 */
typedef struct {
	IMAGE *image;

	int last_percent;	/* The last %complete we displayed */
} Progress;

static int
vips_image_evalstart_cb( Progress *progress )
{
	int tile_width; 
	int tile_height; 
	int nlines;

	progress->last_percent = 0;

	vips_get_tile_size( progress->im, &tile_width, &tile_height, &nlines );
	printf( _( "%s %s: %d threads, %d x %d tiles, groups of %d scanlines" ),
		g_get_prgname(), progress->im->filename,
		im_concurrency_get(),
		tile_width, tile_height, nlines );
	printf( "\n" );

	return( 0 );
}

static int
vips_image_eval_cb( Progress *progress )
{
	IMAGE *im = progress->im;

	if( im->time->percent != progress->last_percent ) {
		printf( _( "%s %s: %d%% complete" ), 
			g_get_prgname(), im->filename, im->time->percent );
		printf( "\r" ); 
		fflush( stdout );

		progress->last_percent = im->time->percent;
	}

	return( 0 );
}

static int
vips_image_evalend_cb( Progress *progress )
{
	IMAGE *im = progress->im;

	/* Spaces at end help to erase the %complete message we overwrite.
	 */
	printf( _( "%s %s: done in %ds          \n" ), 
		g_get_prgname(), im->filename, im->time->run );

	return( 0 );
}

/* Attach progress feedback, if required.
 */
static void
vips_image_add_progress( VipsImage *image )
{
	if( im__progress || 
		g_getenv( "IM_PROGRESS" ) ) {

		Progress *progress = IM_NEW( image, Progress );

		progress->image = image;
		g_signal_connect( image, "evalstart", 
			vips_image_evalstart_cb, progress );
		g_signal_connect( image, "eval", 
			vips_image_eval_cb, progress );
		g_signal_connect( image, "evalend", 
			vips_image_evalend_cb, progress );
	}
}

static int
vips_image_build( VipsObject *object )
{
	VipsImage *image = VIPS_IMAGE (object);
	VipsFormatClass *format;

	VIPS_DEBUG_MSG( "vips_image_build: %p\n", image );

	/* name defaults to filename.
	 */
	if( image->filename ) {
		char *basename;

		basename = g_path_get_basename( image->filename );
		g_object_set( image, "name", basename, NULL );
		g_free( basename );
	}

	if( VIPS_OBJECT_CLASS( parent_class )->build( object ) )
		return( -1 );

	/* Parse the mode string.
	 */
	switch( image->mode[0] ) {
        case 'r':
		if( !(format = vips_format_for_file( image->filename )) )
			return( -1 );

		if( vips_format_is_vips( format ) ) {
			if( vips_open_input( image ) )
				return( -1 );

			if( image->mode[1] == 'w' ) {
				/* "rw" mode ... just sanity check and tag.
				 * The "rw" bit happens when we do an
				 * operation.
				 */

				/* If we have a different byte order 
				 * from the image, we can only process 
				 * 8 bit images.
				 */
				if( vips_image_isMSBfirst( image ) != 
						vips__amiMSBfirst() &&
					vips_format_sizeof( image->format ) !=
						1) {
					im_error( "vips_image_build",
						_( "open for read-"
						"write for native format "
						"images only" ) );
					return( -1 );
				}

				image->dtype = VIPS_IMAGE_TYPE_OPENIN;
			}
		}
		else {
			if( vips_open_lazy( image, 
				format, filename, mode[1] == 'd' ) )
				return( -1 );
		}

        	break;

	case 'w':
		if( (format = vips_format_for_name( filename )) ) {
			if( vips_format_is_vips( format ) ) {
				if( vips_open_output( image ) )
					return( -1 );
			}
			else {
				image->dtype = VIPS_IMAGE_TYPE_PARTIAL;
				vips_attach_save( image, 
					format->save, filename );
			}
		}
		else {
			char suffix[FILENAME_MAX];

			im_filename_suffix( filename, suffix );
			im_error( "vips_image_build", 
				_( "unsupported filetype \"%s\"" ), 
				suffix );

			return( -1 );
		}
        	break;

        case 't':
		image->dtype = VIPS_IMAGE_TYPE_SETBUF;
		image->demand = VIPS_DEMAND_ANY;
                break;

        case 'p':
		image->dtype = VIPS_IMAGE_TYPE_PARTIAL;
                break;

	default:
		im_error( "vips_image_build", _( "bad mode \"%s\"" ), mode );

		return( -1 );
        }

	vips_image_add_progress( image );

#ifdef DEBUG_VIPS
	printf( "vips_image_build: " );
	vips_object_dump( VIPS_OBJECT( image ) );
#endif /*DEBUG_VIPS*/

	return( 0 );
}

static void
vips_image_class_init( VipsImageClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *vobject_class = VIPS_OBJECT_CLASS( class );
	GParamSpec *pspec;
	int i;

	/* Pass in a nonsense name for argv0 ... this init world is only here
	 * for old programs which are missing an im_init_world() call. We must
	 * have threads set up before we can process.
	 */
	if( im_init_world( "vips" ) )
		im_error_clear();

	gobject_class->finalize = vips_image_finalize;
	gobject_class->dispose = vips_image_dispose;
	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	vobject_class->destroy = vips_image_destroy;
	vobject_class->info = vips_image_info;
	vobject_class->generate_caption = vips_image_generate_caption;
	vobject_class->copy_attributes = vips_image_copy_attributes;
	vobject_class->build = vips_image_build;

	/* Create properties.
	 */
	pspec = g_param_spec_int( "width", "Width",
		_( "Image width in pixels" ),
		0, 1000000, 0,
		G_PARAM_READWRITE );
	g_object_class_install_property( gobject_class, PROP_WIDTH, pspec );
	vips_object_class_install_argument( vobject_class, pspec,
		VIPS_ARGUMENT_SET_ONCE, 
		G_STRUCT_OFFSET( VipsImage, Xsize ) );

	pspec = g_param_spec_int( "height", "Height",
		_( "Image height in pixels" ),
		0, 1000000, 0,
		G_PARAM_READWRITE );
	g_object_class_install_property( gobject_class, PROP_HEIGHT, pspec );
	vips_object_class_install_argument( vobject_class, pspec,
		VIPS_ARGUMENT_SET_ONCE, 
		G_STRUCT_OFFSET( VipsImage, Ysize ) );

	pspec = g_param_spec_int( "bands", "Bands",
		_( "Number of bands in image" ),
		0, 1000000, 0, 
		G_PARAM_READWRITE );
	g_object_class_install_property( gobject_class, PROP_BANDS, pspec );
	vips_object_class_install_argument( vobject_class, pspec,
		VIPS_ARGUMENT_SET_ONCE, 
		G_STRUCT_OFFSET( VipsImage, Bands ) );

	pspec = g_param_spec_enum( "format", "Format",
		_( "Pixel format in image" ),
		VIPS_TYPE_FORMAT, VIPS_FORMAT_UNSIGNED8, 
		G_PARAM_READWRITE );
	g_object_class_install_property( gobject_class, PROP_FORMAT, pspec );
	vips_object_class_install_argument( vobject_class, pspec,
		VIPS_ARGUMENT_SET_ONCE, 
		G_STRUCT_OFFSET( VipsImage, BandFmt ) );

	pspec = g_param_spec_string( "filename", "Filename",
		_( "Image filename" ),
		NULL, 
		G_PARAM_READWRITE );
	g_object_class_install_property( gobject_class, PROP_FILENAME, pspec );
	vips_object_class_install_argument( vobject_class, pspec,
		VIPS_ARGUMENT_CONSTRUCT, 
		G_STRUCT_OFFSET( VipsImage, filename ) );

	pspec = g_param_spec_string( "mode", "Mode",
		_( "Open mode" ),
		"p", 			/* Default to partial */
		G_PARAM_READWRITE );
	g_object_class_install_property( gobject_class, PROP_MODE, pspec );
	vips_object_class_install_argument( vobject_class, pspec,
		VIPS_ARGUMENT_CONSTRUCT, 
		G_STRUCT_OFFSET( VipsImage, mode ) );

	pspec = g_param_spec_boolean("kill", "Kill",
		_( "Block evaluation on this image" ),
		FALSE, 
		G_PARAM_READWRITE );
	g_object_class_install_property( gobject_class, PROP_KILL, pspec );
	vips_object_class_install_argument( vobject_class, pspec,
		VIPS_ARGUMENT_NONE, 
		G_STRUCT_OFFSET( VipsImage, kill ) );

	pspec = g_param_spec_enum( "demand", "Demand",
		_( "Preferred demand style for this image" ),
		VIPS_TYPE_DEMAND, VIPS_DEMAND_SMALLTILE,
		G_PARAM_READWRITE );
	g_object_class_install_property( gobject_class, PROP_DEMAND, pspec );
	vips_object_class_install_argument( vobject_class, pspec,
		VIPS_ARGUMENT_NONE, 
		G_STRUCT_OFFSET( VipsImage, demand ) );

	/* Create signals.
	 */
	vips_image_signals[SIG_PRECLOSE] = g_signal_new( "preclose",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( VipsImageClass, preclose ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 );
	vips_image_signals[SIG_CLOSE] = g_signal_new( "close",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( VipsImageClass, close ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 );
	vips_image_signals[SIG_POSTCLOSE] = g_signal_new( "postclose",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( VipsImageClass, postclose ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 );

	vips_image_signals[SIG_PREEVAL] = g_signal_new( "preeval",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( VipsImageClass, preeval ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 );
	vips_image_signals[SIG_EVAL] = g_signal_new( "eval",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( VipsImageClass, eval ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 );
	vips_image_signals[SIG_POSTEVAL] = g_signal_new( "posteval",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET( VipsImageClass, posteval ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 );

	vips_image_signals[SIG_WRITTEN] = g_signal_new( "written",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET( VipsImageClass, written ), 
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0 );
}

static void
vips_image_init( VipsImage *image )
{
	/* Init to 0 is fine for most header fields. Others have default set
	 * by property system.
	 */

	/* Default to native order.
	 */
	image->magic = im_amiMSBfirst() ?  IM_MAGIC_SPARC : IM_MAGIC_INTEL;

	image->fd = -1;			/* since 0 is stdout */
        image->sizeof_header = VIPS_SIZEOF_HEADER;
	image->sslock = g_mutex_new ();
}

/* Set of access functions.
 */

int
vips_image_get_width( VipsImage *image )
{
	return( image->Xsize );
}

int
vips_image_get_height( VipsImage *image )
{
	return( image->Ysize );
}

int
vips_image_get_bands( VipsImage *image )
{
	return( image->Bands );
}

VipsFormat
vips_image_get_format( VipsImage *image )
{
	return( image->BandFmt );
}

VipsCoding
vips_image_get_coding( VipsImage *image )
{
	return( image->Coding );
}

VipsType
vips_image_get_type( VipsImage *image )
{
	return( image->Type );
}

VipsType
vips_image_get_xres( VipsImage *image )
{
	return( image->Xres );
}

VipsType
vips_image_get_yres( VipsImage *image )
{
	return( image->Yres );
}

VipsType
vips_image_get_xoffset( VipsImage *image )
{
	return( image->Xoffset );
}

VipsType
vips_image_get_yoffset( VipsImage *image )
{
	return( image->Yoffset );
}

void
vips_image_written( VipsImage *image )
{
	VipsImageClass *image_class = VIPS_IMAGE_GET_CLASS( image );

#ifdef DEBUG
	printf( "vips_image_written: " );
	vips_object_print( object );
#endif /*DEBUG*/

	g_signal_emit( image, vips_image_signals[SIG_WRITTEN], 0 );
}

void
vips_image_preeval( VipsImage *image )
{
	VipsImageClass *image_class = VIPS_IMAGE_GET_CLASS( image );

#ifdef DEBUG
	printf( "vips_image_preeval: " );
	vips_object_print( object );
#endif /*DEBUG*/

	g_signal_emit( image, vips_image_signals[SIG_PREEVAL], 0 );
}

void
vips_image_eval( VipsImage *image )
{
	VipsImageClass *image_class = VIPS_IMAGE_GET_CLASS( image );

#ifdef DEBUG
	printf( "vips_image_eval: " );
	vips_object_print( object );
#endif /*DEBUG*/

	g_signal_emit( image, vips_image_signals[SIG_EVAL], 0 );
}

void
vips_image_posteval( VipsImage *image )
{
	VipsImageClass *image_class = VIPS_IMAGE_GET_CLASS( image );

#ifdef DEBUG
	printf( "vips_image_posteval: " );
	vips_object_print( object );
#endif /*DEBUG*/

	g_signal_emit( image, vips_image_signals[SIG_POSTEVAL], 0 );
}

/**
 * im_open:
 * @filename: file to open
 * @mode: mode to open with
 *
 * im_open() examines the mode string, and creates an appropriate #IMAGE.
 *
 * <itemizedlist>
 *   <listitem> 
 *     <para>
 *       <emphasis>"r"</emphasis>
 *       opens the named file for reading. If the file is not in the native 
 *       VIPS format for your machine, im_open() automatically converts the 
 *       file for you in memory. 
 *
 *       For some large files (eg. TIFF) this may 
 *       not be what you want, it can fill memory very quickly. Instead, you
 *       can either use "rd" mode (see below), or you can use the lower-level 
 *       API and control the loading process yourself. See 
 *       #VipsFormat. 
 *
 *       im_open() can read files in most formats.
 *
 *       Note that <emphasis>"r"</emphasis> mode works in at least two stages. 
 *       It should return quickly and let you check header fields. It will
 *       only actually read in pixels when you first access them. 
 *     </para>
 *   </listitem>
 *   <listitem> 
 *     <para>
 *       <emphasis>"rd"</emphasis>
 *	 opens the named file for reading. If the uncompressed image is larger 
 *	 than a threshold and the file format does not support random access, 
 *	 rather than uncompressing to memory, im_open() will uncompress to a
 *	 temporary disc file. This file will be automatically deleted when the
 *	 IMAGE is closed.
 *
 *	 See im_system_image() for an explanation of how VIPS selects a
 *	 location for the temporary file.
 *
 *	 The disc threshold can be set with the "--vips-disc-threshold"
 *	 command-line argument, or the IM_DISC_THRESHOLD environment variable.
 *	 The value is a simple integer, but can take a unit postfix of "k", 
 *	 "m" or "g" to indicate kilobytes, megabytes or gigabytes.
 *
 *	 For example:
 *
 *       |[
 *         vips --vips-disc-threshold "500m" im_copy fred.tif fred.v
 *       ]|
 *
 *       will copy via disc if "fred.tif" is more than 500 Mbytes
 *       uncompressed. The default threshold is 100MB.
 *     </para>
 *   </listitem>
 *   <listitem> 
 *     <para>
 *       <emphasis>"w"</emphasis>
 *       opens the named file for writing. It looks at the file name 
 *       suffix to determine the type to write -- for example:
 *
 *       |[
 *         im_open( "fred.tif", "w" )
 *       ]|
 *
 *       will write in TIFF format.
 *     </para>
 *   </listitem>
 *   <listitem> 
 *     <para>
 *       <emphasis>"t"</emphasis>
 *       creates a temporary memory buffer image.
 *     </para>
 *   </listitem>
 *   <listitem> 
 *     <para>
 *       <emphasis>"p"</emphasis>
 *       creates a "glue" descriptor you can use to join two image 
 *       processing operations together.
 *     </para>
 *   </listitem>
 *   <listitem> 
 *     <para>
 *       <emphasis>"rw"</emphasis>
 *       opens the named file for reading and writing. This will only work for 
 *       VIPS files in a format native to your machine. It is only for 
 *       paintbox-type applications.
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 * See also: im_close(), #VipsFormat
 *
 * Returns: the image descriptor on success and NULL on error.
 */
VipsImage *
im_open( const char *filename, const char *mode )
{
	VipsImage *image;

	image = VIPS_IMAGE( g_object_new( VIPS_IMAGE_TYPE, NULL ) );
	g_object_set( image,
		"filename", filename,
		"mode", mode,
		NULL );
	if( vips_object_build( image ) ) {
		VIPS_UNREF( image );
		return( NULL );
	}

	return( image ); 
}
