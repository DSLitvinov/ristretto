/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "util.h"
#include "wallpaper_preview.h"



#define PREVIEW_MIN_WIDTH 400
#define PREVIEW_PIXBUF_SIZE 1024

struct _RsttoWallpaperPreview
{
    GtkDrawingArea parent;

    GdkPixbuf *pixbuf;
    gint image_width;
    gint image_height;
    gint monitor_width;
    gint monitor_height;
    RsttoWallpaperStyle style;
};



G_DEFINE_TYPE (RsttoWallpaperPreview, rstto_wallpaper_preview, GTK_TYPE_DRAWING_AREA)



static void
rstto_wallpaper_preview_finalize (GObject *object)
{
    RsttoWallpaperPreview *preview = RSTTO_WALLPAPER_PREVIEW (object);

    g_clear_object (&preview->pixbuf);

    G_OBJECT_CLASS (rstto_wallpaper_preview_parent_class)->finalize (object);
}

static GtkSizeRequestMode
rstto_wallpaper_preview_get_request_mode (GtkWidget *widget)
{
    return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
rstto_wallpaper_preview_get_preferred_width (GtkWidget *widget,
                                             gint *minimal_width,
                                             gint *natural_width)
{
    *minimal_width = *natural_width = PREVIEW_MIN_WIDTH;
}

static void
rstto_wallpaper_preview_get_preferred_height_for_width (GtkWidget *widget,
                                                        gint width,
                                                        gint *minimal_height,
                                                        gint *natural_height)
{
    RsttoWallpaperPreview *preview = RSTTO_WALLPAPER_PREVIEW (widget);

    *minimal_height = *natural_height = width * preview->monitor_height / preview->monitor_width;
}

static void
rstto_wallpaper_preview_get_preferred_height (GtkWidget *widget,
                                              gint *minimal_height,
                                              gint *natural_height)
{
    rstto_wallpaper_preview_get_preferred_height_for_width (widget, PREVIEW_MIN_WIDTH,
                                                            minimal_height, natural_height);
}

static gboolean
rstto_wallpaper_preview_draw (GtkWidget *widget,
                              cairo_t *cr)
{
    RsttoWallpaperPreview *preview = RSTTO_WALLPAPER_PREVIEW (widget);
    gdouble width = gtk_widget_get_allocated_width (widget);
    gdouble height = gtk_widget_get_allocated_height (widget);
    gdouble pixbuf_width, pixbuf_height, dest_width, dest_height, scale;

    cairo_rectangle (cr, 0, 0, width, height);
    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    cairo_fill_preserve (cr);
    cairo_clip (cr);

    if (preview->pixbuf == NULL)
        return FALSE;

    pixbuf_width = gdk_pixbuf_get_width (preview->pixbuf);
    pixbuf_height = gdk_pixbuf_get_height (preview->pixbuf);

    switch (preview->style)
    {
        case RSTTO_WALLPAPER_STYLE_CENTERED:
        case RSTTO_WALLPAPER_STYLE_TILED:
            /* The image keeps its real size relative to the monitor */
            scale = width / preview->monitor_width;
            dest_width = preview->image_width * scale;
            dest_height = preview->image_height * scale;
            break;
        case RSTTO_WALLPAPER_STYLE_STRETCHED:
            dest_width = width;
            dest_height = height;
            break;
        case RSTTO_WALLPAPER_STYLE_ZOOMED:
            scale = MAX (width / pixbuf_width, height / pixbuf_height);
            dest_width = pixbuf_width * scale;
            dest_height = pixbuf_height * scale;
            break;
        case RSTTO_WALLPAPER_STYLE_AUTOMATIC:
        case RSTTO_WALLPAPER_STYLE_SCALED:
        default:
            scale = MIN (width / pixbuf_width, height / pixbuf_height);
            dest_width = pixbuf_width * scale;
            dest_height = pixbuf_height * scale;
            break;
    }

    if (preview->style != RSTTO_WALLPAPER_STYLE_TILED)
        cairo_translate (cr, (width - dest_width) / 2.0, (height - dest_height) / 2.0);

    cairo_scale (cr, dest_width / pixbuf_width, dest_height / pixbuf_height);
    gdk_cairo_set_source_pixbuf (cr, preview->pixbuf, 0, 0);
    cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_GOOD);
    if (preview->style == RSTTO_WALLPAPER_STYLE_TILED)
        cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
    cairo_paint (cr);

    return FALSE;
}

static void
rstto_wallpaper_preview_class_init (RsttoWallpaperPreviewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = rstto_wallpaper_preview_finalize;

    widget_class->draw = rstto_wallpaper_preview_draw;
    widget_class->get_request_mode = rstto_wallpaper_preview_get_request_mode;
    widget_class->get_preferred_width = rstto_wallpaper_preview_get_preferred_width;
    widget_class->get_preferred_height = rstto_wallpaper_preview_get_preferred_height;
    widget_class->get_preferred_height_for_width = rstto_wallpaper_preview_get_preferred_height_for_width;
}

static void
rstto_wallpaper_preview_init (RsttoWallpaperPreview *preview)
{
    preview->monitor_width = 16;
    preview->monitor_height = 9;
    preview->style = RSTTO_WALLPAPER_STYLE_AUTOMATIC;
}

GtkWidget *
rstto_wallpaper_preview_new (void)
{
    return g_object_new (RSTTO_TYPE_WALLPAPER_PREVIEW, NULL);
}

void
rstto_wallpaper_preview_set_file (RsttoWallpaperPreview *preview,
                                  const gchar *path)
{
    g_clear_object (&preview->pixbuf);
    preview->pixbuf = gdk_pixbuf_new_from_file_at_size (path, PREVIEW_PIXBUF_SIZE, PREVIEW_PIXBUF_SIZE, NULL);

    if (gdk_pixbuf_get_file_info (path, &preview->image_width, &preview->image_height) == NULL
        && preview->pixbuf != NULL)
    {
        preview->image_width = gdk_pixbuf_get_width (preview->pixbuf);
        preview->image_height = gdk_pixbuf_get_height (preview->pixbuf);
    }

    gtk_widget_queue_draw (GTK_WIDGET (preview));
}

void
rstto_wallpaper_preview_set_monitor_size (RsttoWallpaperPreview *preview,
                                          gint width,
                                          gint height)
{
    g_return_if_fail (width > 0 && height > 0);

    preview->monitor_width = width;
    preview->monitor_height = height;
    gtk_widget_queue_resize (GTK_WIDGET (preview));
}

void
rstto_wallpaper_preview_set_style (RsttoWallpaperPreview *preview,
                                   RsttoWallpaperStyle style)
{
    preview->style = style;
    gtk_widget_queue_draw (GTK_WIDGET (preview));
}
