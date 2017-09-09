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

struct _GdModelListBoxselfate
{
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  GPtrArray *widgets;
  GPtrArray *pool;
  GdModelListBoxRemoveFunc remove_func;
  GdModelListBoxFillFunc fill_func;
  gpointer fill_func_data;
  gpointer remove_func_data;
  GListModel *model;

  guint model_from;
  guint model_to;
  double bin_y_diff;
};

typedef struct _GdModelListBoxselfate GdModelListBoxselfate;


G_DEFINE_TYPE_WITH_CODE (GdModelListBox, gd_model_list_box, GTK_TYPE_WIDGET,
                         /*G_ADD_selfATE (GdModelListBox)*/
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL));

#define self_DECL(l) GdModelListBoxselfate *self = (gd_model_list_box_get_instance_selfate ((GdModelListBox *)l))
#define self(l) ((GdModelListBoxselfate *)gd_model_list_box_get_instance_selfate ((GdModelListBox *)l))

#define Foreach_Row {guint i; for (i = 0; i < self->widgets->len; i ++){ \
                       GtkWidget *row = g_ptr_array_index (self->widgets, i);


enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY
};

static GtkWidget *
get_widget (GdModelListBox *self, guint index)
{
  gpointer item;
  GtkWidget *old_widget = NULL;
  GtkWidget *new_widget;

  item = g_list_model_get_item (self->model, index);

  if (self->pool->len > 0)
    old_widget = g_ptr_array_remove_index_fast (self->pool, 0);

  new_widget = self->fill_func (item, old_widget, index, self->fill_func_data);

  if (old_widget != NULL)
    g_assert (old_widget == new_widget);

  if (g_object_is_floating (new_widget))
    g_object_ref_sink (new_widget);

  g_assert (GTK_IS_WIDGET (new_widget));

  if (gtk_widget_get_parent (new_widget) == NULL)
    gtk_widget_set_parent (new_widget, GTK_WIDGET (self));
  else
    g_assert (gtk_widget_get_parent (new_widget) == GTK_WIDGET (self));

  return new_widget;
}

static void
insert_child_internal (GdModelListBox *self,
                       GtkWidget      *widget,
                       guint           index)
{
  g_object_ref (widget);

  if (gtk_widget_get_parent (widget) == NULL)
    gtk_widget_set_parent (widget, GTK_WIDGET (self));

  gtk_widget_set_child_visible (widget, TRUE);
  g_ptr_array_insert (self->widgets, index, widget);
}

static void
remove_child_by_index (GdModelListBox *self,
                       guint           index)
{
  GtkWidget *row;

  row = g_ptr_array_index (self->widgets, index);

  /* We ref() each row when we insert it in insert_child_internal, but we don't
   * unref() them here since we don't want them to be finalized when we unparent()
   * which will unref again. */

  if (self->remove_func)
    {
      guint item_index = self->model_from + index;
      self->remove_func (row, g_list_model_get_item (self->model, item_index));
    }

  gtk_widget_unparent (row);
  gtk_widget_set_child_visible (g_ptr_array_index (self->widgets, index), FALSE);
  /* Can't use _fast for self->widgets, we need to keep the order. */
  g_ptr_array_remove_index (self->widgets, index);
  g_ptr_array_add (self->pool, row);
}

static inline int
requested_row_height (GdModelListBox *box,
                      GtkWidget      *w)
{
  int min;
  gtk_widget_measure (w,
                      GTK_ORIENTATION_VERTICAL,
                      gtk_widget_get_allocated_width (GTK_WIDGET (box)),
                      &min, NULL, NULL, NULL);
  return min;
}

static inline int
row_y (GdModelListBox *self,
       guint           index)
{
  int y = 0;
  guint i;

  for (i = 0; i < index; i ++)
    y += requested_row_height (self, g_ptr_array_index (self->widgets, i));

  return y;
}

static int
estimated_widget_height (GdModelListBox *self)
{
  int avg_widget_height = 0;

  Foreach_Row
    avg_widget_height += requested_row_height (self, row);
  }}

  if (self->widgets->len > 0)
    avg_widget_height /= self->widgets->len;
  else
    return 0;

  return avg_widget_height;

}

