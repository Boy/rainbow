/**
 * @file llmediaimplgstreamervidplug.h
 * @brief Video-consuming static GStreamer plugin for gst-to-LLMediaImpl
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#if LL_GSTREAMER_ENABLED

#include "linden_common.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>

#include "llmediaimplgstreamer_syms.h"

#include "llthread.h"

#include "llmediaimplgstreamervidplug.h"

GST_DEBUG_CATEGORY_STATIC (gst_slvideo_debug);
#define GST_CAT_DEFAULT gst_slvideo_debug

/* Filter signals and args */
enum
{
	/* FILL ME */
	LAST_SIGNAL
};

enum
{
	ARG_0
};

#define SLV_SIZECAPS ", width=(int){1,2,4,8,16,32,64,128,256,512,1024}, height=(int){1,2,4,8,16,32,64,128,256,512,1024} "
#define SLV_ALLCAPS GST_VIDEO_CAPS_RGBx SLV_SIZECAPS ";" GST_VIDEO_CAPS_BGRx SLV_SIZECAPS

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SLV_ALLCAPS)
    );

GST_BOILERPLATE (GstSLVideo, gst_slvideo, GstVideoSink,
    GST_TYPE_VIDEO_SINK);

static void gst_slvideo_set_property (GObject * object, guint prop_id,
				      const GValue * value,
				      GParamSpec * pspec);
static void gst_slvideo_get_property (GObject * object, guint prop_id,
				      GValue * value, GParamSpec * pspec);

static void
gst_slvideo_base_init (gpointer gclass)
{
	static GstElementDetails element_details = {
		"PluginTemplate",
		"Generic/PluginTemplate",
		"Generic Template Element",
		"Linden Lab"
	};
	GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);
	
	llgst_element_class_add_pad_template (element_class,
			      llgst_static_pad_template_get (&sink_factory));
	llgst_element_class_set_details (element_class, &element_details);
}


static void
gst_slvideo_finalize (GObject * object)
{
	GstSLVideo *slvideo;
	slvideo = GST_SLVIDEO (object);
	if (slvideo->caps)
	{
		llgst_caps_unref(slvideo->caps);
	}

	G_OBJECT_CLASS(parent_class)->finalize (object);
}


static GstFlowReturn
gst_slvideo_show_frame (GstBaseSink * bsink, GstBuffer * buf)
{
	GstSLVideo *slvideo;
	llg_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);
	
	slvideo = GST_SLVIDEO(bsink);
	
#if 0
	fprintf(stderr, "\n\ntransferring a frame of %dx%d <- %p (%d)\n\n",
		slvideo->width, slvideo->height, GST_BUFFER_DATA(buf),
		slvideo->format);
#endif
	if (GST_BUFFER_DATA(buf))
	{
		// copy frame and frame info into neutral territory
		GST_OBJECT_LOCK(slvideo);
		slvideo->retained_frame_ready = TRUE;
		slvideo->retained_frame_width = slvideo->width;
		slvideo->retained_frame_height = slvideo->height;
		slvideo->retained_frame_format = slvideo->format;
		int rowbytes = 
			SLVPixelFormatBytes[slvideo->retained_frame_format] *
			slvideo->retained_frame_width;
		int needbytes = rowbytes * slvideo->retained_frame_width;
		// resize retained frame hunk only if necessary
		if (needbytes != slvideo->retained_frame_allocbytes)
		{
			delete[] slvideo->retained_frame_data;
			slvideo->retained_frame_data = new unsigned char[needbytes];
			slvideo->retained_frame_allocbytes = needbytes;
			
		}
		// copy the actual frame data to neutral territory -
		// flipped, for GL reasons
		for (int ypos=0; ypos<slvideo->height; ++ypos)
		{
			memcpy(&slvideo->retained_frame_data[(slvideo->height-1-ypos)*rowbytes],
			       &(((unsigned char*)GST_BUFFER_DATA(buf))[ypos*rowbytes]),
			       rowbytes);
		}
		// done with the shared data
		GST_OBJECT_UNLOCK(slvideo);
	}

	return GST_FLOW_OK;
}


