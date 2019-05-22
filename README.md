MQTTGateway
===========

This sketch will turn an ESP-01 into a UART to MQTT gateway.  It uses a very simple
serial protocol to configure the WiFi and MQTT serrtings, and to subscribe to and
publish messages to topics.

Requires my CLI library: https://github.com/MajenkoLibraries/CLI

The default baud rate is 9600.

Commands
--------

    set <key> <value>

Sets a configuration value. Current values are:

* ssid - the WiFi SSID to connect to
* psk - the pre-shared key (password) for your WiFi connection
* server - the MQTT server to connect to
* port - the port of the MQTT server (1883)
* devid - the client "device ID" of this unit
* user - login username for the MQTT server
* pass - login password for the MQTT server

----

    get [key]

Gets all the (or one specified) configuration values.

----

    unset <key>

Clears the value associated with one of the configuration settings

----

    status

Displays the current status of the system: connection status of both WiFi
and MQTT, IP addresses, and any topics subscribed to.

----

    sub <topic>

Subscribe to a topic.

----

    unsub <topic>

Unsubscribe from a topic.

----

    pub [retain] <topic> <message>

Publish (and optionally retain) a message to a topic.  If the topic contains spaces it should be wrapped in quotes.

----

All commands return either "OK" or "ERR" followed by an error message.  Any extra
information is returned before the OK or ERR and is prefixed by |

Unsolicited messages (event and publish notifications) are prefixed with %.  There are
two basic types:

* Event notifications

These all start with `%EVT` and notify you of system events, including WiFi connection
and disconnection and MQTT connection and disconnection.  For example:

    %EVT WiFi connected
    %EVT WiFi disconnected
    %EVT MQTT connected
    %EVT MQTT disconnected

* Publication events

When a message is published to one of the topics you are subscribed to the topic
and message are given to you as a PUB event.  For instance:

    %PUB topic/you/sub/to This is your message

Or:

    %PUB weather/temperature 34.2


