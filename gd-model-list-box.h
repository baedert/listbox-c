#ifndef _GD_MODEL_LIST_BOX_H
#define _GD_MODEL_LIST_BOX_H

#include <gtk/gtk.h>

struct _GdModelListBox
{
  GtkContainer parent_instance;
};

struct _GdModelListBoxClass
{
  GtkWidgetClass parent_class;
};

typedef struct _GdModelListBox GdModelListBox;

#define GD_TYPE_MODEL_LIST_BOX gd_model_list_box_get_type ()

G_DECLARE_FINAL_TYPE (GdModelListBox, gd_model_list_box, GD, MODEL_LIST_BOX, GtkContainer)

GtkWidget * gd_model_list_box_new (void);

void gd_model_list_box_set_model (GdModelListBox *box, GListModel *model);

GListModel *gd_model_list_box_get_model (GdModelListBox *box);

typedef GtkWidget * (*GdModelListBoxFillFunc) (gpointer  item,
                                               GtkWidget *widget,
                                               guint      item_index,
                                               gpointer   user_data);

typedef void (*GdModelListBoxRemoveFunc) (GtkWidget *widget,
                                          gpointer   item);



void gd_model_list_box_set_fill_func (GdModelListBox         *box,
                                      GdModelListBoxFillFunc  func,
                                      gpointer                user_data);

void gd_model_list_box_set_remove_func (GdModelListBox            *box,
                                        GdModelListBoxRemoveFunc  func,
                                        gpointer                  user_data);

void gd_model_list_box_set_placeholder (GdModelListBox *box, GtkWidget *placeholder);
#endif