static GstStateChangeReturn
gst_slvideo_change_state(GstElement * element, GstStateChange transition)
{
	GstSLVideo *slvideo;
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	
	slvideo = GST_SLVIDEO (element);

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
	if (ret == GST_STATE_CHANGE_FAILURE)
		return ret;

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		slvideo->fps_n = 0;
		slvideo->fps_d = 1;
		GST_VIDEO_SINK_WIDTH(slvideo) = 0;
		GST_VIDEO_SINK_HEIGHT(slvideo) = 0;
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		break;
	default:
		break;
	}

	return ret;
}


static GstCaps *
gst_slvideo_get_caps (GstBaseSink * bsink)
{
	GstSLVideo *slvideo;
	slvideo = GST_SLVIDEO(bsink);
	
	return llgst_caps_ref (slvideo->caps);
}


/* this function handles the link with other elements */
static gboolean
gst_slvideo_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
	GstSLVideo *filter;
	GstStructure *structure;
	GstCaps *intersection;
	
	GST_DEBUG ("set caps with %" GST_PTR_FORMAT, caps);
	
	filter = GST_SLVIDEO(bsink);
	
	intersection = llgst_caps_intersect (filter->caps, caps);
	if (llgst_caps_is_empty (intersection))
	{
		// no overlap between our caps and requested caps
		return FALSE;
	}
	llgst_caps_unref(intersection);
	
	int width, height;
	gboolean ret;
	const GValue *fps;
	const GValue *par;
	structure = llgst_caps_get_structure (caps, 0);
	ret = llgst_structure_get_int (structure, "width", &width);
	ret = ret && llgst_structure_get_int (structure, "height", &height);
	fps = llgst_structure_get_value (structure, "framerate");
	ret = ret && (fps != NULL);
	par = llgst_structure_get_value (structure, "pixel-aspect-ratio");
	if (!ret)
		return FALSE;

	filter->width = width;
	filter->height = height;
	filter->fps_n = llgst_value_get_fraction_numerator(fps);
	filter->fps_d = llgst_value_get_fraction_denominator(fps);
	if (par)
	{
		filter->par_n = llgst_value_get_fraction_numerator(par);
		filter->par_d = llgst_value_get_fraction_denominator(par);
	}
	else
	{
		filter->par_n = 1;
		filter->par_d = 1;
	}
	GST_VIDEO_SINK_WIDTH(filter) = width;
	GST_VIDEO_SINK_HEIGHT(filter) = height;
	
	filter->format = SLV_PF_UNKNOWN;
	if (0 == strcmp(llgst_structure_get_name(structure),
			"video/x-raw-rgb"))
	{
		int red_mask;
		int green_mask;
		int blue_mask;
		llgst_structure_get_int(structure, "red_mask", &red_mask);
		llgst_structure_get_int(structure, "green_mask", &green_mask);
		llgst_structure_get_int(structure, "blue_mask", &blue_mask);
		if ((unsigned int)red_mask   == 0xFF000000 &&
		    (unsigned int)green_mask == 0x00FF0000 &&
		    (unsigned int)blue_mask  == 0x0000FF00)
		{
			filter->format = SLV_PF_RGBX;
			//fprintf(stderr, "\n\nPIXEL FORMAT RGB\n\n");
		} else if ((unsigned int)red_mask   == 0x0000FF00 &&
			   (unsigned int)green_mask == 0x00FF0000 &&
			   (unsigned int)blue_mask  == 0xFF000000)
		{
			filter->format = SLV_PF_BGRX;
			//fprintf(stderr, "\n\nPIXEL FORMAT BGR\n\n");
		}
	}
	
	return TRUE;
}


static gboolean
gst_slvideo_start (GstBaseSink * bsink)
{
	GstSLVideo *slvideo;
	gboolean ret = TRUE;
	
	slvideo = GST_SLVIDEO(bsink);

	return ret;
}

static gboolean
gst_slvideo_stop (GstBaseSink * bsink)
{
	GstSLVideo *slvideo;
	slvideo = GST_SLVIDEO(bsink);

	// free-up retained frame buffer
	GST_OBJECT_LOCK(slvideo);
	slvideo->retained_frame_ready = FALSE;
	delete[] slvideo->retained_frame_data;
	slvideo->retained_frame_data = NULL;
	slvideo->retained_frame_allocbytes = 0;
	GST_OBJECT_UNLOCK(slvideo);

	return TRUE;
}


