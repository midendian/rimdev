/* -*- Mode: ab-c -*- */

#include <rimsim.h>
#include <gtk/gtk.h>

static GdkPixmap *lcdpix = NULL;
static GtkWidget *lcdglob = NULL;
static GdkFont *lcdfont = NULL;

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

/* XXX the pixmap should be a fixed size no matter what */
static gint lcd_configure(GtkWidget *widget, GdkEventConfigure *event)
{

	if (lcdpix)
		gdk_pixmap_unref(lcdpix);

	lcdpix = gdk_pixmap_new(widget->window,
							widget->allocation.width,
							widget->allocation.height,
							-1);

	gdk_draw_rectangle(lcdpix,
					   widget->style->white_gc,
					   TRUE,
					   0, 0,
					   widget->allocation.width,
					   widget->allocation.height);

	return TRUE;
}

static gboolean lcd_expose(GtkWidget *widget, GdkEventExpose *event)
{

	gdk_draw_pixmap(widget->window,
					widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
					lcdpix,
					event->area.x, event->area.y,
					event->area.x, event->area.y,
					event->area.width, event->area.height);

#if 0

	gdk_window_clear_area(widget->window,
						  event->area.x, event->area.y,
						  event->area.width, event->area.height);
#endif
#if 0
	gdk_draw_rectangle (widget->window,
						widget->style->black_gc,
						TRUE,
						event->area.x, event->area.y, 10, 10);
#endif

	return FALSE;
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

static void updatelcd(void)
{
	GdkRectangle r;

	if (!lcdglob)
		return;

	/* XXX could be more selective about this */
	r.x = 0;
	r.y = 0;
	r.width = lcdglob->allocation.width;
	r.height = lcdglob->allocation.height;

	fprintf(stderr, "updatelcd: %dx%d+%d+%d\n", r.width, r.height, r.x, r.y);

#if 0
	gtk_widget_draw(lcdglob, &r);
#else
	gtk_widget_queue_draw(lcdglob);
#endif
	return;
}

void gui_putstring(int x, int y, const char *str, int len)
{

	gdk_threads_enter();

	gdk_draw_text(lcdpix, lcdfont, lcdglob->style->black_gc, x, y, str, len);

	updatelcd();

	gdk_threads_leave();

	return;
}

/* XXX i'm sure theres a better way of doing this */
void gui_scroll(int pixels)
{
	GdkPixmap *newlcd;

	if (!pixels)
		return; /* no point */

	gdk_threads_enter();

	/*
	 * Create a new pixmap, then draw the parts we want
	 * to keep into the new one. 
	 */

    newlcd = gdk_pixmap_new(lcdglob->window,
							lcdglob->allocation.width,
							lcdglob->allocation.height,
							-1);

	gdk_draw_rectangle(newlcd,
					   lcdglob->style->white_gc,
					   TRUE,
					   0, 0,
					   lcdglob->allocation.width,
					   lcdglob->allocation.height);

	gdk_draw_pixmap(newlcd, 
					lcdglob->style->black_gc,
					lcdpix,
					0, (pixels > 0) ? pixels : 0,
					0, (pixels < 0) ? -pixels : 0,
					lcdglob->allocation.width,
					lcdglob->allocation.height);

	gdk_pixmap_unref(lcdpix);
	lcdpix = newlcd;

	updatelcd();

	gdk_threads_leave();

	return;
}

void gui_drawline(int type, int x1, int y1, int x2, int y2)
{

	gdk_threads_enter();

	/* XXX DRAW_INVERT */
	if (type == DRAW_BLACK)
		gdk_draw_line(lcdpix, lcdglob->style->black_gc, x1, y1, x2, y2);
	else if (type == DRAW_WHITE)
		gdk_draw_line(lcdpix, lcdglob->style->white_gc, x1, y1, x2, y2);

	updatelcd();

	gdk_threads_leave();

	return;
}

const static BYTE _abcart2[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x04, 0x44, 0x97, 0x85, 0x97, 0x44, 0x04, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x0A, 0x0B, 0x1E, 0xFE, 0xFF, 0x16, 0x56, 0x06, 0xFE, 0x02, 0x76, 0x06, 0xFF, 0xFE, 0x1E, 0x0B, 0x0A, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x10, 0x00, 0x00, 0x10, 0x10, 0x10, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x18, 0x1F, 0x1F, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x1F, 0x1F, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x10, 0x10, 0x00, 0x00, 0x10
};

const static BitMap abcart2 = { 0,
 73, 21,
(BYTE *) _abcart2
};


static void dumpbm(unsigned char *data, int width, int height)
{
	int i,j,k;

	printf("width1 = %d\n", width);
	width = (width % 8) ? (width / 8) + 1 : (width / 8);

	printf("width2 = %d\n", width);

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			for (k = 7; k >= 0; k--)
				fprintf(stderr, "%d", (data[(i * width)+j] >> k) & 1 ? 1 : 0); 
			fprintf(stderr, " ");
		}
		fprintf(stderr, "\n");
	}

	return;
}

void gui_putbitmap(const unsigned char *bitmap, int width, int height, int destx, int desty)
{
	GdkBitmap *bm;

	fprintf(stderr, "putbitmap(%p, %d, %d, %d, %d)\n", bitmap, width, height, destx, desty);

	//dumpbm(bitmap, width, height);
	dumpbm(abcart2.data, abcart2.wide, abcart2.high);

	gdk_threads_enter();

	if (!(bm = gdk_bitmap_create_from_data(lcdglob->window, bitmap, width, height)))
		abort();

	gdk_draw_pixmap(lcdpix, 
					lcdglob->style->black_gc,
					bm,
					0, 0,
					destx, desty,
					width, height);

	updatelcd();

	gdk_bitmap_unref(bm);

	gdk_threads_leave();

	return;
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

	if (!(lcdfont = gdk_font_load("-*-lucida-medium-r-*-*-8-*-*-*-*-*-*-*")))
		abort();

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_signal_connect(GTK_OBJECT(window), "destroy",
					   GTK_SIGNAL_FUNC(destroy), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event", 
					   GTK_SIGNAL_FUNC(lcd_keyrelease), NULL);

	gtk_container_set_border_width(GTK_CONTAINER (window), 10);

	vbox = gtk_vbox_new(FALSE, 0);

	lcdglob = lcd = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(lcd), 160, 160);
	gtk_box_pack_start(GTK_BOX(vbox), lcd, FALSE, FALSE, 0);

	gtk_signal_connect(GTK_OBJECT(lcd), "expose_event", 
					   GTK_SIGNAL_FUNC(lcd_expose), NULL);
	gtk_signal_connect(GTK_OBJECT(lcd), "configure_event", 
					   GTK_SIGNAL_FUNC(lcd_configure), NULL);

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
