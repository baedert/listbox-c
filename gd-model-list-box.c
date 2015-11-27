/*
 *  Copyright 2015 Timm Bäder
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "gd-model-list-box.h"

struct _GdModelListBoxPrivate
{
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  GPtrArray *widgets;
  GPtrArray *pool;
  GdkWindow *bin_window;

  GdModelListBoxFillFunc fill_func;
  gpointer fill_func_data;
  GListModel *model;

  guint model_from;
  guint model_to;
  double bin_y_diff;
};

typedef struct _GdModelListBoxPrivate GdModelListBoxPrivate;


G_DEFINE_TYPE_WITH_CODE (GdModelListBox, gd_model_list_box, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GdModelListBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL));

#define LIST(l) GD_MODEL_LIST_BOX(l)
#define PRIV_DECL(l) GdModelListBoxPrivate *priv = ((GdModelListBoxPrivate *)gd_model_list_box_get_instance_private ((GdModelListBox *)l))
#define PRIV(l) ((GdModelListBoxPrivate *)gd_model_list_box_get_instance_private ((GdModelListBox *)l))

#define Foreach_Row {int i; for (i = 0; i < priv->widgets->len; i ++){ \
                       GtkWidget *row = g_ptr_array_index (priv->widgets, i);


enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY
};

static GtkWidget *
get_widget (GdModelListBox *box, guint index)
{
  gpointer item;
  GtkWidget *old_widget = NULL;
  GtkWidget *new_widget;
  PRIV_DECL (box);

  item = g_list_model_get_item (priv->model, index);

  if (priv->pool->len > 0)
    old_widget = g_ptr_array_remove_index_fast (priv->pool, 0);

  new_widget = priv->fill_func (item, old_widget, index, priv->fill_func_data);

  if (g_object_is_floating (new_widget))
    g_object_ref_sink (new_widget);

  g_assert (GTK_IS_WIDGET (new_widget));

  /* We just enforce this here. */
  gtk_widget_show (new_widget);

  return new_widget;
}

static void
insert_child_internal (GdModelListBox *box, GtkWidget *widget, guint index)
{
  PRIV_DECL (box);

  g_object_ref (widget);

  gtk_widget_set_parent_window (widget, priv->bin_window);
  gtk_widget_set_parent (widget, GTK_WIDGET (box));

  g_ptr_array_insert (priv->widgets, index, widget);
}

static void
remove_child_internal (GdModelListBox *box, GtkWidget *widget)
{
  PRIV_DECL (box);

  g_object_unref (widget);

  gtk_widget_unparent (widget);
  g_ptr_array_remove (priv->widgets, widget);
  g_ptr_array_add (priv->pool, widget);
}

static void
position_children (GdModelListBox *box)
{
  GtkAllocation alloc;
  GtkAllocation child_alloc;
  PRIV_DECL (box);
  int i, y, foo;

  gtk_widget_get_allocation (GTK_WIDGET (box), &alloc);

  y = alloc.y;

  child_alloc.x = 0;
  child_alloc.width = alloc.width;


  Foreach_Row
    int h;

    gtk_widget_get_preferred_height_for_width (row, alloc.width, &h, &foo);
    child_alloc.y = y;
    child_alloc.height = h;
    gtk_widget_size_allocate (row, &child_alloc);

    y += h;
  }}
}

static inline int
row_height (GdModelListBox *box, GtkWidget *w)
{
  PRIV_DECL (box);
  int min, nat;
  gtk_widget_get_preferred_height_for_width (w,
                                             gtk_widget_get_allocated_width (GTK_WIDGET (box)),
                                             &min, &nat);

  return min;
}

static inline int
row_y (GdModelListBox *box, guint index)
{
  int y = 0;
  int i;
  PRIV_DECL (box);

  for (i = 0; i < index; i ++)
    y += row_height (box, g_ptr_array_index (priv->widgets, i));

  return y;
}

static int
estimated_widget_height (GdModelListBox *box)
{
  int avg_widget_height = 0;
  int i;
  PRIV_DECL (box);

  Foreach_Row
    avg_widget_height += row_height (box, row);
  }}

  if (priv->widgets->len > 0)
    avg_widget_height /= priv->widgets->len;
  else
    return 0;

  return avg_widget_height;

}

static inline int
bin_y (GdModelListBox *box)
{
  return - gtk_adjustment_get_value (PRIV (box)->vadjustment) + PRIV (box)->bin_y_diff;
}