static inline int
bin_y (GdModelListBox *self)
{
  return - gtk_adjustment_get_value (self->vadjustment) + self->bin_y_diff;
}

static inline int
bin_height (GdModelListBox *self)
{
  int height = 0;
  int min;

  /* XXX This is only true if we actually allocate all rows at minimum height... */
  Foreach_Row
    gtk_widget_measure (row,
                        GTK_ORIENTATION_VERTICAL,
                        gtk_widget_get_allocated_width (GTK_WIDGET (self)),
                        &min, NULL, NULL, NULL);

    height += min;
  }}

  return height;
}

static int
estimated_list_height (GdModelListBox *self)
{
  int avg_height;
  int top_widgets;
  int bottom_widgets;
  int exact_height = 0;

  avg_height = estimated_widget_height (self);
  top_widgets = self->model_from;
  bottom_widgets = g_list_model_get_n_items (self->model) - self->model_to;

  g_assert (top_widgets + bottom_widgets + self->widgets->len == g_list_model_get_n_items (self->model));

  Foreach_Row
    exact_height += requested_row_height (self, row);
  }}

  g_assert (self->widgets->len == (self->model_to - self->model_from));

  return (top_widgets * avg_height) +
         exact_height +
         (bottom_widgets * avg_height);
}

static void
configure_adjustment (GdModelListBox *self)
{
  int widget_height;
  int list_height;
  double cur_upper;
  double cur_value;
  double page_size;

  widget_height = gtk_widget_get_allocated_height (GTK_WIDGET (self));
  list_height   = estimated_list_height (self);
  cur_upper     = gtk_adjustment_get_upper (self->vadjustment);
  cur_value     = gtk_adjustment_get_value (self->vadjustment);
  page_size     = gtk_adjustment_get_page_size (self->vadjustment);

  /*g_message ("%s: Estimated list height: %d", __FUNCTION__, list_height);*/

  if ((int)cur_upper != list_height)
    {
      gtk_adjustment_set_upper (self->vadjustment, list_height);
    }
  else if (list_height == 0)
    {
      gtk_adjustment_set_upper (self->vadjustment, widget_height);
    }

  if ((int)page_size != widget_height)
    gtk_adjustment_set_page_size (self->vadjustment, widget_height);

  /*if (cur_value > cur_upper - widget_height)*/
    /*{*/
      /*gtk_adjustment_set_value (self->vadjustment, cur_upper - widget_height);*/
    /*}*/
}

