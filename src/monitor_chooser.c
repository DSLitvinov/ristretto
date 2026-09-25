/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
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
#include "monitor_chooser.h"

#include <glib/gi18n.h>


#define ICON_RESOURCE_ACTIVE "/org/xfce/ristretto/display-active.svg"
#define ICON_RESOURCE_INACTIVE "/org/xfce/ristretto/display-inactive.svg"

/* The icon is drawn on a 64x64 viewBox with the screen at (3,13)-(61,51);
 * a size of 128 keeps the screen on whole pixels. */
#define ICON_SIZE 128
#define ICON_SCREEN_X 6
#define ICON_SCREEN_Y 26
#define CELL_WIDTH 116
#define CELL_HEIGHT 76

#define GRID_SPACING 8
#define GRID_MAX_COLUMNS 4

enum
{
    RSTTO_MONITOR_CHOOSER_SIGNAL_CHANGED = 0,
    RSTTO_MONITOR_CHOOSER_N_SIGNALS
};

static gint rstto_monitor_chooser_signals[RSTTO_MONITOR_CHOOSER_N_SIGNALS];

typedef struct _Monitor Monitor;



static void
rstto_monitor_chooser_finalize (GObject *object);
static gboolean
rstto_monitor_chooser_draw (GtkWidget *widget,
                            cairo_t *cr);
static void
rstto_monitor_chooser_realize (GtkWidget *widget);
static void
rstto_monitor_chooser_get_preferred_width (GtkWidget *widget,
                                           gint *minimal_width,
                                           gint *natural_width);
static void
rstto_monitor_chooser_get_preferred_height (GtkWidget *widget,
                                            gint *minimal_height,
                                            gint *natural_height);
static void
rstto_monitor_chooser_size_allocate (GtkWidget *widget,
                                     GtkAllocation *allocation);

static void
cb_rstto_button_press_event (GtkWidget *widget,
                             GdkEventButton *event);
static void
paint_monitor (GtkWidget *widget,
               cairo_t *cr,
               gint x,
               gint y,
               const gchar *label,
               gboolean active);



struct _Monitor
{
    gint width;
    gint height;
};

struct _RsttoMonitorChooserPrivate
{
    Monitor **monitors;
    gint n_monitors;

    /* Index of the selected grid item, the "All" item comes first */
    gint selected;

    cairo_surface_t *icon_active;
    cairo_surface_t *icon_inactive;
    gint icon_scale;
};



G_DEFINE_TYPE_WITH_PRIVATE (RsttoMonitorChooser, rstto_monitor_chooser, GTK_TYPE_WIDGET)



static void
rstto_monitor_chooser_init (RsttoMonitorChooser *chooser)
{
    chooser->priv = rstto_monitor_chooser_get_instance_private (chooser);
    chooser->priv->selected = 0;
    chooser->priv->monitors = g_new0 (Monitor *, 1);

    g_signal_connect (chooser, "button-press-event",
                      G_CALLBACK (cb_rstto_button_press_event), NULL);

    gtk_widget_set_redraw_on_allocate (GTK_WIDGET (chooser), TRUE);
    gtk_widget_set_events (GTK_WIDGET (chooser),
                           GDK_POINTER_MOTION_MASK);
}

