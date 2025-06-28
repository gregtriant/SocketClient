


class MqttClient {

    public:
        MqttClient(const char* host, const int port, const char* username, const char* password, const char* clientId, const char* topic);
        void publish(const char* topic, const char* payload);
        void subscribe(const char* topic);
        void loop();
        void reconnect();
        void setCallback(MQTT_CALLBACK_SIGNATURE);

    

}