/*
 *  Copyright 2017 Timm Bäder
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
  GtkWidget *placeholder;

  GdModelListBoxRemoveFunc remove_func;
  GdModelListBoxFillFunc fill_func;
  gpointer fill_func_data;
  gpointer remove_func_data;
  GListModel *model;

  guint model_from;
  guint model_to;
  double bin_y_diff;
};

typedef struct _GdModelListBoxPrivate GdModelListBoxPrivate;


G_DEFINE_TYPE_WITH_CODE (GdModelListBox, gd_model_list_box, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GdModelListBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL));

#define PRIV_DECL(l) GdModelListBoxPrivate *priv = ((GdModelListBoxPrivate *)gd_model_list_box_get_instance_private ((GdModelListBox *)l))
#define PRIV(l) ((GdModelListBoxPrivate *)gd_model_list_box_get_instance_private ((GdModelListBox *)l))

#define Foreach_Row {guint i; for (i = 0; i < priv->widgets->len; i ++){ \
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
  PRIV_DECL (box);
  gpointer item;
  GtkWidget *old_widget = NULL;
  GtkWidget *new_widget;

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

  gtk_widget_set_parent (widget, GTK_WIDGET (box));
  gtk_widget_set_child_visible (widget, TRUE);
  g_ptr_array_insert (priv->widgets, index, widget);
}

static void
remove_child_by_index (GdModelListBox *box, guint index)
{
  PRIV_DECL (box);
  GtkWidget *row;

  row = g_ptr_array_index (priv->widgets, index);

  /* We ref() each row when we insert it in insert_child_internal, but we don't
   * unref() them here since we don't want them to be finalized when we unparent()
   * which will unref again. */

  if (priv->remove_func)
    {
      guint item_index = priv->model_from + index;
      priv->remove_func (row, g_list_model_get_item (priv->model, item_index));
    }

  gtk_widget_unparent (row);
  gtk_widget_set_child_visible (g_ptr_array_index (priv->widgets, index), FALSE);
  /* Can't use _fast for priv->widgets, we need to keep the order. */
  g_ptr_array_remove_index (priv->widgets, index);
  g_ptr_array_add (priv->pool, row);
}

static inline int
requested_row_height (GdModelListBox *box, GtkWidget *w)
{
  int nat;
  gtk_widget_measure (w,
                      GTK_ORIENTATION_VERTICAL,
                      gtk_widget_get_allocated_width (GTK_WIDGET (box)),
                      &nat, NULL, NULL, NULL);
  return nat;
}

static inline int
row_y (GdModelListBox *box, guint index)
{
  int y = 0;
  guint i;
  PRIV_DECL (box);

  for (i = 0; i < index; i ++)
    y += requested_row_height (box, g_ptr_array_index (priv->widgets, i));

  return y;
}

static int
estimated_widget_height (GdModelListBox *box)
{
  int avg_widget_height = 0;
  PRIV_DECL (box);

  Foreach_Row
    avg_widget_height += requested_row_height (box, row);
  }}

  if (priv->widgets->len > 0)
    avg_widget_height /= priv->widgets->len;
  else
    return 0;

  return avg_widget_height;

}

static inline int
bin_y (GdModelListBox *self)
{
  return - gtk_adjustment_get_value (PRIV (self)->vadjustment) + PRIV (self)->bin_y_diff;
}