static void
ensure_visible_widgets (GdModelListBox *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  int widget_height;
  int bottom_removed = 0;
  int bottom_added = 0;
  int top_removed = 0;
  int top_added = 0;

  g_message (__FUNCTION__);

  // TODO: Temporarily disabled since it doesn't work in unit tests
  //       -> just remove it?
  if (!gtk_widget_get_mapped (widget) && FALSE) {
    g_message ("%s: Not mapped!", __FUNCTION__);
    return;
  }

  if (!self->vadjustment)
    return;

  double upper_before = gtk_adjustment_get_upper (self->vadjustment);
  double value_before = gtk_adjustment_get_value (self->vadjustment);
  double page_size_before = gtk_adjustment_get_page_size (self->vadjustment);

  // TODO: This should use the "content height"
  widget_height = gtk_widget_get_allocated_height (widget);

  g_assert_cmpint (self->bin_y_diff, >=, 0);
  g_assert_cmpint (bin_height (self), >=, 0);

  /*g_message ("-------------------------------------");*/

  /* This "out of sight" case happens when the new value is so different from the old one
   * that we rather just remove all widgets and adjust the model_from/model_to values.
   * This happens when scrolling fast, clicking the scrollbar directly or just by programmatically
   * setting the vadjustment value.
   */
  if (bin_y (self) + bin_height (self) < 0 ||
      bin_y (self) >= widget_height)
    {
      int avg_widget_height = estimated_widget_height (self);
      double percentage;
      double value = gtk_adjustment_get_value (self->vadjustment);
      double upper = gtk_adjustment_get_upper (self->vadjustment);
      double page_size = gtk_adjustment_get_page_size (self->vadjustment);
      guint top_widget_index;
      int i;

      /*g_message ("OUT OF SIGHT! bin_y: %d, bin_height; %d, widget_height: %d",*/
                 /*bin_y (self), bin_height (self), widget_height);*/
      /*g_message ("Value: %f, upper: %f", value, upper);*/

      for (i = self->widgets->len - 1; i >= 0; i --)
        remove_child_by_index (self, i);

      g_assert (self->widgets->len == 0);

      /* We do NOT subtract page_size here since we are interested in the row
       * at the top of the widget. */
      percentage = value / upper;

      top_widget_index = (guint) (g_list_model_get_n_items (self->model) * percentage);
      /*g_message ("top_widget_index: %u", top_widget_index);*/

      if (top_widget_index > g_list_model_get_n_items (self->model))
        {
          /* XXX Can this still happen? */
          self->model_from = g_list_model_get_n_items (self->model);
          self->model_to   = g_list_model_get_n_items (self->model);
          self->bin_y_diff = value + page_size;
          g_assert (FALSE);
        }
      else
        {
          self->model_from = top_widget_index;
          self->model_to   = top_widget_index;
          self->bin_y_diff = top_widget_index * avg_widget_height;
        }

        g_assert (self->model_from <= g_list_model_get_n_items (self->model));
        g_assert (self->model_to <= g_list_model_get_n_items (self->model));
        g_assert (bin_y (self) <= widget_height);

        /*g_message ("After OOS: model_from: %d, model_to: %d, bin_y: %d, bin_height; %d",*/
                   /*self->model_from, self->model_to, bin_y (self), bin_height (self));*/
    }

  /* Remove top widgets */
  {
    guint i;

    for (i = 0; i < self->widgets->len; i ++)
      {
        GtkWidget *w = g_ptr_array_index (self->widgets, i);
        int w_height = requested_row_height (self, w);
        if (bin_y (self) + row_y (self, i) + w_height < 0)
          {
            g_assert_cmpint (i, ==, 0);
            self->bin_y_diff += w_height;
            remove_child_by_index (self, i);
            self->model_from ++;
            top_removed ++;
            /*g_message ("Removing from top");*/

            /* Do the first row again */
            i--;
          } else
            break;
      }
  }

  /* Add top widgets */
  {
    for (;;)
      {
        GtkWidget *new_widget;
        int min;

        if (bin_y (self) <= 0)
          {
            break;
          }

        if (self->model_from == 0)
          {
            break;
          }

        self->model_from --;

        /*g_message ("Adding on top");*/
        new_widget = get_widget (self, self->model_from);
        g_assert (new_widget != NULL);
        insert_child_internal (self, new_widget, 0);
        min = requested_row_height (self, new_widget);
        self->bin_y_diff -= min;
        top_added ++;
      }
  }


  /* Remove bottom widgets */
  {
    int i = self->widgets->len - 1;
    for (;;)
      {
        GtkWidget *w;
        int y;

        if (i <= 0)
          {
            break;
          }

        y = bin_y (self) + row_y (self, i);

        if (y < widget_height)
          {
            break;
          }
        /*g_message ("Removing widget at bottom with y %d", y);*/

        w = g_ptr_array_index (self->widgets, i);
        g_assert (w);

        remove_child_by_index (self, i);
        self->model_to --;
        bottom_removed ++;

        i--;
      }
  }

  /* Insert bottom widgets */
  {
    for (;;)
      {
        GtkWidget *new_widget;

        /* If the widget is full anyway */
        if (bin_y (self) + bin_height (self) >= widget_height)
          {
            break;
          }

        /* ... or if we are out of items */
        if (self->model_to >= g_list_model_get_n_items (self->model))
          {
            break;
          }

        /*g_message ("Adding at bottom for model index %u. bin_y: %d, bin_height: %d", self->model_to,*/
                   /*bin_y (self), bin_height (self));*/
        new_widget = get_widget (self, self->model_to);
        insert_child_internal (self, new_widget, self->widgets->len);

        self->model_to ++;
        bottom_added ++;
      }
  }

  if (top_removed    > 0) g_assert_cmpint (top_added,      ==, 0);
  if (top_added      > 0) g_assert_cmpint (top_removed,    ==, 0);
  if (bottom_removed > 0) g_assert_cmpint (bottom_added,   ==, 0);
  if (bottom_added   > 0) g_assert_cmpint (bottom_removed, ==, 0);

  g_assert_cmpint (self->bin_y_diff, >=, 0);
  g_assert_cmpint (bin_y (self), <=, 0);
  g_assert_cmpint (self->widgets->len, ==, self->model_to - self->model_from);

  configure_adjustment (self);
  /*g_message ("AFTER: Value: %f, upper; %f",*/
             /*gtk_adjustment_get_value (self->vadjustment),*/
             /*gtk_adjustment_get_upper (self->vadjustment));*/

  {
    /*double upper_after = gtk_adjustment_get_upper (self->vadjustment);*/
    /*double page_size_after = gtk_adjustment_get_page_size (self->vadjustment);*/

    /*if (upper_before != upper_after)*/
      /*{*/
        /* The value of the vadjustment makes only sense considering its upper value.
         * The (new) value is for the upper_before, not the upper_after, which might be a lot
         * less or a lot more. If the upper changed, we here adapt the value by scaling it
         * to the new upper_after */
        /*g_message ("Upper diff: %f/%f", upper_before, upper_after);*/
        /*g_message ("upper_before: %f", upper_before);*/
        /*g_message ("upper_after : %f", upper_after);*/
        /*g_message ("page_size   : %f", gtk_adjustment_get_page_size (self->vadjustment));*/

        /*double percent_before = value_before / (upper_before - page_size_before);*/

        /*double new_value = percent_before * (upper_after - page_size_after);*/
        /*g_message ("new_value: %f", new_value);*/

        /*self->fuck = TRUE;*/
        /*gtk_adjustment_set_value (self->vadjustment, new_value);*/
        /*self->fuck = FALSE;*/

        /*g_assert (0);*/
      /*}*/
  }
}

