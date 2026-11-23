#include "obs-status.hpp"

#include <QJsonArray>

namespace fiji::model {

namespace {

QJsonArray strings(const QStringList &list)
{
	return QJsonArray::fromStringList(list);
}

QStringList strings(const QJsonValue &value)
{
	QStringList list;
	for (const auto &v : value.toArray()) {
		if (v.isString()) {
			list.append(v.toString());
		}
	}
	return list;
}

} // namespace

QJsonObject ObsSnapshot::toJson() const
{
	QJsonArray c, o, d;
	for (const auto &x : canvases) {
		c.append(QJsonObject{{"name", x.name},
				     {"main", x.main},
				     {"program", x.program},
				     {"preview", x.preview},
				     {"studioMode", x.studioMode},
				     {"scenes", strings(x.scenes)},
				     {"transitions", strings(x.transitions)}});
	}
	for (const auto &x : outputs) {
		o.append(QJsonObject{{"name", x.name},
				     {"kind", x.kind},
				     {"canvas", x.canvas},
				     {"group", x.group},
				     {"main", x.main},
				     {"active", x.active},
				     {"reconnecting", x.reconnecting},
				     {"uptime", double(x.uptime)},
				     {"droppedFrames", double(x.droppedFrames)},
				     {"totalFrames", double(x.totalFrames)}});
	}
	for (const auto &x : decks) {
		d.append(QJsonObject{{"id", x.id}, {"name", x.name}, {"buttons", x.buttons}});
	}
	return QJsonObject{{"canvases", c}, {"outputs", o}, {"decks", d}, {"running", strings(running)}};
}

ObsSnapshot ObsSnapshot::fromJson(const QJsonObject &json)
{
	ObsSnapshot s;
	for (const auto &v : json.value("canvases").toArray()) {
		const auto x = v.toObject();
		s.canvases.append(CanvasState{x.value("name").toString(), x.value("main").toBool(),
					      x.value("program").toString(), x.value("preview").toString(),
					      x.value("studioMode").toBool(), strings(x.value("scenes")),
					      strings(x.value("transitions"))});
	}
	for (const auto &v : json.value("outputs").toArray()) {
		const auto x = v.toObject();
		s.outputs.append(OutputStatus{x.value("name").toString(), x.value("kind").toString(),
					      x.value("canvas").toString(), x.value("group").toString(),
					      x.value("main").toBool(), x.value("active").toBool(),
					      x.value("reconnecting").toBool(), qint64(x.value("uptime").toDouble()),
					      qint64(x.value("droppedFrames").toDouble()),
					      qint64(x.value("totalFrames").toDouble())});
	}
	for (const auto &v : json.value("decks").toArray()) {
		const auto x = v.toObject();
		s.decks.append(DeckSummary{x.value("id").toString(), x.value("name").toString(), x.value("buttons").toInt()});
	}
	s.running = strings(json.value("running"));
	return s;
}

} // namespace fiji::model
