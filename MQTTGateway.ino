#pragma board esp8266-generic

#pragma option CpuFrequency = 160
#pragma option CrystalFreq = 26
#pragma option Debug = Disabled
#pragma option DebugLevel = None____
#pragma option FlashErase = sdk
#pragma option FlashFreq = 40
#pragma option FlashMode = dio
#pragma option FlashSize = 1M512
#pragma option LwIPVariant = v2mss536
#pragma option ResetMethod = ck
#pragma option UploadSpeed = 230400
#pragma option led = 4
#pragma option warnings = off

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <CLI.h>

// You don't want to go too fast - an Arduino's
// SoftwareSerial is no use at 115200 or more.
#define BAUD 9600



#define ERR_OK dev->println("OK"); return 0;

#define ERR_BAD_SYNTAX dev->println("ERR 10 Bad syntax"); return 0;
#define ERR_UNKNOWN_KEY dev->println("ERR 11 Unknown key"); return 0;
#define ERR_NOT_CONNECTED dev->println("ERR 12 Not connected"); return 0;
#define ERR_NOMEM dev->println("ERR 13 Out of memory"); return 0;


#define EVT_WIFI_CONNECT CLI.print("%EVT 01 WiFi connected: "); CLI.println(WiFi.localIP());
#define EVT_WIFI_DISCONNECT CLI.println("%EVT 02 WiFi disconnected");
#define EVT_MQTT_CONNECT CLI.println("%EVT 03 MQTT connected");
#define EVT_MQTT_DISCONNECT CLI.println("%EVT 04 MQTT disconnected");

struct kvpair {
    const char *key;
    char *value;
    void (*onChange)();
};

struct subscription {
    char *topic;
    struct subscription *next;
};

static int wifiConnected = 0;
static int mqttConnected = 0;
    


void updateWifi();
void updateMQTTServer();
void updateMQTT();

struct kvpair settings[] = {
    { "ssid", NULL, updateWifi },
    { "psk", NULL, updateWifi },
    { "server", NULL, updateMQTTServer },
    { "port", NULL, updateMQTTServer },
    { "devname", NULL, updateMQTT },
    { "user", NULL, updateMQTT },
    { "pass", NULL, updateMQTT },
    { NULL, NULL, NULL }
};

struct subscription *subs = NULL;

WiFiClient mqttClient;
PubSubClient mqtt(mqttClient);


CLI_COMMAND(cmd_set) {
    if (argc != 3) {
        ERR_BAD_SYNTAX
    }

    for (struct kvpair *p = settings; p->key != NULL; p++) {
        if (!strcasecmp(p->key, argv[1])) {
            if (p->value != NULL) {
                free(p->value);
                p->value = NULL;
            }
            p->value = strdup(argv[2]);
            if (p->onChange != NULL) {
                p->onChange();
            }
            ERR_OK
        }
    }

    ERR_UNKNOWN_KEY
}

CLI_COMMAND(cmd_get) {
    if (argc == 1) {
        for (struct kvpair *p = settings; p->key != NULL; p++) {
            dev->print("|");
            dev->print(p->key);
            dev->print(" ");
            if (p->value == NULL) {
                dev->println("[unset]");
            } else {
                dev->println(p->value);
            }
        }
        ERR_OK
    } 

    if (argc == 2) {
        for (struct kvpair *p = settings; p->key != NULL; p++) {
            if (!strcasecmp(p->key, argv[1])) {
                dev->print("|");
                dev->print(p->key);
                dev->print(" ");
                if (p->value == NULL) {
                    dev->println("[unset]");
                } else {
                    dev->println(p->value);
                }
                ERR_OK
            }
        }        
        ERR_UNKNOWN_KEY
    }
    ERR_BAD_SYNTAX
}

CLI_COMMAND(cmd_unset) {
    if (argc != 2) {
        ERR_BAD_SYNTAX
    }

    for (struct kvpair *p = settings; p->key != NULL; p++) {
        if (!strcasecmp(p->key, argv[1])) {
            if (p->value != NULL) {
                free(p->value);
                p->value = NULL;
            }
            if (p->onChange != NULL) {
                p->onChange();
            }
            ERR_OK
        }
    }

    ERR_UNKNOWN_KEY
}

