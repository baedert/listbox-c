#include <gtk/gtk.h>
#include "gd-model-list-box.h"
#include "pnl-multi-paned.h"

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

  GtkWidget *paned;

  GtkWidget *label1;
  GtkWidget *label2;
  GtkWidget *button;
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
  gtk_widget_set_margin_start (GTK_WIDGET (d), 12);
  gtk_widget_set_margin_end (GTK_WIDGET (d), 12);

  d->paned = pnl_multi_paned_new (GTK_ORIENTATION_HORIZONTAL);


  d->label1 = gtk_label_new ("");
  d->label2 = gtk_label_new ("");
  d->button = gtk_button_new_with_label ("Click Me");

  gtk_label_set_ellipsize (GTK_LABEL (d->label2), TRUE);
  gtk_widget_set_hexpand (d->label2, TRUE);


  gtk_container_add (GTK_CONTAINER (d->paned), d->label1);
  gtk_container_add (GTK_CONTAINER (d->paned), d->label2);
  gtk_container_add (GTK_CONTAINER (d->paned), d->button);
  gtk_container_add (GTK_CONTAINER (d), d->paned);

  gtk_widget_show_all (GTK_WIDGET (d));
}
static void gd_row_widget_class_init (GdRowWidgetClass *dc) {}
/* }}} */


GtkSizeGroup *size_group1;
GtkSizeGroup *size_group2;


GtkWidget *
fill_func (gpointer   item,
           GtkWidget *old_widget,
           guint      item_index,
           gpointer   user_data)
{
  GdRowWidget *row;
  GtkWidget *header = user_data;
  GdData *data = GD_DATA (item);
  gchar *label;

  g_assert (header);

  if (G_UNLIKELY (!old_widget))
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
  gtk_widget_set_margin_top (GTK_WIDGET (row), 12);

  g_free (label);

  return GTK_WIDGET (row);
}

int
main (int argc, char **argv)
{
  guint i;
  guint model_size;
  gtk_init (&argc, &argv);

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *list = gd_model_list_box_new ();
  GtkWidget *header = pnl_multi_paned_new (GTK_ORIENTATION_HORIZONTAL);
  GtkWidget *column_header1 = gtk_label_new ("Column1");
  GtkWidget *column_header2 = gtk_label_new ("Column2");
  GtkWidget *column_header3 = gtk_label_new ("Column3");

  size_group1 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  size_group2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  GListStore *store = g_list_store_new (GD_TYPE_DATA);
  model_size = 15000;
  for (i = 0; i < model_size; i ++)
    {
      GdData *d = g_object_new (GD_TYPE_DATA, NULL);
      d->model_size = model_size;
      d->index = i;
      d->text = "fpoobar'lsfasdf asdf asdfas df asd fasd f asdf as dfewrthuier htuiheasruig hdrhfughseduig hisdfiugsdhiugisdf";
      g_list_store_append (store, d);
    }


  gd_model_list_box_set_model (GD_MODEL_LIST_BOX (list), G_LIST_MODEL (store));
  gd_model_list_box_set_fill_func (GD_MODEL_LIST_BOX (list), fill_func, header);

  gtk_container_add (GTK_CONTAINER (header), column_header1);
  gtk_container_add (GTK_CONTAINER (header), column_header2);
  gtk_container_add (GTK_CONTAINER (header), column_header3);
  gtk_container_add (GTK_CONTAINER (box), header);

  gtk_widget_set_vexpand (scroller, TRUE);
  gtk_container_add (GTK_CONTAINER (scroller), list);
  gtk_container_add (GTK_CONTAINER (box), scroller);
  gtk_container_add (GTK_CONTAINER (window), box);

  g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_main_quit), NULL);
  gtk_window_resize (GTK_WINDOW (window), 500, 200);
  gtk_widget_show_all (window);
  gtk_main ();
  return 0;
}
