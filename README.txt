CRVR - C[e]RV[e]R
-----------------

This program creates a webserver.

For now this server is just going to work for me to serve me a website I can
spin up when I need that lets me study my American Sign Language signs using
spaced repetition learning. Yes, I could use various apps to do this for me but
I'm curious about how all this works so I wanted to have a go at creating a
webserver from scratch.

I tried to vaguely follow NASA coding standards here, although I definitly
switched to just-make-it-go mode several times. But I'd like to get it back to
the rules of 10 at some point.

This server serves any file from the directory its launched in or its
subdirectories. So to launch your server navigate to your web root directory
and run the server binary. It does, however, have a special page: asl.html,
which redirects to an embedded "application", which expects to find the
asl.html and asl_done.html. Eventually I want to extract these into a loadable
shared object file in the future, but my primary goal was to get this working
for me. So stay tuned for that in the future.

BUILDING
--------

--- Unix/Linux/Mac ---

Copy or symlink unix.mk to config.mk so the makefile will find config.mk, then run
make.

--- Windows ---

TODO, although cygwin might just work. Haven't tried it though.

FUTURE ENHANCEMENTS
-------------------
Save card data when a quiz is completed. For each card, save the users's score
for the front and back of the card and when it should next be reviewed.

Split out the ASL specific stuff into its own project the server loads via .so.
The idea here is to have the .so be an "application" that the server can be
told to load with an attached URI. If that URI is specified in a request, the
server will send that request to the application to be processed. The
application will then respond to the appropriate request.

Get clang-tidy running on the code.

Get the windows build working for nmake (maybe?). Although a mingw build might
be a better place to start. Then github might be able to build it and provide
the binary for users. Ideally nmake should use the same makefile as the unix-
likes for maintainability purposes. But TBD.

Fix POST so it can handle uploading image files to photo-base deck building
easier.

Someday: HTTPS, because I'd like to learn how that magic works.
