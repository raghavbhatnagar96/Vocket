compile:
				gcc client.c -o client -lpulse -lpulse-simple
				gcc server.c -o server -lpulse -lpulse-simple
dependencies:
				sudo apt-get install libpulse0 libpulse-mainloop-glib0 libglib2.0-dev libavahi-client-dev