static gboolean
bin_window_full (GdModelListBox *box)
{
  int bin_height;
  int widget_height;
  PRIV_DECL (box);

  if (gtk_widget_get_realized (GTK_WIDGET (box)))
    bin_height = gdk_window_get_height (priv->bin_window);
  else
    bin_height = 0;

  widget_height = gtk_widget_get_allocated_height (GTK_WIDGET (box));

  /*
   * We consider the bin_window "full" if either the bottom of it is out of view,
   * OR it already contains all the items.
   */
  return (bin_y (box) + bin_height > widget_height) ||
         (priv->model_to - priv->model_from == g_list_model_get_n_items (priv->model));
}

static int
estimated_list_height (GdModelListBox *box,
                       guint          *top_part,
                       guint          *bottom_part)
{
  int avg_height;
  int top_widgets;
  int bottom_widgets;
  int exact_height = 0;
  PRIV_DECL (box);

  avg_height = estimated_widget_height (box);
  top_widgets = priv->model_from;
  bottom_widgets = g_list_model_get_n_items (priv->model) - priv->model_to;

  g_assert (top_widgets + bottom_widgets + priv->widgets->len == g_list_model_get_n_items (priv->model));

  Foreach_Row
    exact_height += row_height (box, row);
  }}

  if (top_part)
    *top_part = top_widgets * avg_height;

  if (bottom_part)
    *bottom_part = bottom_widgets * avg_height;

  return (top_widgets * avg_height) +
         exact_height +
         (bottom_widgets * avg_height);
}


static void
update_bin_window (GdModelListBox *box)
{
  GtkAllocation alloc;
  int h = 0;
  PRIV_DECL (box);

  gtk_widget_get_allocation (GTK_WIDGET (box), &alloc);

  Foreach_Row
    int min = row_height (box, row);
    g_assert (min >= 0);
    h += min;
  }}

  if (h == 0) h = 1;

  if (h != gdk_window_get_height (priv->bin_window) ||
      alloc.width != gdk_window_get_width (priv->bin_window))
    {
        gdk_window_move_resize (priv->bin_window,
                                0, bin_y (box),
                                alloc.width, h);
    }
  else
    {
      gdk_window_move (priv->bin_window, 0, bin_y (box));
    }
}

static void
configure_adjustment (GdModelListBox *box)
{
  int widget_height;
  int list_height;
  double cur_upper;
  double cur_value;
  double page_size;
  PRIV_DECL (box);

  widget_height = gtk_widget_get_allocated_height (GTK_WIDGET (box));
  list_height   = estimated_list_height (box, NULL, NULL);
  cur_upper = gtk_adjustment_get_upper (priv->vadjustment);
  page_size = gtk_adjustment_get_page_size (priv->vadjustment);
  cur_value = gtk_adjustment_get_value (priv->vadjustment);


  if ((int)cur_upper != MAX (list_height, widget_height)) {
    /*g_message ("New upper: %d (%d, %d)", MAX (list_height, widget_height), list_height, widget_height);*/
    gtk_adjustment_set_upper (priv->vadjustment, MAX (list_height, widget_height));
  } else if (list_height == 0)
    gtk_adjustment_set_upper (priv->vadjustment, widget_height);


  if ((int)page_size != widget_height)
    gtk_adjustment_set_page_size (priv->vadjustment, widget_height);

  if (cur_value > cur_upper - widget_height)
    {
      gtk_adjustment_set_value (priv->vadjustment, cur_upper - widget_height);
    }

}


