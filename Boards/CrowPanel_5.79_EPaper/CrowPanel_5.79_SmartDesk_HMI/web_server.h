#pragma once
#include <Arduino.h>
#include "ui_engine.h"

void web_server_init();
void web_server_handle();
void web_server_set_nameboard_data(const NameboardData& data);
NameboardData web_server_get_nameboard_data();
