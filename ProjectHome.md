[Issue 377](https://code.google.com/p/callhome/issues/detail?id=377) of App Engine is about how servers can notify clients. Instead of having GAE support comet (which would be a complete change of semantics), CallHome allows gae apps to do a urlfetch that will tell the javascript app to poll.

The callHome program can run anywhere, but ideally would run close to the web client, minimizing web traffic. It is simple with no long term state and could run in the user's firewall/NAT box.