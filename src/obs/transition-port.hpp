#pragma once

// The OBS ports for the Transitions dock. Free of obs.h.
//   TransitionSet      one canvas's transitions: OBS's own for the main canvas (obs::FrontendTransitions), or a
//                      Fiji Suite canvas dock's private ones (implemented by the canvas dock).
//   TransitionCatalog  what is shared: the transition types and OBS's transition duration.

#include "../model/transitions.hpp"

#include <QList>
#include <QString>

namespace fiji::obs {

class TransitionSet {
public:
	virtual ~TransitionSet() = default;

	// A canvas dock's own list (true), or OBS's main one (false): the main one adds through OBS's own dialog,
	// cannot copy from itself and is not trimmed here.
	virtual bool ownList() const = 0;
	virtual QList<model::TransitionInfo> transitions() = 0;
	virtual QString current() = 0;
	virtual void select(const QString &name) = 0;
	virtual bool copyFromMain(const QString &mainName) = 0;
	// Makes a transition of this type, selects it and opens its properties. name: the one the user chose (ownList).
	virtual bool addNew(const QString &typeId, const QString &name) = 0;
	virtual bool remove(const QString &name) = 0;
	virtual bool rename(const QString &name, const QString &newName) = 0;
	virtual void openProperties(const QString &name) = 0;
};

class TransitionCatalog {
public:
	virtual ~TransitionCatalog() = default;
	virtual QList<model::TransitionType> types() = 0; // the ones with properties
	virtual int duration() = 0;                         // ms
	virtual void setDuration(int ms) = 0;
};

} // namespace fiji::obs
