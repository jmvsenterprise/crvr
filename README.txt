WEBSERVER
---------

This program creates a webserver for personal use.

For now this server is just going to work for me to serve me a website I can
spin up when I need that lets me study my American Sign Language signs using
spaced repetition learning. Yes, I could use various apps to do this for me but
I'm curious about how all this works so I wanted to have a go at creating a
webserver from scratch.

For now this serves very specific files, but once I've got it working for my
use case I have a few ideas for possible expansion which I may, or may not,
persue. Details follow.

TODO
----
Simplest viable product:

[x] The server scans its starting directory for all image files.
[x] It then creates an association of the card with the front or back of the card.
    The front of the card will be the image in the file, and the back of the card will be the
    file name of the image.

[x] The server then compiles a list of all the cards and whether it should show
    the front or back of the card and saves it as a quiz.

[x] Then, the server runs the quiz, which consists of showing each card once for
    both faces and quizzing the user to recall the other side of the card.
[x] They click the reveal button to show the other side of the card and then three
    buttons appear asking them to rate their recall level as either "Poor", 
    "Good", or "Great".
[ ] Their recall level, the image name, what face was shown originally are
    stored to a results file so I can later process that into a database or
    something.


FUTURE ENHANCEMENTS
-------------------
Creating quiz files and remembering card state long-term
Splitting out the ASL stuff into its own project the server loads via .so.
Uploading images to the server via an upload page
HTTPS

THE PLAN
--------
What I think I can do is have the webserver load a shared object file as an
application. The shared object file would need to provide the following
functions:

int get(request, html)

Where path is a path and html stores the result of the get call.

int post(request, html)

Where path is a path and html stores the result of the post call.

int load()

Which loads the module (we probably need to hand back a context of some kind.

int unload()

Which unloads the module (probably takes a module pointer or something).

The application could be written in C or C++ if I organize the API correctly.
But my thinking is the webserver will load an application by loading its SO
file and calling its load() function so it can initialize itself. Then the
webserver will wait for requests. When a request comes in it identifies the
type of request as GET or POST, and calls either get or post accordingly.

Although... probably could just send in the generic request instead.
