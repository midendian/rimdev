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

static gboolean lcd_expose(GtkWidget *widget, GdkEventExpose *event)
{

	gdk_window_clear_area(widget->window,
						  event->area.x, event->area.y,
						  event->area.width, event->area.height);

#if 0
	gdk_draw_rectangle (widget->window,
						widget->style->black_gc,
						TRUE,
						event->area.x, event->area.y, 10, 10);
#endif

	return TRUE;
}

static gboolean lcd_keyrelease(GtkWidget *widget, GdkEventKey *event)
{
	MESSAGE keypadmsg;

	keypadmsg.Device = DEVICE_KEYPAD;
	keypadmsg.Event = KEY_DOWN;

	keypadmsg.Length = 0;
	keypadmsg.DataPtr = NULL;

	keypadmsg.SubMsg = event->string[0];

	keypadmsg.Data[0] = 0;
	if (event->state & GDK_SHIFT_MASK)
		keypadmsg.Data[0] |= SHIFT_STATUS;
	if (event->state & GDK_LOCK_MASK)
		keypadmsg.Data[0] |= CAPS_LOCK;
   	if (event->state & GDK_CONTROL_MASK)
		keypadmsg.Data[0] |= ALT_STATUS;

	keypadmsg.Data[1] = 0;

	sendmessage(&keypadmsg, RIM_TASK_INVALID);

	return TRUE;
}

/* this is Bad. */
static GtkWidget *debugtext_global = NULL;

/* gaim. */
void gui_debugprintf(const char *fmt, va_list ap)
{
	gchar *s;

	gdk_threads_enter();

	s = g_strdup_vprintf(fmt, ap);

	if (debugtext_global)
		gtk_text_insert(GTK_TEXT(debugtext_global), NULL, NULL, NULL, s, -1);

	g_free(s);

	gdk_threads_leave();

	return;
}

int gui_start(int *argc, char ***argv)
{
	pthread_t guitid;
	GtkWidget *window, *label, *vbox, *debugtext, *lcd, *vscrollbar, *hbox;

	g_thread_init(NULL);

	gtk_init(argc, argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_signal_connect(GTK_OBJECT(window), "destroy",
					   GTK_SIGNAL_FUNC(destroy), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event", 
					   GTK_SIGNAL_FUNC(lcd_keyrelease), NULL);

	gtk_container_set_border_width(GTK_CONTAINER (window), 10);

	vbox = gtk_vbox_new(FALSE, 0);

	lcd = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(lcd), 160, 160);
	gtk_box_pack_start(GTK_BOX(vbox), lcd, FALSE, FALSE, 0);

	gtk_signal_connect(GTK_OBJECT(lcd), "expose_event", 
					   GTK_SIGNAL_FUNC(lcd_expose), NULL);

	label = gtk_label_new("And now for something completely different ...");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);


	hbox = gtk_hbox_new(FALSE, 0);

	debugtext = gtk_text_new(NULL, NULL);
	debugtext_global = debugtext;
	gtk_text_insert(GTK_TEXT(debugtext), NULL, NULL, NULL, "Debug window\n", -1); 
	gtk_box_pack_start(GTK_BOX(hbox), debugtext, TRUE, TRUE, 0);

	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(debugtext)->vadj);
	gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtk_widget_show(vscrollbar);
	gtk_widget_show(debugtext);
	gtk_widget_show(hbox);
	gtk_widget_show(label);
	gtk_widget_show(vbox);
	gtk_widget_show(lcd);
	gtk_widget_show(window);

	pthread_create(&guitid, NULL, guithread, NULL);

	return 0;
}
