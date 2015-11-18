#include <gtk/gtk.h>
#include "gd-model-list-box.h"

/* Test class {{{ */
struct _GdData
{
  GObject parent_instance;

  guint index;
  guint model_size;
  const gchar *text;
};

typedef struct _GdData GdData;

G_DECLARE_FINAL_TYPE (GdData, gd_data, GD, DATA, GObject);
G_DEFINE_TYPE (GdData, gd_data, G_TYPE_OBJECT);
#define GD_TYPE_DATA gd_data_get_type ()

static void gd_data_init (GdData *d) {}
static void gd_data_class_init (GdDataClass *dc) {}
/* }}} */


/* Test Row widget {{{ */
struct _GdRowWidget
{
  GtkBox parent_instance;

  GtkWidget *label1;
  GtkWidget *label2;
};

typedef struct _GdRowWidget GdRowWidget;

G_DECLARE_FINAL_TYPE (GdRowWidget, gd_row_widget, GD, ROW_WIDGET, GtkBox);
G_DEFINE_TYPE (GdRowWidget, gd_row_widget, GTK_TYPE_BOX);
#define GD_TYPE_ROW_WIDGET gd_row_widget_get_type ()
#define GD_ROW_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GD_TYPE_ROW_WIDGET, GdRowWidget))

static GtkWidget * gd_row_widget_new (void) { return GTK_WIDGET (g_object_new (GD_TYPE_ROW_WIDGET, NULL)); }

static void gd_row_widget_init (GdRowWidget *d)
{
  gtk_box_set_spacing (GTK_BOX (d), 6);

  d->label1 = gtk_label_new ("");
  d->label2 = gtk_label_new ("");

  gtk_label_set_ellipsize (GTK_LABEL (d->label2), TRUE);

  gtk_container_add (GTK_CONTAINER (d), d->label1);
  gtk_container_add (GTK_CONTAINER (d), d->label2);

  gtk_widget_show (d->label1);
  gtk_widget_show (d->label2);
}
static void gd_row_widget_class_init (GdRowWidgetClass *dc) {}
/* }}} */


GtkSizeGroup *size_group1;
GtkSizeGroup *size_group2;


GtkWidget *
fill_func (gpointer item, GtkWidget *old_widget, gpointer user_data)
{
  GdRowWidget *row;
  GtkWidget *label1, *label2;
  GdData *data = GD_DATA (item);
  gchar *label;

  if (!old_widget)
    {
      row = GD_ROW_WIDGET (gd_row_widget_new ());
      gtk_widget_show (GTK_WIDGET (row));

      gtk_size_group_add_widget (size_group1, row->label1);
      gtk_size_group_add_widget (size_group2, row->label2);
    }
  else
    {
      row = GD_ROW_WIDGET (old_widget);
    }


  label = g_strdup_printf ("Row %u of %u", data->index, data->model_size);
  gtk_label_set_label (GTK_LABEL (row->label1), label);
  gtk_label_set_label (GTK_LABEL (row->label2), data->text);

  g_free (label);

  return GTK_WIDGET (row);
}


int
main (int argc, char **argv)
{
  int i;
  guint model_size;
  gtk_init (&argc, &argv);

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GtkWidget *list = gd_model_list_box_new ();

  size_group1 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  size_group2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  GListStore *store = g_list_store_new (GD_TYPE_DATA);
  model_size = 20000;
  for (i = 0; i < model_size; i ++)
    {
      GdData *d = g_object_new (GD_TYPE_DATA, NULL);
      d->model_size = model_size;
      d->index = (guint)i;
      d->text = "fpoobar'lsfasdf asdf asdfas df asd fasd f asdf as dfewrthuier htuiheasruig hdrhfughseduig hisdfiugsdhiugisdf";
      g_list_store_append (store, d);
    }


  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (list), G_LIST_MODEL (store));
  gd_model_list_box_set_fill_func (GD_MODEL_LIST_BOX (list), fill_func, NULL);


  gtk_container_add (GTK_CONTAINER (scroller), list);
  gtk_container_add (GTK_CONTAINER (window), scroller);

  g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_main_quit), NULL);
  gtk_window_resize (GTK_WINDOW (window), 200, 200);
  gtk_widget_show_all (window);
  gtk_main ();
  return 0;
}
