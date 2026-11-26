#pragma once

// OBS adapter: the main canvas's transitions (OBS's own, through the frontend API) and the transition catalogue.

#include "transition-port.hpp"

namespace fiji::obs {

class FrontendTransitions final : public TransitionSet, public TransitionCatalog {
public:
	bool ownList() const override { return false; }
	QList<model::TransitionInfo> transitions() override;
	QString current() override;
	void select(const QString &name) override;
	bool copyFromMain(const QString &) override { return false; }
	bool addNew(const QString &typeId, const QString &name) override; // OBS's own dialog names it
	bool remove(const QString &) override { return false; }            // OBS's own dock removes them
	bool rename(const QString &name, const QString &newName) override;
	void openProperties(const QString &name) override;

	QList<model::TransitionType> types() override;
	int duration() override;
	void setDuration(int ms) override;
};

} // namespace fiji::obs
