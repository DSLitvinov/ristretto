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

#ifndef __RISTRETTO_WALLPAPER_PREVIEW_H__
#define __RISTRETTO_WALLPAPER_PREVIEW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Values match the xfdesktop "image-style" property */
typedef enum
{
    RSTTO_WALLPAPER_STYLE_AUTOMATIC = 0,
    RSTTO_WALLPAPER_STYLE_CENTERED,
    RSTTO_WALLPAPER_STYLE_TILED,
    RSTTO_WALLPAPER_STYLE_STRETCHED,
    RSTTO_WALLPAPER_STYLE_SCALED,
    RSTTO_WALLPAPER_STYLE_ZOOMED
} RsttoWallpaperStyle;

#define RSTTO_TYPE_WALLPAPER_PREVIEW rstto_wallpaper_preview_get_type ()
G_DECLARE_FINAL_TYPE (RsttoWallpaperPreview, rstto_wallpaper_preview, RSTTO, WALLPAPER_PREVIEW, GtkDrawingArea)

GtkWidget *
rstto_wallpaper_preview_new (void);

void
rstto_wallpaper_preview_set_file (RsttoWallpaperPreview *preview,
                                  const gchar *path);

void
rstto_wallpaper_preview_set_monitor_size (RsttoWallpaperPreview *preview,
                                          gint width,
                                          gint height);

void
rstto_wallpaper_preview_set_style (RsttoWallpaperPreview *preview,
                                   RsttoWallpaperStyle style);

G_END_DECLS

#endif /* __RISTRETTO_WALLPAPER_PREVIEW_H__ */