static inline int
bin_height (GdModelListBox *self)
{
  PRIV_DECL (self);
  int height = 0;

  Foreach_Row
    GtkAllocation a;

    gtk_widget_get_allocated_size (row, &a, NULL);

    height += a.height;
  }}

  return height;
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
    exact_height += requested_row_height (box, row);
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
  cur_upper     = gtk_adjustment_get_upper (priv->vadjustment);
  page_size     = gtk_adjustment_get_page_size (priv->vadjustment);
  cur_value     = gtk_adjustment_get_value (priv->vadjustment);


  if ((int)cur_upper != MAX (list_height, widget_height)) {
    /*g_message ("New upper: %d (%d, %d)", MAX (list_height, widget_height), list_height, widget_height);*/
    gtk_adjustment_set_upper (priv->vadjustment, MAX (list_height, widget_height));
  } else if (list_height == 0)
    gtk_adjustment_set_upper (priv->vadjustment, widget_height);


  if ((int)page_size != widget_height)
    gtk_adjustment_set_page_size (priv->vadjustment, widget_height);

  /*if (cur_value > cur_upper - widget_height)*/
    /*{*/
      /*gtk_adjustment_set_value (priv->vadjustment, cur_upper - widget_height);*/
    /*}*/
}


static void
ensure_visible_widgets (GdModelListBox *box)
{
  GtkWidget *widget = GTK_WIDGET (box);
  int rows_height;
  int widget_height;
  gboolean top_removed, top_added, bottom_removed, bottom_added;
  PRIV_DECL (box);

  if (!gtk_widget_get_mapped (widget))
    return;

  g_message (__FUNCTION__);

  widget_height = gtk_widget_get_allocated_height (widget);

  g_assert_cmpint (priv->bin_y_diff, >=, 0);
  g_assert_cmpint (bin_height (box), >=, 0);
  /*g_assert_cmpint (bin_y (box), <=, 0);*/

  rows_height = bin_height (box);

  /* This "out of sight" case happens when the new value is so different from the old one
   * that we rather just remove all widgets and adjust the model_from/model_to values
   */
  if (bin_y (box) + rows_height < 0 ||
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
        remove_child_by_index (box, i);

      rows_height = 0; /* Should be obvious, right? */

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

        g_assert (priv->model_from <= g_list_model_get_n_items (priv->model));
        g_assert (priv->model_to <= g_list_model_get_n_items (priv->model));
        g_assert (bin_y (box) <= widget_height);
    }


  /* Remove top widgets */
  {
    guint i;
    top_removed = FALSE;

    for (i = 0; i < priv->widgets->len; i ++)
      {
        GtkWidget *w = g_ptr_array_index (priv->widgets, i);
        int w_height = requested_row_height (box, w);
        if (bin_y (box) + row_y (box, i) + w_height < 0)
          {
            g_assert_cmpint (i, ==, 0);
            priv->bin_y_diff += w_height;
            rows_height -= w_height;
            remove_child_by_index (box, i);
            priv->model_from ++;
            top_removed = TRUE;

            /* Do the first row again */
            i--;
          } else
            break;
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
        min = requested_row_height (box, new_widget);
        priv->bin_y_diff -= min;
        rows_height += min;
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
        g_assert (w);
        int y = bin_y (box) + row_y (box, i);

        /*g_message ("%d: %d + %d > %d", i, bin_y (box), row_y (box, i), widget_height);*/
        if (y > widget_height)
          {
            /*g_message ("Removing widget %d", i);*/
            int w_height = requested_row_height (box, w);
            remove_child_by_index (box, i);
            rows_height -= w_height;
            priv->model_to --;
            bottom_removed = TRUE;
            i--;
          }
        else
          break;
      }
  }


  /* Insert bottom widgets */
  {
    bottom_added = FALSE;
    /*g_message ("%d (%f) + %d <= %d", bin_y (box), priv->bin_y_diff, bin_height, widget_height);*/
    while (bin_y (box) + rows_height <= widget_height &&
           priv->model_to < g_list_model_get_n_items (priv->model))
      {
        GtkWidget *new_widget;
        int min;

        /*g_message ("Inserting bottom widget for position %u at %u", priv->model_to, priv->widgets->len);*/
        new_widget = get_widget (box, priv->model_to);
        insert_child_internal (box, new_widget, priv->widgets->len);
        min = requested_row_height (box, new_widget);
        rows_height += min;

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

    new_upper = estimated_list_height (box, &top_part, &bottom_part);

    if (new_upper > gtk_adjustment_get_upper (priv->vadjustment))
      priv->bin_y_diff = MAX (top_part, gtk_adjustment_get_value (priv->vadjustment));
    else
      priv->bin_y_diff = MIN (top_part, gtk_adjustment_get_value (priv->vadjustment));

    configure_adjustment (box);

    /*gtk_adjustment_set_value (priv->vadjustment, priv->bin_y_diff);// - bin_window_y);*/
    /*if (gtk_adjustment_get_value (priv->vadjustment) < priv->bin_y_diff)*/
      /*{*/
        /*gtk_adjustment_set_value (priv->vadjustment, priv->bin_y_diff);*/
        /*g_message ("Case 1");*/
      /*}*/

    if (bin_y (box) > 0)
      {
        /*g_message ("CRAP");*/
        priv->bin_y_diff = gtk_adjustment_get_value (priv->vadjustment);
      }
  }



  configure_adjustment (box);

  gtk_widget_queue_allocate (widget); // Reposition children
  gtk_widget_queue_draw (widget);
}

static void
value_changed_cb (GtkAdjustment *adjustment, gpointer user_data)
{
  ensure_visible_widgets (user_data);

  gtk_widget_queue_allocate (user_data);
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
  if (position > priv->model_to)
    {
      configure_adjustment (box);
      return;
    }

  if (priv->placeholder)
    gtk_widget_set_child_visible (priv->placeholder,
                                  g_list_model_get_n_items (model) == 0);

  /* Empty the current view */
  for (i = priv->widgets->len - 1; i >= 0; i --)
    remove_child_by_index (box, i);

  priv->model_to = priv->model_from;
  ensure_visible_widgets (box);
}

/* GtkWidget vfuncs {{{ */
static void
__size_allocate (GtkWidget           *widget,
                 const GtkAllocation *allocation,
                 int                  baseline,
                 GtkAllocation       *out_clip)
{
  PRIV_DECL (widget);
  /* TODO: This check is wrong now. */
  gboolean height_changed = allocation->height != gtk_widget_get_allocated_height (widget);

  if (priv->widgets->len > 0)
    {
      GtkAllocation child_alloc;
      int y;

      if (height_changed)
        ensure_visible_widgets ((GdModelListBox *)widget);

      /* Now actually allocate sizes to all the rows */
      y = bin_y ((GdModelListBox *)widget);

      g_message ("%s: bin_y: %d, value: %f, diff: %f",
                 __FUNCTION__, y, gtk_adjustment_get_value (priv->vadjustment), priv->bin_y_diff);

      child_alloc.x = 0;
      child_alloc.width = allocation->width;

      Foreach_Row
        int h;

        gtk_widget_measure (row, GTK_ORIENTATION_VERTICAL, allocation->width,
                            &h, NULL, NULL, NULL);

        if (i == 1) {
          /*g_message ("%s: y: %d", __FUNCTION__, child_alloc.y);*/
        }
        child_alloc.y = y;
        child_alloc.height = h;
        gtk_widget_size_allocate (row, &child_alloc, -1, out_clip);

        y += h;
      }}

      configure_adjustment ((GdModelListBox *)widget);
    }
  else if (priv->placeholder)
    {
      /* List is empty, position placeholder. */
      int min_width, min_height;
      GtkAllocation placeholder_allocation;

      gtk_widget_measure (priv->placeholder, GTK_ORIENTATION_HORIZONTAL, -1, &min_width, NULL, NULL, NULL);
      gtk_widget_measure (priv->placeholder, GTK_ORIENTATION_VERTICAL, min_width, &min_height, NULL, NULL, NULL);

      placeholder_allocation.x = 0;
      placeholder_allocation.y = 0;
      placeholder_allocation.width = MAX (min_width, allocation->width);
      placeholder_allocation.height = MAX (min_height, allocation->height);

      gtk_widget_size_allocate (priv->placeholder, &placeholder_allocation, -1, out_clip);
    }
}

