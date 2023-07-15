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

The server scans its directory for image files and creates a randomized list
of them.
Get a page that renders one face of a card, the "active" face. The card is
two sided and either side can be a photo or text. Either side of the card can
have a note which should be displayed when that side of the card is active.

The user can then click a button to reveal the inactive face of the card. And
when they do the reveal button goes away and is replaced by three buttons that
describes their confidence level on their recall of the inactive face of the
card.

When the user clicks on one of these three buttons, their confidence level is
logged for that card as a rating.


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
