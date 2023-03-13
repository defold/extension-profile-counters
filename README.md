# Extension profile counters

This is an advanced example of retrieving some of the profiling data at runtime.

In this example the profile counters are available even in release mode.
This is achieved by using an [app manifest](./game.appmanifest) in order to remove some engine libraries.
(which this extension replaces).

Note!
When using this extension, you remove the regular profiler that is normally included in Defold!

# Script api

This extension provides a lua table with the latest counters through the `profile` namespace:

```Lua
local counters = profile.get_properties()
pprint("Counters", counters)
```