static void
__snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  PRIV_DECL (widget);
  GtkAllocation alloc;

  gtk_widget_get_allocation (widget, &alloc);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT(
                            0, 0,
                            alloc.width,
                            alloc.height
                          ),
                          "GdModelListBox clip");


  if (g_list_model_get_n_items (priv->model) > 0)
    {
      Foreach_Row
        gtk_widget_snapshot_child (widget,
                                   row,
                                   snapshot);
      }}
    }
  else
    {
      gtk_widget_snapshot_child (widget,
                                 priv->placeholder,
                                 snapshot);
    }

  gtk_snapshot_pop (snapshot);
}

static void
__map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gd_model_list_box_parent_class)->map (widget);

  ensure_visible_widgets ((GdModelListBox *) widget);
}

static void
__measure (GtkWidget      *widget,
           GtkOrientation  orientation,
           int             for_size,
           int            *minimum,
           int            *natural,
           int            *minimum_baseline,
           int            *natural_baseline)
{
  PRIV_DECL (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int min_width = 0;
      int nat_width = 0;

      Foreach_Row
        int m, n;
        gtk_widget_measure (row, GTK_ORIENTATION_HORIZONTAL, -1,
                            &m, &n, NULL, NULL);
        min_width = MAX (min_width, m);
        nat_width = MAX (nat_width, n);
      }}

      *minimum = min_width;
      *natural = nat_width;
    }
  else /* VERTICAL */
    {
      *minimum = 1;
      *natural = 1;
    }
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
  g_clear_object (&priv->placeholder);

  G_OBJECT_CLASS (gd_model_list_box_parent_class)->finalize (obj);
}
/* }}} */

