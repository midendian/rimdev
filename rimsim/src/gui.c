/* -*- Mode: ab-c -*- */

#include <rimsim.h>
#include <gtk/gtk.h>

static void destroy(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();

	exit(2);

	return;
}

static void *guithread(void *arg)
{

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return NULL;
}

static gint lcd_expose(GtkWidget *widget, GdkEventExpose *event)
{

	gdk_window_clear_area(widget->window,
						  event->area.x, event->area.y,
						  event->area.width, event->area.height);

	gdk_draw_rectangle (widget->window,
						widget->style->black_gc,
						TRUE,
						event->area.x, event->area.y, 10, 10);

	return TRUE;
}

int gui_start(int *argc, char ***argv)
{
	pthread_t guitid;
	GtkWidget *window, *label, *vbox, *debugtext, *lcd;

	g_thread_init(NULL);

	gtk_init(argc, argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_signal_connect(GTK_OBJECT(window), "destroy",
					   GTK_SIGNAL_FUNC(destroy), NULL);

	gtk_container_set_border_width(GTK_CONTAINER (window), 10);

	vbox = gtk_vbox_new(FALSE, 0);

	lcd = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(lcd), 160, 160);
	gtk_box_pack_start(GTK_BOX(vbox), lcd, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(lcd), "expose_event", GTK_SIGNAL_FUNC(lcd_expose), NULL);

	label = gtk_label_new("And now for something completely different ...");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	debugtext = gtk_text_new(NULL, NULL);
	gtk_text_insert(GTK_TEXT(debugtext), NULL, NULL, NULL, "Debug window\n", -1); 
	gtk_box_pack_start(GTK_BOX(vbox), debugtext, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtk_widget_show(debugtext);
	gtk_widget_show(label);
	gtk_widget_show(vbox);
	gtk_widget_show(lcd);
	gtk_widget_show(window);

	pthread_create(&guitid, NULL, guithread, NULL);

	return 0;
}
