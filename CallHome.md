# The CallHome server: introduction #

Many web services don't want to add comet capabilities, with endlessly running programs and many long running, mostly idle, client connections. Google App Engine is the particular case that lead to this proposal.

The long term idea is to provide a comet service close to the client. There is no reason why it can't be just outside (on the Internet side) of the user's firewall/NAT box provided by the ISP. However it can be anywhere, and in particular the plan is to ask Google to run a CallHome service to support GAE apps.

## How it works ##

The 3 parts are: The Web App server (e.g. GAE app), the browser client and the CallHome service.

  * The web app server knows the client user's CallHome server (though we expect that a default one will be allocated which the user can change).
  * The browser client connects to the web app server.
  * The web app server generates a random id (currently 64 bits, but maybe should be more)
  * The web app checks creates id on CallHome server (also checks is not already in use): idcreate URL.
  * Communicates id to the browser client. E.g. it might be set into a web page by a templating system.
  * The browser creates a hidden iframe and specifies connecting to the user's CallHome server with 2 parameters: id and the httpDomain of the web app (to be used as the 2nd parameter of postMessage, so can be `*`).
  * When the web app wants to communicate to the browser client it connects to a CallHome URL with two parameters: the id and some extra information.
  * The CallHome server now sends a message to the matching client, providing the extra information.
  * The browser client now connects to the web app to get the newly available information.

Note that the extra information communication is not certain. If two messages come to the CallHome server from the web app before a message can go to the browser client, then the extra information in the first will be lost. Typically the extra information is something like a server-to-client sequence number. If the client already has that sequence number it can ignore the CallHome message.