CLI_COMMAND(cmd_status) {
    if (argc != 1) {
        ERR_BAD_SYNTAX
    }

    dev->print("|WiFi: ");
    dev->println(wifiConnected ? "Connected" : "Disconnected");
    dev->print("|IP Address: ");
    dev->println(wifiConnected ? WiFi.localIP() : IPAddress(0, 0, 0, 0));
    dev->print("|Netmask: ");
    dev->println(wifiConnected ? WiFi.subnetMask() : IPAddress(0, 0, 0, 0));
    dev->print("|Gateway: ");
    dev->println(wifiConnected ? WiFi.gatewayIP() : IPAddress(0, 0, 0, 0));
    dev->print("|MQTT: ");
    dev->println(mqttConnected ? "Connected" : "Disconnected");


    int count = 0;
    for (struct subscription *s = subs; s != NULL; s = s->next) {
        count++;
    }

    dev->print("|Subscriptions: ");
    dev->println(count);
    for (struct subscription *s = subs; s != NULL; s = s->next) {
        dev->print("|    ");
        dev->println(s->topic);
    }
    ERR_OK
}

CLI_COMMAND(cmd_publish) {
    if (!mqttConnected) {
        ERR_NOT_CONNECTED
    }
    
    if (argc == 3) {
        mqtt.publish(argv[1], argv[2], false);
        ERR_OK
    }

    if (argc == 4) {
        if (!strcasecmp(argv[1], "retain")) {
            mqtt.publish(argv[2], argv[3], true);
            ERR_OK
        }
    }
    ERR_BAD_SYNTAX
}

CLI_COMMAND(cmd_subscribe) {
    if (argc != 2) {
        ERR_BAD_SYNTAX
    }

    // First subscription
    if (subs == NULL) {
        subs = (struct subscription *)malloc(sizeof(struct subscription));
        if (subs == NULL) {
            ERR_NOMEM
        }
        subs->next = NULL;
        subs->topic = strdup(argv[1]);
        if (mqttConnected) {
            mqtt.subscribe(argv[1]);
        }
        ERR_OK
    }

    // Find duplicate
    for (struct subscription *s = subs; s != NULL; s = s->next) {
        if (!strcmp(s->topic, argv[1])) {
            if (mqttConnected) {
                mqtt.subscribe(argv[1]);
            }
            ERR_OK
        }
    }

    struct subscription *s = subs;



    while (s->next != NULL) {
        s = s->next;
    }

    struct subscription *n = (struct subscription *)malloc(sizeof(struct subscription));
    if (n == NULL) {
        ERR_NOMEM
    }
    n->next = NULL;
    n->topic = strdup(argv[1]);
    s->next = n;
    if (mqttConnected) {
        mqtt.subscribe(argv[1]);
    }
    ERR_OK
}

CLI_COMMAND(cmd_unsubscribe) {
    if (argc != 2) {
        ERR_BAD_SYNTAX
    }

    if (subs == NULL) { // No subs - do nothing
        ERR_UNKNOWN_KEY
    }

    if (!strcmp(subs->topic, argv[1])) {
        struct subscription *s = subs;
        subs = subs->next;
        free(s);
        ERR_OK
    }

    for (struct subscription *s = subs; s->next != NULL; s = s->next) {
        if (!strcmp(s->next->topic, argv[1])) {
            struct subscription *del = s->next;
            s->next = s->next->next;
            free(del);
            ERR_OK
        }
    }
    ERR_UNKNOWN_KEY
}

CLI_COMMAND(cmd_help) {
    dev->println("|set <key> <value>                Set a configuration value");
    dev->println("|unset <key>                      Clear a configuration value");
    dev->println("|get [key]                        Get configuration values");
    dev->println("|status                           Show system status");
    dev->println("|sub <topic>                      Subscribe to a topic");
    dev->println("|unsub <topic>                    Unsubscribe from a topic");
    dev->println("|pub [retain] <topic> <message>   Publish a message");
    ERR_OK
}


