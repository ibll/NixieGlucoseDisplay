#ifndef PARSE_HTTP_H
#define PARSE_HTTP_H

#include "WiFiS3.h"
#include "WiFiSSLClient.h"

String readLineWithTimeout(Client &client, unsigned long timeout = 100);
bool readHttpBody(Client &client, String &body, unsigned long timeout = 500);

#endif // PARSE_HTTP_H
