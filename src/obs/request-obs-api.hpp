#pragma once

// The real ObsApi, on top of the plugin's request table (the same requests obs-websocket clients and the local deck
// server use), so every libobs call stays where it already works. As docks and dialogs move into the layers, these
// calls move here as direct libobs calls. Runs on the UI thread (the dispatcher's consumer).

#include "obs-api.hpp"

namespace fiji::obs {

class RequestObsApi final : public ObsApi {
public:
	model::ObsSnapshot snapshot() override;
	ObsCall switchScene(const QString &scene, const QString &canvas) override;
	ObsCall setTransition(const QString &canvas, const QString &transition) override;
	ObsCall runTransition(const QString &canvas) override;
	ObsCall startOutputs(const QString &target) override;
	ObsCall stopOutputs(const QString &target) override;
	ObsCall webFile(const QString &name) override;
	ObsCall look() override;

private:
	ObsCall outputs(bool start, const QString &target);
};

} // namespace fiji::obs