static void
value_changed_cb (GtkAdjustment *adjustment, gpointer user_data)
{

  g_message (__FUNCTION__);
  /*if (self->fuck)*/
    /*return;*/

  g_message ("QUEUE ALLOCATE");
  /* ensure_visible_widgets will be called from size_allocate */
  gtk_widget_queue_allocate (user_data);
}

static void
items_changed_cb (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added,
                  gpointer    user_data)
{
  GdModelListBox *self = user_data;
  int i;

  g_message ("%s: position %d, removed: %u, added: %u", __FUNCTION__, position, removed, added);

  /* If the change is out of our visible range anyway,
   * we don't care. */
  if (position > self->model_to)
    {
      configure_adjustment (self);
      return;
    }

  /* Empty the current view */
  for (i = self->widgets->len - 1; i >= 0; i --)
    remove_child_by_index (self, i);

  self->model_to = self->model_from;
  self->bin_y_diff = 0;

  /*g_message ("From %s", __FUNCTION__);*/
  ensure_visible_widgets (self);
}

/* GtkWidget vfuncs {{{ */
static void
__size_allocate (GtkWidget           *widget,
                 const GtkAllocation *allocation,
                 int                  baseline,
                 GtkAllocation       *out_clip)
{
  GdModelListBox *self = GD_MODEL_LIST_BOX (widget);

  g_message (__FUNCTION__);

  if (self->widgets->len > 0)
    {
      GtkAllocation child_alloc;
      int y;

      /*g_message ("From %s", __FUNCTION__);*/
      ensure_visible_widgets ((GdModelListBox *)widget);

      /* Now actually allocate sizes to all the rows */
      y = bin_y ((GdModelListBox *)widget);

      child_alloc.x = 0;
      child_alloc.width = allocation->width;

      Foreach_Row
        int h;

        gtk_widget_measure (row, GTK_ORIENTATION_VERTICAL, allocation->width,
                            &h, NULL, NULL, NULL);
        child_alloc.y = y;
        child_alloc.height = h;
        gtk_widget_size_allocate (row, &child_alloc, -1, out_clip);

        y += h;
      }}
      /* configure_adjustment is being called from ensure_visible_widgets already */
    }
}

