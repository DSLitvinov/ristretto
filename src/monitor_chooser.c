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



#define RSTTO_MAX_MONITORS 9

#define ICON_SIZE 96
#define ICON_SPACING 12
#define ICON_RESOURCE_ACTIVE "/org/xfce/ristretto/display-active.svg"
#define ICON_RESOURCE_INACTIVE "/org/xfce/ristretto/display-inactive.svg"

/* Screen rectangle inside the 64x64 icon viewBox */
#define ICON_SCREEN_X1 (3.0 / 64.0)
#define ICON_SCREEN_X2 (61.0 / 64.0)
#define ICON_SCREEN_Y1 (13.0 / 64.0)
#define ICON_SCREEN_Y2 (51.0 / 64.0)

enum
{
    RSTTO_MONITOR_CHOOSER_SIGNAL_CHANGED = 0,
    RSTTO_MONITOR_CHOOSER_N_SIGNALS
};

static gint rstto_monitor_chooser_signals[RSTTO_MONITOR_CHOOSER_N_SIGNALS];

typedef struct _Monitor Monitor;
typedef struct _MonitorPosition MonitorPosition;



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


static gboolean
rstto_monitor_chooser_paint (GtkWidget *widget,
                             cairo_t *ctx);
static void
cb_rstto_button_press_event (GtkWidget *widget,
                             GdkEventButton *event);
static void
paint_monitor (GtkWidget *widget,
               cairo_t *cr,
               gint x,
               gint y,
               gint size,
               const gchar *label,
               gboolean active);



struct _Monitor
{
    gint width;
    gint height;

    cairo_surface_t *image_surface;
};

struct _MonitorPosition
{
    guint x;
    guint y;
    guint width;
    guint height;
};

struct _RsttoMonitorChooserPrivate
{
    Monitor **monitors;
    gint n_monitors;
    gint selected;

    cairo_surface_t *icon_active;
    cairo_surface_t *icon_inactive;
    gint icon_pixel_size;
};



G_DEFINE_TYPE_WITH_PRIVATE (RsttoMonitorChooser, rstto_monitor_chooser, GTK_TYPE_WIDGET)



static void
rstto_monitor_chooser_init (RsttoMonitorChooser *chooser)
{
    chooser->priv = rstto_monitor_chooser_get_instance_private (chooser);
    chooser->priv->selected = -1;
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

    g_clear_pointer (&chooser->priv->icon_active, cairo_surface_destroy);
    g_clear_pointer (&chooser->priv->icon_inactive, cairo_surface_destroy);

    G_OBJECT_CLASS (rstto_monitor_chooser_parent_class)->finalize (object);
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
    // TODO: comment out for now
    // attributes.colormap = gtk_widget_get_colormap (widget);

    // TODO: comment out for now
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL /*| GDK_WA_COLORMAP*/;
    window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
    gtk_widget_set_window (widget, window);
    gdk_window_set_user_data (window, widget);
}

static void
rstto_monitor_chooser_get_preferred_width (GtkWidget *widget,
                                           gint *minimal_width,
                                           gint *natural_width)
{
    *minimal_width = *natural_width = 400;
}

static void
rstto_monitor_chooser_get_preferred_height (GtkWidget *widget,
                                            gint *minimal_height,
                                            gint *natural_height)
{
    *minimal_height = *natural_height = ICON_SIZE;
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

static gboolean
rstto_monitor_chooser_draw (GtkWidget *widget,
                            cairo_t *cr)
{
    cairo_save (cr);
    rstto_monitor_chooser_paint (widget, cr);
    cairo_restore (cr);
    return FALSE;
}

static void
get_icon_layout (RsttoMonitorChooser *chooser,
                 gint alloc_width,
                 gint alloc_height,
                 gint *size,
                 gint *x,
                 gint *y)
{
    gint n = chooser->priv->n_monitors;

    *size = MIN (ICON_SIZE, (alloc_width - (n - 1) * ICON_SPACING) / n);
    *size = MIN (*size, alloc_height);
    *x = (alloc_width - (n * *size + (n - 1) * ICON_SPACING)) / 2;
    *y = (alloc_height - *size) / 2;
}

static cairo_surface_t *
load_icon (GtkWidget *widget,
           const gchar *resource,
           gint pixel_size)
{
    cairo_surface_t *surface;
    GdkPixbuf *pixbuf;
    GError *error = NULL;

    pixbuf = gdk_pixbuf_new_from_resource_at_scale (resource, pixel_size, pixel_size, TRUE, &error);
    if (pixbuf == NULL)
    {
        g_warning ("Failed to load icon '%s': %s", resource, error->message);
        g_error_free (error);
        return NULL;
    }

    surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, gtk_widget_get_scale_factor (widget),
                                                    gtk_widget_get_window (widget));
    g_object_unref (pixbuf);

    return surface;
}

static void
update_icons (RsttoMonitorChooser *chooser,
              gint size)
{
    GtkWidget *widget = GTK_WIDGET (chooser);
    gint pixel_size = size * gtk_widget_get_scale_factor (widget);

    if (pixel_size == chooser->priv->icon_pixel_size)
        return;

    g_clear_pointer (&chooser->priv->icon_active, cairo_surface_destroy);
    g_clear_pointer (&chooser->priv->icon_inactive, cairo_surface_destroy);
    chooser->priv->icon_active = load_icon (widget, ICON_RESOURCE_ACTIVE, pixel_size);
    chooser->priv->icon_inactive = load_icon (widget, ICON_RESOURCE_INACTIVE, pixel_size);
    chooser->priv->icon_pixel_size = pixel_size;
}

