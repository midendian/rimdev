
INCLUDES = -I. -I../include
# `gtk-config --cflags`

bin_PROGRAMS = rimsim

rimsim_SOURCES = \
	rimsim.c \
	scheduler.c \
	gui.c

rimsim_LDADD = \
	-L$(top_srcdir)/simulation/rimos/libc -lrimsim_simul_rimos_libc \
	-L$(top_srcdir)/simulation/rimos/core -lrimsim_simul_rimos_core \
	-L$(top_srcdir)/simulation/rimos/lcd -lrimsim_simul_rimos_lcd \
	-L$(top_srcdir)/simulation/rimos/db -lrimsim_simul_rimos_db

#rimsim_LDFLAGS = \
#	`gtk-config --libs gthread`

