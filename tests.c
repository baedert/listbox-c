#include <glib.h>
#include "gd-model-list-box.h"

#define ROW_WIDTH  300
#define ROW_HEIGHT 100

static GtkWidget *
label_from_label (gpointer  item,
                  GtkWidget *widget,
                  guint      item_index,
                  gpointer   user_data)
{
  GtkWidget *widget_item = item;
  GtkWidget *new_widget;

  g_assert (GTK_IS_LABEL (widget_item));

  if (widget)
    new_widget = widget;
  else
    new_widget = gtk_label_new ("");

  g_assert (GTK_IS_LABEL (new_widget));

  gtk_widget_set_size_request (new_widget, ROW_WIDTH, ROW_HEIGHT);

  gtk_label_set_label (GTK_LABEL (new_widget), gtk_label_get_label (GTK_LABEL (widget_item)));

  return new_widget;
}


static void
simple ()
{
  GtkWidget *listbox = gd_model_list_box_new ();
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GListStore *store = g_list_store_new (GTK_TYPE_LABEL); // Shrug
  GtkWidget *w;
  int min, nat;
  GtkAllocation fake_alloc;
  GtkAllocation fake_clip;
  GtkAdjustment *vadjustment;
  int i;

  gtk_container_add (GTK_CONTAINER (scroller), listbox);
  g_object_ref_sink (G_OBJECT (scroller));

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));

  g_assert (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (listbox)) == vadjustment);

  // No viewport in between
  g_assert (gtk_widget_get_parent (listbox) == scroller);
  g_assert (GTK_IS_SCROLLABLE (listbox));

  // Empty model
  gtk_widget_measure (listbox, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, 1); // XXX Widgets still have a min size of 1
  g_assert_cmpint (nat, ==, 1);

  gd_model_list_box_set_fill_func (GD_MODEL_LIST_BOX (listbox), label_from_label, NULL);
  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (listbox), G_LIST_MODEL (store));

  w = gtk_label_new ("Some Text");
  g_list_store_append (store, w);
  g_message ("---");

  /* We always request the minimum height */
  gtk_widget_measure (listbox, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, 1);
  g_assert_cmpint (nat, ==, 1);

  /* Width should be one row now though */
  gtk_widget_measure (listbox, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, ROW_WIDTH);
  g_assert_cmpint (nat, ==, ROW_WIDTH);


  gtk_widget_measure (scroller, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  fake_alloc.x = 0;
  fake_alloc.y = 0;
  fake_alloc.width = MAX (min, 300);
  fake_alloc.height = 500;
  gtk_widget_size_allocate (scroller, &fake_alloc, -1, &fake_clip);


  /* 20 all in all */
  for (i = 0; i < 19; i++)
    {
      w = gtk_label_new ("Some Text");
      g_list_store_append (store, w);
    }
  gtk_widget_measure (listbox, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, NULL, NULL);
  g_assert_cmpint (min, ==, ROW_WIDTH);
  g_assert_cmpint (nat, ==, ROW_WIDTH);

  g_assert_cmpint ((int)gtk_adjustment_get_upper (vadjustment), ==, 20 * ROW_HEIGHT);

  // Didn't scroll yet...
  g_assert_cmpint ((int)gtk_adjustment_get_value (vadjustment), ==, 0);

  double new_value = gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment);
  gtk_adjustment_set_value (vadjustment, new_value);
  g_assert_cmpint ((int)gtk_adjustment_get_value (vadjustment), ==, (int)new_value);

  // The allocated height of the list should be the exact same as the
  // scrolledwindow (provided css doesn't fuck it up), which is 500px.
  // Every row is 100px so there should now be 5 of those.
  g_assert_cmpint (GD_MODEL_LIST_BOX (listbox)->widgets->len, ==, 5);

  // Force size-allocating all rows after the scrolling above.
  // This works out because changing the adjustment's value will cause a queue_allocate.
  gtk_widget_size_allocate (listbox, &fake_alloc, -1, &fake_clip);

  // So, the last one should be allocated at the very bottom of the listbox, nowhere else.
  GtkWidget *last_row = g_ptr_array_index (GD_MODEL_LIST_BOX (listbox)->widgets,
                                           GD_MODEL_LIST_BOX (listbox)->widgets->len - 1);
  g_assert_nonnull (last_row);
  g_assert (gtk_widget_get_parent (last_row) == listbox);
  GtkAllocation row_alloc;
  gtk_widget_get_allocation (last_row, &row_alloc);

  // Widget allocations in gtk4 are parent relative to this works out.
  g_assert_cmpint (row_alloc.y, ==, gtk_widget_get_allocated_height (listbox) - ROW_HEIGHT);


  g_object_unref (scroller);
}

int
main (int argc, char **argv)
{
  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/listbox/simple", simple);

  return g_test_run ();
}
