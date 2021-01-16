
confrm-arduino-esp32
====================

.. image:: https://github.com/confrm/confrm-arduino-esp32/workflows/Test/Build/badge.svg

This library implements the API to enable use of the confrm server.

For more on the server see https://github.com/confrm/confrm, for quickstart guides go to https://confrm.io.

Prerequisites
-------------

1) OTA + SPIFFS - confrm will not function if it cannot do over the air updates, and it requires some config space to store persistent information.

2) Some space - on the ESP32 Devkit v1 a basic confrm compatible application will take up ~70% of the 1.5 MB program space. The vast majority of this is the HTTP libraries. If your program is not using HTTP and the device is fairly full then this might not fit.

Installing the library
----------------------

(Arduino IDE integration coming soon)

For now the library has to be added manually. There are a number of good tutorials on adding libraries manually, for example [https://www.digikey.co.uk/en/maker/blogs/2018/how-to-install-arduino-libraries].

Adding it to your project
-------------------------

At the end of the includes add::

  #include <confrm.h>

After the includes and defines add::

  Confrm *confrm;

In the `void setup()` function add::

  confrm = new Confrm("mypackage", "http://192.168.0.100:8000", "Test Sensor", "esp32", 10);

This sets confrm to look for updates to the "mypackage" package, and that the server is located at the given IP address. The rest of the arguments are optional.

The third argument sets a readable name for this package (useful for debugging) and the platform type (defaults to esp32 for this library) and the 10 sets how often to poll for updates. This defaults to 60 seconds.

Program the ESP32 with the above added and the node will show up in the confrm server nodes screen.

If you have configurations set up, you can load them in using::

  const String mqtt_server = confrm->get_config("mqtt_server");

It will return an empty string on error.


____