static void
ensure_visible_widgets (GdModelListBox *box)
{
  GtkWidget *widget = GTK_WIDGET (box);
  int bin_height;
  int widget_height;
  gboolean top_removed, top_added, bottom_removed, bottom_added;
  PRIV_DECL (box);

  if (!gtk_widget_get_mapped (widget))
    return;

  /*g_message (__FUNCTION__);*/

  widget_height = gtk_widget_get_allocated_height (widget);
  bin_height = gdk_window_get_height (priv->bin_window);

  if (bin_height == 1) bin_height = 0;

  /* This "out of sight" case happens when the new value is so different from the old one
   * that we rather just remove all widgets and adjust the model_from/model_to values
   */
  if (bin_y (box) + bin_height < 0 ||
      bin_y (box) >= widget_height)
    {
      int avg_widget_height = estimated_widget_height (box);
      double percentage;
      double value = gtk_adjustment_get_value (priv->vadjustment);
      double upper = gtk_adjustment_get_upper (priv->vadjustment);
      double page_size = gtk_adjustment_get_page_size (priv->vadjustment);
      guint top_widget_index;
      int i;

      /*g_message ("OUT OF SIGHT");*/

      for (i = priv->widgets->len - 1; i >= 0; i --)
        remove_child_internal (box, g_ptr_array_index (priv->widgets, i));
      bin_height = 0; /* The window is empty now, obviously */

      g_assert (priv->widgets->len == 0);

      /* Percentage of the current vadjustment value */
      percentage = value / (upper - page_size);

      top_widget_index = (guint) (g_list_model_get_n_items (priv->model) * percentage);

      if (top_widget_index > g_list_model_get_n_items (priv->model))
        {
          /* XXX Can this still happen? */
          priv->model_from = g_list_model_get_n_items (priv->model);
          priv->model_to   = g_list_model_get_n_items (priv->model);
          priv->bin_y_diff = value + page_size;
          g_assert (FALSE);
        }
      else
        {
          priv->model_from = top_widget_index;
          priv->model_to   = top_widget_index;
          priv->bin_y_diff = top_widget_index * avg_widget_height;
        }

        g_assert (priv->model_from >= 0);
        g_assert (priv->model_to >= 0);
        g_assert (priv->model_from <= g_list_model_get_n_items (priv->model));
        g_assert (priv->model_to <= g_list_model_get_n_items (priv->model));
        g_assert (bin_y (box) <= widget_height);
    }


  /* Remove top widgets */
  {
    int i;
    top_removed = FALSE;

    for (i = 0; i < priv->widgets->len; i ++)
      {
        GtkWidget *w = g_ptr_array_index (priv->widgets, i);
        int w_height = row_height (box, w);
        if (bin_y (box) + row_y (box, i) + w_height < 0)
          {
            /*g_message ("Removing top widget %d", i);*/
            priv->bin_y_diff += w_height;
            bin_height -= w_height;
            remove_child_internal (box, w);
            priv->model_from ++;
            top_removed = TRUE;
          }
      }
  }

  /* Add top widgets */
  {
    top_added = FALSE;
    while (priv->model_from > 0 && bin_y (box) >= 0)
      {
        GtkWidget *new_widget;
        int min;
        priv->model_from --;

        new_widget = get_widget (box, priv->model_from);
        g_assert (new_widget != NULL);
        insert_child_internal (box, new_widget, 0);
        min = row_height (box, new_widget);
        priv->bin_y_diff -= min;
        bin_height += min;
        top_added = TRUE;
        /*g_message ("Adding top widget for index %d", priv->model_from);*/
      }
  }


  /* Remove bottom widgets */
  {
    bottom_removed = FALSE;
    int i = priv->widgets->len - 1;
    for (; i >= 0; i --)
      {
        GtkWidget *w = g_ptr_array_index (priv->widgets, i);
        int y = bin_y (box) + row_y (box, i);

        /*g_message ("%d: %d + %d > %d", i, bin_y (box), row_y (box, i), widget_height);*/
        if (y > widget_height)
          {
            /*g_message ("Removing widget %d", i);*/
            int w_height = row_height (box, w);
            remove_child_internal (box, w);
            bin_height -= w_height;
            priv->model_to --;
            bottom_removed = TRUE;
          }
        else
          break;
      }
  }


  /* Insert bottom widgets */
  {
    bottom_added = FALSE;
    /*g_message ("%d + %d <= %d", bin_y (box), bin_height, widget_height);*/
    while (bin_y (box) + bin_height <= widget_height &&
           priv->model_to < g_list_model_get_n_items (priv->model))
      {
        GtkWidget *new_widget;
        int min;

        /*g_message ("Inserting bottom widget for position %u at %u", priv->model_to, priv->widgets->len);*/
        new_widget = get_widget (box, priv->model_to);
        insert_child_internal (box, new_widget, priv->widgets->len);
        min = row_height (box, new_widget);
        bin_height += min;

        priv->model_to ++;
        bottom_added = TRUE;
      }
  }

  if (top_removed) g_assert (!top_added);
  if (top_added)   g_assert (!top_removed);

  if (bottom_removed) g_assert (!bottom_added);
  if (bottom_added)   g_assert (!bottom_removed);



  {
    double new_upper;
    guint top_part, bottom_part;
    gboolean widgets_changed = top_removed    || top_added ||
                               bottom_removed || bottom_added;
    int bin_window_y = bin_y (box);


    new_upper = estimated_list_height (box, &top_part, &bottom_part);

    if (new_upper > gtk_adjustment_get_upper (priv->vadjustment))
      priv->bin_y_diff = MAX (top_part, gtk_adjustment_get_value (priv->vadjustment));
    else
      priv->bin_y_diff = MIN (top_part, gtk_adjustment_get_value (priv->vadjustment));

    configure_adjustment (box);

    /*g_message ("Setting value to %f - %d", priv->bin_y_diff, bin_window_y);*/
    gtk_adjustment_set_value (priv->vadjustment, priv->bin_y_diff - bin_window_y);
    if (gtk_adjustment_get_value (priv->vadjustment) < priv->bin_y_diff)
      {
        gtk_adjustment_set_value (priv->vadjustment, priv->bin_y_diff);
        /*g_message ("Case 1");*/
      }

    if (bin_y (box) > 0)
      {
        /*g_message ("CRAP");*/
        priv->bin_y_diff = gtk_adjustment_get_value (priv->vadjustment);
      }
  }



  update_bin_window (box);
  position_children (box);
  configure_adjustment (box);

  gtk_widget_queue_draw (widget);
}