static void
rstto_monitor_chooser_class_init (RsttoMonitorChooserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = rstto_monitor_chooser_finalize;

    widget_class->draw = rstto_monitor_chooser_draw;
    widget_class->realize = rstto_monitor_chooser_realize;
    widget_class->get_preferred_width = rstto_monitor_chooser_get_preferred_width;
    widget_class->get_preferred_height = rstto_monitor_chooser_get_preferred_height;
    widget_class->size_allocate = rstto_monitor_chooser_size_allocate;

    rstto_monitor_chooser_signals[RSTTO_MONITOR_CHOOSER_SIGNAL_CHANGED] =
        g_signal_new ("changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

static void
rstto_monitor_chooser_finalize (GObject *object)
{
    RsttoMonitorChooser *chooser = RSTTO_MONITOR_CHOOSER (object);
    gint id;

    for (id = 0; id < chooser->priv->n_monitors; ++id)
        g_free (chooser->priv->monitors[id]);
    g_free (chooser->priv->monitors);

    g_clear_pointer (&chooser->priv->icon_active, cairo_surface_destroy);
    g_clear_pointer (&chooser->priv->icon_inactive, cairo_surface_destroy);

    G_OBJECT_CLASS (rstto_monitor_chooser_parent_class)->finalize (object);
}

static gboolean
has_all_item (RsttoMonitorChooser *chooser)
{
    return chooser->priv->n_monitors > 1;
}

static gint
get_n_items (RsttoMonitorChooser *chooser)
{
    return chooser->priv->n_monitors + (has_all_item (chooser) ? 1 : 0);
}

static void
get_grid_size (RsttoMonitorChooser *chooser,
               gint *columns,
               gint *rows)
{
    gint n_items = get_n_items (chooser);

    *columns = MAX (1, MIN (GRID_MAX_COLUMNS, n_items));
    *rows = MAX (1, (n_items + *columns - 1) / *columns);
}

static void
get_cell_position (RsttoMonitorChooser *chooser,
                   gint item,
                   gint *x,
                   gint *y)
{
    gint columns, rows, grid_width;

    get_grid_size (chooser, &columns, &rows);
    grid_width = columns * CELL_WIDTH + (columns - 1) * GRID_SPACING;

    *x = (gtk_widget_get_allocated_width (GTK_WIDGET (chooser)) - grid_width) / 2
         + (item % columns) * (CELL_WIDTH + GRID_SPACING);
    *y = (item / columns) * (CELL_HEIGHT + GRID_SPACING);
}

/**
 * rstto_monitor_chooser_realize:
 * @widget:
 *
 */
static void
rstto_monitor_chooser_realize (GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;
    GtkAllocation allocation;
    GdkWindow *window;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (RSTTO_IS_MONITOR_CHOOSER (widget));

    gtk_widget_set_realized (widget, TRUE);

    gtk_widget_get_allocation (widget, &allocation);

    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;
    attributes.visual = gtk_widget_get_visual (widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
    window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
    gtk_widget_set_window (widget, window);
    gdk_window_set_user_data (window, widget);
}

static void
rstto_monitor_chooser_get_preferred_width (GtkWidget *widget,
                                           gint *minimal_width,
                                           gint *natural_width)
{
    gint columns, rows;

    get_grid_size (RSTTO_MONITOR_CHOOSER (widget), &columns, &rows);
    *minimal_width = *natural_width = columns * CELL_WIDTH + (columns - 1) * GRID_SPACING;
}

static void
rstto_monitor_chooser_get_preferred_height (GtkWidget *widget,
                                            gint *minimal_height,
                                            gint *natural_height)
{
    gint columns, rows;

    get_grid_size (RSTTO_MONITOR_CHOOSER (widget), &columns, &rows);
    *minimal_height = *natural_height = rows * CELL_HEIGHT + (rows - 1) * GRID_SPACING;
}

static void
rstto_monitor_chooser_size_allocate (GtkWidget *widget,
                                     GtkAllocation *allocation)
{
    gtk_widget_set_allocation (widget, allocation);
    if (gtk_widget_get_realized (widget))
    {
        gdk_window_move_resize (gtk_widget_get_window (widget),
                                allocation->x, allocation->y,
                                allocation->width, allocation->height);
    }
}

static cairo_surface_t *
load_icon (GtkWidget *widget,
           const gchar *resource,
           gint scale)
{
    cairo_surface_t *surface;
    GdkPixbuf *pixbuf;
    GError *error = NULL;

    pixbuf = gdk_pixbuf_new_from_resource_at_scale (resource, ICON_SIZE * scale, ICON_SIZE * scale,
                                                    TRUE, &error);
    if (pixbuf == NULL)
    {
        g_warning ("Failed to load icon '%s': %s", resource, error->message);
        g_error_free (error);
        return NULL;
    }

    surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, gtk_widget_get_window (widget));
    g_object_unref (pixbuf);

    return surface;
}

static void
update_icons (RsttoMonitorChooser *chooser)
{
    GtkWidget *widget = GTK_WIDGET (chooser);
    gint scale = gtk_widget_get_scale_factor (widget);

    if (scale == chooser->priv->icon_scale)
        return;

    g_clear_pointer (&chooser->priv->icon_active, cairo_surface_destroy);
    g_clear_pointer (&chooser->priv->icon_inactive, cairo_surface_destroy);
    chooser->priv->icon_active = load_icon (widget, ICON_RESOURCE_ACTIVE, scale);
    chooser->priv->icon_inactive = load_icon (widget, ICON_RESOURCE_INACTIVE, scale);
    chooser->priv->icon_scale = scale;
}

static gboolean
rstto_monitor_chooser_draw (GtkWidget *widget,
                            cairo_t *cr)
{
    RsttoMonitorChooser *chooser = RSTTO_MONITOR_CHOOSER (widget);
    gboolean all_item = has_all_item (chooser);
    gchar *label;
    gint item, x, y;

    gtk_render_background (gtk_widget_get_style_context (widget), cr, 0, 0,
                           gtk_widget_get_allocated_width (widget),
                           gtk_widget_get_allocated_height (widget));

    update_icons (chooser);

    for (item = 0; item < get_n_items (chooser); ++item)
    {
        if (all_item && item == 0)
            label = g_strdup (_("All"));
        else
            label = g_strdup_printf ("%d", all_item ? item : item + 1);

        get_cell_position (chooser, item, &x, &y);
        cairo_save (cr);
        paint_monitor (widget, cr, x, y, label, item == chooser->priv->selected);
        cairo_restore (cr);
        g_free (label);
    }

    return FALSE;
}

static void
paint_monitor (GtkWidget *widget,
               cairo_t *cr,
               gint x,
               gint y,
               const gchar *label,
               gboolean active)
{
    RsttoMonitorChooser *chooser = RSTTO_MONITOR_CHOOSER (widget);
    cairo_surface_t *icon = active ? chooser->priv->icon_active : chooser->priv->icon_inactive;
    PangoLayout *layout;
    PangoFontDescription *font_description;
    gint text_width = 0;
    gint text_height = 0;

    if (icon)
    {
        cairo_set_source_surface (cr, icon, x - ICON_SCREEN_X, y - ICON_SCREEN_Y);
        cairo_paint (cr);
    }

    font_description = pango_font_description_copy (
        pango_context_get_font_description (gtk_widget_get_pango_context (widget)));
    pango_font_description_set_weight (font_description, PANGO_WEIGHT_BOLD);
    pango_font_description_set_absolute_size (font_description, CELL_HEIGHT * 0.3 * PANGO_SCALE);

    layout = pango_cairo_create_layout (cr);
    pango_layout_set_font_description (layout, font_description);
    pango_layout_set_text (layout, label, -1);
    pango_layout_get_pixel_size (layout, &text_width, &text_height);

    cairo_move_to (cr, x + (CELL_WIDTH - text_width) / 2.0, y + (CELL_HEIGHT - text_height) / 2.0);
    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
    pango_cairo_show_layout (cr, layout);

    g_object_unref (layout);
    pango_font_description_free (font_description);
}

/**
 * rstto_monitor_chooser_new:
 *
 * @Returns: a new monitor-chooser object.
 */
GtkWidget *
rstto_monitor_chooser_new (void)
{
    RsttoMonitorChooser *chooser;

    chooser = g_object_new (RSTTO_TYPE_MONITOR_CHOOSER, NULL);

    return GTK_WIDGET (chooser);
}

/**
 * rstto_monitor_chooser_add:
 * @chooser: MonitorChooser
 * @width:   monitor-width (pixels)
 * @height:  monitor-height (pixels)
 *
 * Add a monitor to the monitor-chooser. The selection is reset to "All"
 * (or to the only monitor).
 */
gint
rstto_monitor_chooser_add (RsttoMonitorChooser *chooser,
                           gint width,
                           gint height)
{
    Monitor **monitors = g_new0 (Monitor *, chooser->priv->n_monitors + 2);
    gint id;

    Monitor *monitor = g_new0 (Monitor, 1);
    monitor->width = width;
    monitor->height = height;

    for (id = 0; id < chooser->priv->n_monitors; ++id)
        monitors[id] = chooser->priv->monitors[id];
    g_free (chooser->priv->monitors);

    monitors[id] = monitor;

    chooser->priv->monitors = monitors;
    chooser->priv->n_monitors++;
    chooser->priv->selected = 0;

    gtk_widget_queue_resize (GTK_WIDGET (chooser));

    return id;
}

/**
 * cb_rstto_button_press_event:
 * @widget: Monitor-Chooser widget
 * @event:  Event
 *
 * Switch the monitor based on the location where a user clicks.
 */
static void
cb_rstto_button_press_event (GtkWidget *widget,
                             GdkEventButton *event)
{
    RsttoMonitorChooser *chooser = RSTTO_MONITOR_CHOOSER (widget);
    gint item, x, y;

    for (item = 0; item < get_n_items (chooser); ++item)
    {
        get_cell_position (chooser, item, &x, &y);
        if (event->x >= x && event->x < x + CELL_WIDTH
            && event->y >= y && event->y < y + CELL_HEIGHT)
        {
            if (item != chooser->priv->selected)
            {
                chooser->priv->selected = item;

                g_signal_emit (chooser,
                               rstto_monitor_chooser_signals[RSTTO_MONITOR_CHOOSER_SIGNAL_CHANGED],
                               0, NULL);

                gtk_widget_queue_draw (widget);
            }
            break;
        }
    }
}

/**
 * rstto_monitor_chooser_get_selected:
 * @chooser: The monitor-chooser widget
 *
 * Returns the id of the selected monitor, or RSTTO_MONITOR_CHOOSER_ALL.
 */
gint
rstto_monitor_chooser_get_selected (RsttoMonitorChooser *chooser)
{
    if (has_all_item (chooser))
        return chooser->priv->selected - 1;

    return chooser->priv->selected;
}

/**
 * rstto_monitor_chooser_get_dimensions:
 * @chooser: The monitor-chooser widget
 * @nr:      The monitor-number, RSTTO_MONITOR_CHOOSER_ALL means the first monitor
 * @width:   A gint to store the width of the monitor (in pixels)
 * @height:  A gint to store the height of the monitor (in pixels)
 *
 * Returns the dimensions of a monitor identified by 'nr'.
 */
void
rstto_monitor_chooser_get_dimensions (RsttoMonitorChooser *chooser,
                                      gint nr,
                                      gint *width,
                                      gint *height)
{
    if (nr == RSTTO_MONITOR_CHOOSER_ALL)
        nr = 0;

    g_return_if_fail (nr >= 0 && nr < chooser->priv->n_monitors);

    *width = chooser->priv->monitors[nr]->width;
    *height = chooser->priv->monitors[nr]->height;
}
