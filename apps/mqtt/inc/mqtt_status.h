#ifndef MQTT_STATUS_H
#define MQTT_STATUS_H

/* Local observer API. No MQTT credentials, topics or NS payload changes. */
int mqtt_status_init(const char *directory);
void mqtt_status_connack(int rc);
void mqtt_status_subscribe(int mid, int rc);
void mqtt_status_suback(int mid, int count, const int *granted_qos);
void mqtt_status_disconnect(int rc);
void mqtt_status_connect_error(int rc);
void mqtt_status_fault(const char *reason, int rc);
void mqtt_status_tick(void);
void mqtt_status_stop(void);

#endif
