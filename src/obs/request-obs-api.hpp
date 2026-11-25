#pragma once

// The real ObsApi, on top of the plugin's request table (the same requests obs-websocket clients and the local deck
// server use), so every libobs call stays where it already works. As docks and dialogs move into the layers, these
// calls move here as direct libobs calls. Runs on the UI thread (the dispatcher's consumer).

#include "obs-api.hpp"
#include "output-port.hpp"

namespace fiji::obs {

class RequestObsApi final : public ObsApi, public OutputBackend {
public:
	model::ObsSnapshot snapshot() override;
	ObsCall switchScene(const QString &scene, const QString &canvas) override;
	ObsCall setTransition(const QString &canvas, const QString &transition) override;
	ObsCall runTransition(const QString &canvas) override;
	ObsCall webFile(const QString &name) override;
	ObsCall look() override;


	// OutputBackend
	QList<model::OutputStatus> outputs() override;
	ObsCall startAll() override;
	ObsCall stopAll() override;
	ObsCall startGroup(const QString &group) override;
	ObsCall stopGroup(const QString &group) override;
	ObsCall startMain(const QString &kind) override;
	ObsCall stopMain(const QString &kind) override;
	ObsCall startOutput(const QString &name) override;
	ObsCall stopOutput(const QString &name) override;
};

} // namespace fiji::obs