static void
__snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  GdModelListBox *self = GD_MODEL_LIST_BOX (widget);
  GtkAllocation alloc;

  /* TODO: This should be the content size */
  gtk_widget_get_allocation (widget, &alloc);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT(
                            0, 0,
                            alloc.width,
                            alloc.height
                          ),
                          "GdModelListBox clip");

  Foreach_Row
    gtk_widget_snapshot_child (widget,
                               row,
                               snapshot);
  }}

  gtk_snapshot_pop (snapshot);
}

static void
__map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gd_model_list_box_parent_class)->map (widget);

  /*g_message ("From %s", __FUNCTION__);*/
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
  GdModelListBox *self = GD_MODEL_LIST_BOX (widget);

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

      /*g_message ("Measured width: %d, %d", min_width, nat_width);*/
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
  GdModelListBox *self = GD_MODEL_LIST_BOX (object);

  switch (prop_id)
    {
      case PROP_HADJUSTMENT:
        g_set_object (&self->hadjustment, g_value_get_object (value));
        break;
      case PROP_VADJUSTMENT:
        g_set_object (&self->vadjustment, g_value_get_object (value));
        if (g_value_get_object (value))
          g_signal_connect (G_OBJECT (self->vadjustment), "value-changed",
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
  GdModelListBox *self = GD_MODEL_LIST_BOX (object);

  switch (prop_id)
    {
      case PROP_HADJUSTMENT:
        g_value_set_object (value, self->hadjustment);
        break;
      case PROP_VADJUSTMENT:
        g_value_set_object (value, self->vadjustment);
        break;
      case PROP_HSCROLL_POLICY:
      case PROP_VSCROLL_POLICY:
        break;
    }
}

static void
__finalize (GObject *obj)
{
  GdModelListBox *self = GD_MODEL_LIST_BOX (obj);
  guint i;

  for (i = 0; i < self->pool->len; i ++)
    gtk_widget_unparent (g_ptr_array_index (self->pool, i));

  for (i = 0; i < self->widgets->len; i ++)
    gtk_widget_unparent (g_ptr_array_index (self->widgets, i));

  g_ptr_array_free (self->pool, TRUE);
  g_ptr_array_free (self->widgets, TRUE);

  G_OBJECT_CLASS (gd_model_list_box_parent_class)->finalize (obj);
}
/* }}} */

GtkWidget *
gd_model_list_box_new (void)
{
  return GTK_WIDGET (g_object_new (GD_TYPE_MODEL_LIST_BOX, NULL));
}

void
gd_model_list_box_set_fill_func (GdModelListBox         *self,
                                 GdModelListBoxFillFunc  func,
                                 gpointer                user_data)
{
  self->fill_func = func;
  self->fill_func_data = user_data;
}

void
gd_model_list_box_set_remove_func (GdModelListBox           *self,
                                   GdModelListBoxRemoveFunc  func,
                                   gpointer                  user_data)

{
  self->remove_func = func;
  self->remove_func_data = user_data;
}

void
gd_model_list_box_set_model (GdModelListBox *self,
                             GListModel     *model)
{
  if (self->model != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->model,
                                            G_CALLBACK (items_changed_cb), self);
      g_object_unref (self->model);
    }

  self->model = model;
  if (model != NULL)
    {
      g_signal_connect (G_OBJECT (model), "items-changed", G_CALLBACK (items_changed_cb), self);
      g_object_ref (model);
    }

  /*g_message ("From %s", __FUNCTION__);*/
  ensure_visible_widgets (self);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

GListModel *
gd_model_list_box_get_model (GdModelListBox *self)
{
  return self->model;
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
gd_model_list_box_init (GdModelListBox *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->widgets    = g_ptr_array_sized_new (20);
  self->pool       = g_ptr_array_sized_new (10);
  self->model_from = 0;
  self->model_to   = 0;
  self->bin_y_diff = 0;
}