static gboolean
gst_slvideo_unlock (GstBaseSink * bsink)
{
	// nothing really to do here.
	return TRUE;
}


/* initialize the plugin's class */
static void
gst_slvideo_class_init (GstSLVideoClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseSinkClass *gstbasesink_class;
	
	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	gstbasesink_class = (GstBaseSinkClass *) klass;
	
	gobject_class->finalize = gst_slvideo_finalize;
	gobject_class->set_property = gst_slvideo_set_property;
	gobject_class->get_property = gst_slvideo_get_property;
	
	gstelement_class->change_state = gst_slvideo_change_state;
	
	gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_slvideo_get_caps);
	gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR( gst_slvideo_set_caps);
	//gstbasesink_class->buffer_alloc=GST_DEBUG_FUNCPTR(gst_slvideo_buffer_alloc);
	//gstbasesink_class->get_times = GST_DEBUG_FUNCPTR (gst_slvideo_get_times);
	gstbasesink_class->preroll = GST_DEBUG_FUNCPTR (gst_slvideo_show_frame);
	gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_slvideo_show_frame);
	
	gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_slvideo_start);
	gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_slvideo_stop);
	
	gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_slvideo_unlock);
}


static void
gst_slvideo_update_caps (GstSLVideo * slvideo)
{
	GstCaps *caps;

	// GStreamer will automatically convert colourspace if necessary.
	// GStreamer will automatically resize media to one of these enumerated
	// powers-of-two that we ask for (yay GStreamer!)
	caps = llgst_caps_from_string (SLV_ALLCAPS);
	
	llgst_caps_replace (&slvideo->caps, caps);
}


/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_slvideo_init (GstSLVideo * filter,
		  GstSLVideoClass * gclass)
{
	filter->width = -1;
	filter->height = -1;

	// this is the info we share with the client app
	GST_OBJECT_LOCK(filter);
	filter->retained_frame_ready = FALSE;
	filter->retained_frame_data = NULL;
	filter->retained_frame_allocbytes = 0;
	filter->retained_frame_width = filter->width;
	filter->retained_frame_height = filter->height;
	filter->retained_frame_format = SLV_PF_UNKNOWN;
	GST_OBJECT_UNLOCK(filter);
	
	gst_slvideo_update_caps(filter);
}

static void
gst_slvideo_set_property (GObject * object, guint prop_id,
			  const GValue * value, GParamSpec * pspec)
{
	llg_return_if_fail (GST_IS_SLVIDEO (object));
	
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_slvideo_get_property (GObject * object, guint prop_id,
			  GValue * value, GParamSpec * pspec)
{
	llg_return_if_fail (GST_IS_SLVIDEO (object));

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
	//fprintf(stderr, "\n\n\nPLUGIN INIT\n\n\n");

	GST_DEBUG_CATEGORY_INIT (gst_slvideo_debug, "private-slvideo-plugin",
				 0, "Second Life Video Sink");

	return llgst_element_register (plugin, "private-slvideo",
				       GST_RANK_NONE, GST_TYPE_SLVIDEO);
}

/* this is the structure that gstreamer looks for to register plugins
 */
/* NOTE: Can't rely upon GST_PLUGIN_DEFINE_STATIC to self-register, since
   some g++ versions buggily avoid __attribute__((constructor)) functions -
   so we provide an explicit plugin init function.
 */
void gst_slvideo_init_class (void)
{
#define PACKAGE "packagehack"
	static GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
				  GST_VERSION_MINOR,
				  "private-slvideoplugin", 
				  "SL Video sink plugin",
				  plugin_init, "0.1", GST_LICENSE_UNKNOWN,
				  "Second Life",
				  "http://www.secondlife.com/");
#undef PACKAGE
	ll_gst_plugin_register_static (&gst_plugin_desc);
	//fprintf(stderr, "\n\n\nCLASS INIT\n\n\n");
}

#endif // LL_GSTREAMER_ENABLED
