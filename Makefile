# X_LIBPATH		  path used by the linker to find X libraries
# MOTIF_LIBPATH		  path used by the linker to find Motif libraries
# X_CFLAGS		  path used by the cpp to find X include files.
# MOTIF_CFLAGS		  path used by the cpp to find MOTIF include files.

X_LIBPATH  = -L/usr/local/lib
MOTIF_LIBPATH = -L /usr/local/lib/Motif1.1
X_CFLAGS = -I/usr/local/include
MOTIF_CFLAGS = -I/usr/include/Motif1.1

# linker flags to disable shared libraries for use with 7.0
LKFLAGS = -Wl,a
CC    = gcc -g
CFLAGS = $(X_CFLAGS)
LIBS1 = $(X_LIBPATH)  -lX11 -ll -lm
LIBS2 = $(X_LIBPATH)  -lX11 -lm

OBJS=	www.o file.o display.o scrollbar.o toolbar.o entities.o forms.o\
  status.o html.o parsehtml.o http.o cache.o ftp.o tcp.o nntp.o image.o gif.o

www: $(OBJS) www.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o www $(OBJS) $(LIBS2)

w3cache: w3cache.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o w3cache w3cache.o libgdbm.a

w3client: w3client.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o w3client w3client.o

tidy: tidy.c tidy.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o tidy tidy.c $(LIBS2)

relayd: relay.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o relayd relay.o $(LIBS2)

wrelay: wrelay.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o wrelay wrelay.o $(LIBS2)

testwwwd: testwwwd.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o testwwwd testwwwd.o $(LIBS2)

wwwd: wwwd.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o wwwd wwwd.o $(LIBS2)

tn: telnet.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o tn telnet.o $(LIBS2)

html: html.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o html html.o $(LIBS2)

html2latex: html2latex.o
	$(CC) -Aa -g -D_HPUX_SOURCE -o html2latex html2latex.o -lm -lmalloc -lPW