static void
value_changed_cb (GtkAdjustment *adjustment, gpointer user_data)
{
  ensure_visible_widgets (user_data);
}

static void
items_changed_cb (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added,
                  gpointer    user_data)
{
  GdModelListBox *box = user_data;
  int i;
  PRIV_DECL (user_data);

  /* If the change is out of our visible range anyway,
   * we don't care. */
  if (position > priv->model_to &&
      bin_window_full (box))
    {
      configure_adjustment (box);
      return;
    }

  /* Empty the current view */
  for (i = priv->widgets->len - 1; i >= 0; i --)
    remove_child_internal (box, g_ptr_array_index (priv->widgets, i));

  priv->model_to = priv->model_from;
  update_bin_window (box);
  ensure_visible_widgets (box);
}

/* GtkContainer vfuncs {{{ */
static void
__add (GtkContainer *container, GtkWidget *child)
{
  g_error ("Don't use that.");
}

static void
__remove (GtkContainer *container, GtkWidget *child)
{
  PRIV_DECL (container);
  g_assert (gtk_widget_get_parent (child) == GTK_WIDGET (container));
  /*g_assert (gtk_widget_get_parent_window (child) == priv->bin_window); XXX ??? */
}

static void
__forall (GtkContainer *container,
          gboolean      include_internals,
          GtkCallback   callback,
          gpointer      callback_data)
{
  PRIV_DECL (container);

  Foreach_Row
    (*callback)(row, callback_data);
  }}
}
/* }}} */

/* GtkWidget vfuncs {{{ */
static void
__size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  PRIV_DECL(widget);
  gboolean height_changed = allocation->height != gtk_widget_get_allocated_height (widget);

  gtk_widget_set_allocation (widget, allocation);

  position_children ((GdModelListBox *)widget);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);

      update_bin_window ((GdModelListBox *)widget);
    }


  /*if (!bin_window_full ((GdModelListBox *)widget) && height_changed)*/
  if (height_changed)
    ensure_visible_widgets ((GdModelListBox *)widget);

  configure_adjustment ((GdModelListBox *)widget);
}

static gboolean
__draw (GtkWidget *widget, cairo_t *ct)
{
  GtkAllocation alloc;
  GtkStyleContext *context;
  int i;
  PRIV_DECL (widget);

  context = gtk_widget_get_style_context (widget);
  gtk_widget_get_allocation (widget, &alloc);

  gtk_render_background (context, ct, 0, 0, alloc.width, alloc.height);

  if (gtk_cairo_should_draw_window (ct, priv->bin_window))
    Foreach_Row
      gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                    row,
                                    ct);
    }}

  return GDK_EVENT_PROPAGATE;
}

static void
__realize (GtkWidget *widget)
{
  PRIV_DECL (widget);
  GtkAllocation  allocation;
  GdkWindowAttr  attributes = { 0, };
  GdkWindow     *window;
  int            i;

  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_set_realized (widget, TRUE);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget) |
                          GDK_ALL_EVENTS_MASK;
  attributes.wclass = GDK_INPUT_OUTPUT;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, GDK_WA_X | GDK_WA_Y);
  gdk_window_set_user_data (window, widget);
  gtk_widget_set_window (widget, window);


  attributes.height = 1;
  priv->bin_window = gdk_window_new (window, &attributes, GDK_WA_X | GDK_WA_Y);
  gtk_widget_register_window (widget, priv->bin_window);
  gdk_window_show (priv->bin_window);

  Foreach_Row
    gtk_widget_set_parent_window (row, priv->bin_window);
  }}
}

