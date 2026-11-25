#include "release-version.hpp"

namespace fiji::model {

ReleaseVersion parseVersion(const QString &text)
{
	ReleaseVersion v;
	int i = 0;
	if (i < text.size() && (text[i] == QLatin1Char('v') || text[i] == QLatin1Char('V'))) {
		i++;
	}
	for (int k = 0; k < 3; k++) {
		if (i >= text.size() || !text[i].isDigit()) {
			return v;
		}
		int n = 0;
		while (i < text.size() && text[i].isDigit()) {
			n = n * 10 + text[i].digitValue();
			if (n > 1000000) {
				return v;
			}
			i++;
		}
		v.part[k] = n;
		if (k < 2) {
			if (i >= text.size() || text[i] != QLatin1Char('.')) {
				return v;
			}
			i++;
		}
	}
	if (i < text.size()) {
		if (text[i] != QLatin1Char('-') && text[i] != QLatin1Char('+')) {
			return v;
		}
		v.prerelease = text[i] == QLatin1Char('-'); // "+..." is build metadata and does not change the version
	}
	v.valid = true;
	return v;
}

bool isNewer(const QString &latest, const QString &current)
{
	const auto a = parseVersion(latest);
	const auto b = parseVersion(current);
	if (!a.valid || !b.valid) {
		return false;
	}
	for (int k = 0; k < 3; k++) {
		if (a.part[k] != b.part[k]) {
			return a.part[k] > b.part[k];
		}
	}
	return b.prerelease && !a.prerelease;
}

QString bareVersion(const QString &tag)
{
	return tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V')) ? tag.mid(1) : tag;
}

} // namespace fiji::model
