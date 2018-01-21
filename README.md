# Arduino ESP8266 + 2.9" e-paper module door sign

This is a basic project to display a public Google calendar on a low-power e-ink
screen, suitable for doors to show when the room is free or the occupant will be
there.

Hardware used: https://www.tindie.com/products/squix78/esp8266-29-espaper-plus-module-e-ink/

It wakes up once a day, connects to the specified wifi network, synchronizes the
time through NTP, and retrieves the entries in the specifed Google calendar for
the next 5-7 days, depending on what day of the week it runs on (weekends are
excluded). It then does some rudimentary parsing of the entries and displays
time and title.

There's no checking for overflows of any kind, i.e. event titles too long or too
many events for one day. Up to 4 events a day fit on the display, and titles can
be a word or two.