static void
__unrealize (GtkWidget *widget)
{
  PRIV_DECL (widget);

  if (priv->bin_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->bin_window);
      gdk_window_destroy (priv->bin_window);
      priv->bin_window = NULL;
    }

  GTK_WIDGET_CLASS (gd_model_list_box_parent_class)->unrealize (widget);
}

static void
__map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gd_model_list_box_parent_class)->map (widget);

  ensure_visible_widgets ((GdModelListBox *) widget);
}

static void
__get_preferred_width (GtkWidget *widget, int *min, int *nat)
{
  *min = 0;
  *nat = 0;
}

static void
__get_preferred_height (GtkWidget *widget, int *min, int *nat)
{
  *min = 0;
  *nat = 0;
}
/* }}} */

/* GObject vfuncs {{{ */
static void
__set_property (GObject      *object,
                guint         prop_id,
                const GValue *value,
                GParamSpec   *pspec)
{
  switch (prop_id)
    {
      case PROP_HADJUSTMENT:
        PRIV (object)->hadjustment = g_value_get_object (value);
        break;
      case PROP_VADJUSTMENT:
        PRIV (object)->vadjustment = g_value_get_object (value);
        if (g_value_get_object (value))
          g_signal_connect (G_OBJECT (PRIV (object)->vadjustment), "value-changed",
                            G_CALLBACK (value_changed_cb), object);
        break;
      case PROP_HSCROLL_POLICY:
      case PROP_VSCROLL_POLICY:
        break;
    }
}

static void
__get_property (GObject    *object,
                guint       prop_id,
                GValue     *value,
                GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_HADJUSTMENT:
        g_value_set_object (value, PRIV (object)->hadjustment);
        break;
      case PROP_VADJUSTMENT:
        g_value_set_object (value, PRIV (object)->vadjustment);
        break;
      case PROP_HSCROLL_POLICY:
      case PROP_VSCROLL_POLICY:
        break;
    }
}

static void
__finalize (GObject *obj)
{
  PRIV_DECL (obj);

  g_ptr_array_free (priv->pool, TRUE);
  g_ptr_array_free (priv->widgets, TRUE);

  G_OBJECT_CLASS (gd_model_list_box_parent_class)->finalize (obj);
}
/* }}} */

GtkWidget *
gd_model_list_box_new (void)
{
  return GTK_WIDGET (g_object_new (GD_TYPE_MODEL_LIST_BOX, NULL));
}

void
gd_model_list_box_set_fill_func (GdModelListBox *box,
                                 GdModelListBoxFillFunc func,
                                 gpointer               user_data)
{
  PRIV (box)->fill_func = func;
  PRIV (box)->fill_func_data = user_data;
}

void
gd_model_list_box_set_model (GdModelListBox *box,
                             GListModel     *model)
{
  PRIV_DECL(box);

  if (priv->model != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->model,
                                            G_CALLBACK (items_changed_cb), box);
      g_object_unref (priv->model);
    }

  PRIV (box)->model = model;
  if (model != NULL)
    {
      g_signal_connect (G_OBJECT (model), "items-changed", G_CALLBACK (items_changed_cb), box);
      g_object_ref (model);
    }
  ensure_visible_widgets (box);

  gtk_widget_queue_resize (GTK_WIDGET (box));
}

GListModel *
gd_model_list_box_get_model (GdModelListBox *box)
{
  return PRIV (box)->model;
}

static void
gd_model_list_box_class_init (GdModelListBoxClass *class)
{
  GObjectClass      *object_class    = G_OBJECT_CLASS (class);
  GtkWidgetClass    *widget_class    = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->set_property = __set_property;
  object_class->get_property = __get_property;
  object_class->finalize     = __finalize;

  widget_class->draw                 = __draw;
  widget_class->size_allocate        = __size_allocate;
  widget_class->map                  = __map;
  widget_class->get_preferred_width  = __get_preferred_width;
  widget_class->get_preferred_height = __get_preferred_height;
  widget_class->realize              = __realize;
  widget_class->unrealize            = __unrealize;

  container_class->add    = __add;
  container_class->remove = __remove;
  container_class->forall = __forall;

  g_object_class_override_property (object_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (object_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");
}

static void
gd_model_list_box_init (GdModelListBox *box)
{
  PRIV_DECL (box);
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (box));

  priv->widgets = g_ptr_array_new ();
  priv->pool = g_ptr_array_new ();
  priv->model_from = 0;
  priv->model_to   = 0;
  priv->bin_y_diff = 0;

  gtk_style_context_add_class (context, "list");
}