static gboolean
rstto_monitor_chooser_paint (GtkWidget *widget,
                             cairo_t *ctx)
{
    RsttoMonitorChooser *chooser = RSTTO_MONITOR_CHOOSER (widget);
    gchar *label;
    gint id, size, x, y;
    gint alloc_width = gtk_widget_get_allocated_width (widget);
    gint alloc_height = gtk_widget_get_allocated_height (widget);
    GtkStyleContext *context = gtk_widget_get_style_context (widget);

    gtk_render_background (context, ctx, 0, 0, alloc_width, alloc_height);

    if (chooser->priv->n_monitors == 0)
        return FALSE;

    get_icon_layout (chooser, alloc_width, alloc_height, &size, &x, &y);
    update_icons (chooser, size);

    for (id = 0; id < chooser->priv->n_monitors; ++id)
    {
        label = g_strdup_printf ("%d", id + 1);
        cairo_save (ctx);
        paint_monitor (widget, ctx, x + id * (size + ICON_SPACING), y, size,
                       label, id == chooser->priv->selected);
        cairo_restore (ctx);
        g_free (label);
    }

    return FALSE;
}

static void
paint_monitor (GtkWidget *widget,
               cairo_t *cr,
               gint x,
               gint y,
               gint size,
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
        cairo_set_source_surface (cr, icon, x, y);
        cairo_paint (cr);
    }

    font_description = pango_font_description_copy (
        pango_context_get_font_description (gtk_widget_get_pango_context (widget)));
    pango_font_description_set_weight (font_description, PANGO_WEIGHT_BOLD);
    pango_font_description_set_absolute_size (font_description, size * 0.2 * PANGO_SCALE);

    layout = pango_cairo_create_layout (cr);
    pango_layout_set_font_description (layout, font_description);
    pango_layout_set_text (layout, label, -1);
    pango_layout_get_pixel_size (layout, &text_width, &text_height);

    cairo_move_to (cr, x + (size - text_width) / 2.0, y + (size - text_height) / 2.0);
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
 * Add a monitor to the monitor-chooser.
 */
gint
rstto_monitor_chooser_add (RsttoMonitorChooser *chooser,
                           gint width,
                           gint height)
{
    Monitor **monitors = g_new0 (Monitor *, chooser->priv->n_monitors + 2);
    gint id = 0;

    Monitor *monitor = g_new0 (Monitor, 1);
    monitor->width = width;
    monitor->height = height;

    if (NULL == chooser->priv->monitors)
    {
        chooser->priv->selected = 0;
    }
    else
    {
        chooser->priv->selected = 0;

        for (id = 0; chooser->priv->monitors[id]; ++id)
        {
            monitors[id] = chooser->priv->monitors[id];
        }
        g_free (chooser->priv->monitors);
    }

    monitors[id] = monitor;

    chooser->priv->monitors = monitors;
    chooser->priv->n_monitors++;

    return id;
}

/**
 * rstto_monitor_chooser_set_image_surface:
 * @chooser:    Monitor chooser
 * @monitor_id: Monitor number
 * @surface:    Surface
 * @error:
 *
 * Set the image-surface for a specific monitor. (the image visible in
 * the monitor)
 */
gint
rstto_monitor_chooser_set_image_surface (RsttoMonitorChooser *chooser,
                                         gint monitor_id,
                                         cairo_surface_t *surface,
                                         GError **error)
{
    Monitor *monitor;
    gint retval = -1;

    g_return_val_if_fail (monitor_id < chooser->priv->n_monitors, retval);

    monitor = chooser->priv->monitors[monitor_id];

    if (monitor)
    {
        if (monitor->image_surface)
        {
            cairo_surface_destroy (monitor->image_surface);
        }

        monitor->image_surface = surface;

        retval = monitor_id;
    }
    if (gtk_widget_get_realized (GTK_WIDGET (chooser)))
    {
        gtk_widget_queue_draw (GTK_WIDGET (chooser));
    }

    return retval;
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
    gdouble icon_x;
    gint id, size, x, y;

    if (chooser->priv->n_monitors < 2)
        return;

    get_icon_layout (chooser,
                     gtk_widget_get_allocated_width (widget),
                     gtk_widget_get_allocated_height (widget),
                     &size, &x, &y);

    if (event->y < y + size * ICON_SCREEN_Y1 || event->y > y + size * ICON_SCREEN_Y2)
        return;

    for (id = 0; id < chooser->priv->n_monitors; ++id)
    {
        icon_x = x + id * (size + ICON_SPACING);
        if (event->x >= icon_x + size * ICON_SCREEN_X1 && event->x <= icon_x + size * ICON_SCREEN_X2)
        {
            if (id != chooser->priv->selected)
            {
                chooser->priv->selected = id;

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
 * Returns the id of the selected monitor.
 */
gint
rstto_monitor_chooser_get_selected (RsttoMonitorChooser *chooser)
{
    return chooser->priv->selected;
}

/**
 * rstto_monitor_chooser_get_dimensions:
 * @chooser: The monitor-chooser widget
 * @nr:      The monitor-number
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
    g_return_if_fail (nr < chooser->priv->n_monitors);

    *width = chooser->priv->monitors[nr]->width;
    *height = chooser->priv->monitors[nr]->height;
}