GtkWidget *
gd_model_list_box_new (void)
{
  return GTK_WIDGET (g_object_new (GD_TYPE_MODEL_LIST_BOX, NULL));
}

void
gd_model_list_box_set_fill_func (GdModelListBox         *box,
                                 GdModelListBoxFillFunc  func,
                                 gpointer                user_data)
{
  PRIV (box)->fill_func = func;
  PRIV (box)->fill_func_data = user_data;
}

void
gd_model_list_box_set_remove_func (GdModelListBox           *box,
                                   GdModelListBoxRemoveFunc  func,
                                   gpointer                  user_data)

{
  PRIV (box)->remove_func = func;
  PRIV (box)->remove_func_data = user_data;
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

      if (priv->placeholder)
        gtk_widget_set_visible (priv->placeholder, g_list_model_get_n_items (model) == 0);
    }
  ensure_visible_widgets (box);

  gtk_widget_queue_resize (GTK_WIDGET (box));
}

void
gd_model_list_box_set_placeholder (GdModelListBox *box,
                                   GtkWidget      *placeholder)
{
  PRIV_DECL (box);

  g_return_if_fail (GD_IS_MODEL_LIST_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (placeholder));

  priv->placeholder = placeholder;
  gtk_widget_set_parent (placeholder, GTK_WIDGET (box));
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

  object_class->set_property = __set_property;
  object_class->get_property = __get_property;
  object_class->finalize     = __finalize;

  widget_class->map           = __map;
  widget_class->measure       = __measure;
  widget_class->size_allocate = __size_allocate;
  widget_class->snapshot      = __snapshot;

  g_object_class_override_property (object_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (object_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  gtk_widget_class_set_css_name (widget_class, "list");
}

static void
gd_model_list_box_init (GdModelListBox *box)
{
  PRIV_DECL (box);

  gtk_widget_set_has_window (GTK_WIDGET (box), FALSE);

  priv->widgets    = g_ptr_array_sized_new (20);
  priv->pool       = g_ptr_array_sized_new (10);
  priv->model_from = 0;
  priv->model_to   = 0;
  priv->bin_y_diff = 0;
}
