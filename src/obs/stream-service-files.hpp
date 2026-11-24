#pragma once

// OBS side of the stream output dialog: rtmp-services' services.json and twitch_ingests.json, read into the server
// lists the dialog offers (model::serversOf / model::twitchIngests).

#include "../model/stream-output.hpp"

namespace fiji::obs {

// The servers for a service name (for "Twitch", its ingests first).
QList<model::StreamServer> StreamServersFor(const QString &service);

} // namespace fiji::obs