char *getKey(const char *k) {
    for (struct kvpair *p = settings; p->key != NULL; p++) {
        if (!strcasecmp(p->key, k)) {
            return p->value;
        }
    }
    return NULL;
}

void setKey(const char *k, const char *v) {
    for (struct kvpair *p = settings; p->key != NULL; p++) {
        if (!strcasecmp(p->key, k)) {
            if (p->value != NULL) {
                free(p->value);
                p->value = NULL;
            }
            p->value = strdup(v);
            if (p->onChange != NULL) {
                p->onChange();
            }
            return;
        }
    }    
}

void updateWifi() {
//    WiFi.end();
    char *ssid = getKey("ssid");
    char *psk = getKey("psk");
    if (ssid != NULL && psk != NULL) {
        WiFi.begin(getKey("ssid"), getKey("psk"));
    }
}

void updateMQTTServer() {
    char *host = getKey("server");
    char *port = getKey("port");

    if (host != NULL && port != NULL) {
        int p = atoi(port);
        mqtt.setServer(host, p);
    }
    mqtt.disconnect();
    mqttConnected = 0;
}

void updateMQTT() {
    mqtt.disconnect();
    mqttConnected = 0;
}


void callback(char *topic, byte *payload, unsigned int length) {
    CLI.print("%PUB ");
    CLI.print(topic);
    CLI.print(" ");
    for(unsigned int i = 0; i < length; i++) {
        CLI.print((char)payload[i]);
    }
    CLI.println();
}

void setup() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    
    Serial.begin(BAUD);
    CLI.setDefaultPrompt("");

    CLI.addCommand("set", cmd_set);
    CLI.addCommand("get", cmd_get);
    CLI.addCommand("unset", cmd_unset);
    CLI.addCommand("status", cmd_status);
    CLI.addCommand("pub", cmd_publish);
    CLI.addCommand("sub", cmd_subscribe);
    CLI.addCommand("unsub", cmd_unsubscribe);

    CLI.addCommand("help", cmd_help);
    
    CLI.addClient(Serial);

    setKey("ssid", WiFi.SSID().c_str());
    setKey("psk", WiFi.psk().c_str());
    setKey("port", "1883");

    mqtt.setCallback(callback);
    
    updateWifi();
    updateMQTT();
}

void subscribeAllTopics() {
    for (struct subscription *s = subs; s != NULL; s = s->next) {
        mqtt.subscribe(s->topic);
    }
}

void loop() {

    static uint32_t connectTimeStamp = millis();

    CLI.process();

    if (WiFi.status() == WL_CONNECTED) {
        if (wifiConnected == 0) {
            wifiConnected = 1;
            EVT_WIFI_CONNECT
        }
    } else {
        if (wifiConnected == 1) {
            wifiConnected = 0;
            EVT_WIFI_DISCONNECT
            if (mqttConnected == 1) {
                mqttConnected = 0;
                EVT_MQTT_DISCONNECT
            }
        }
    }

    if (wifiConnected == 1) {
        if (mqttConnected == 0) {
            if (mqtt.connected()) {
                mqttConnected = 1;
                EVT_MQTT_CONNECT
                subscribeAllTopics();
            }
        } else {
            if (!mqtt.connected()) {
                mqttConnected = 0;
                EVT_MQTT_DISCONNECT
            }
        }

        if (mqttConnected == 0) {
            // Connecting is a blocking operation. Don't do it
            // too often or it futzes with the serial communications.
            if (millis() - connectTimeStamp >= 5000) {
                connectTimeStamp = millis();
                char *server = getKey("server");
                char *port = getKey("port");
                if (server != NULL && port != NULL) {
                    char *user = getKey("user");
                    char *pass = getKey("pass");
                    char *dev = getKey("devname");
                    if (dev != NULL) { // MUST have a device name!
                        if (user == NULL && pass == NULL) { // No creds
                            mqtt.connect(dev);                    
                        } else if (user != NULL && pass != NULL) { // Both creds
                            mqtt.connect(dev, user, pass);
                        }
                    }
                }
            }
        }

        mqtt.loop();
    } 
   
}
