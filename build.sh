gcc ./main.c `pkg-config --cflags --libs gtk+-3.0` ./sine_approx_256_1_5.c -o harmonics-$(uname -p) -s -lm -O2 -I/usr/include/gtk-3.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/pango-1.0 -I/usr/include/harfbuzz -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/atk-1.0
 
