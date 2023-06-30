ASL
===

This program creates a webserver for personal use.

## PLAN

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
