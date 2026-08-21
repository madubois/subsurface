// SPDX-License-Identifier: GPL-2.0
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QVector>
#include <QUrl>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/divelog.h"
#include "core/selection.h"

#include "qmlmapwidgethelper.h"
#include "core/divesite.h"
#include "core/qthelper.h"
#include "core/divefilter.h"
#include "commands/command.h"
#include "qt-models/maplocationmodel.h"
#include "qt-models/divelocationmodel.h"
#ifndef SUBSURFACE_MOBILE
#include "desktop-widgets/mapwidget.h"
#include <QFileDialog>
#endif

#define SMALL_CIRCLE_RADIUS_PX            26.0

MapWidgetHelper::MapWidgetHelper(QObject *parent) : QObject(parent)
{
	m_mapLocationModel = new MapLocationModel(this);
	m_smallCircleRadius = SMALL_CIRCLE_RADIUS_PX;
	m_map = nullptr;
	m_editMode = false;
	m_currentDs = nullptr;
	connect(&diveListNotifier, &DiveListNotifier::diveSiteChanged, this, &MapWidgetHelper::diveSiteChanged);
	connect(&diveListNotifier, &DiveListNotifier::divesSelected, this, &MapWidgetHelper::divesSelected);
}

QGeoCoordinate MapWidgetHelper::getCoordinates(struct dive_site *ds)
{
	if (!dive_site_has_gps_location(ds))
		return QGeoCoordinate(0.0, 0.0);
	return QGeoCoordinate(ds->location.lat.udeg * 0.000001, ds->location.lon.udeg * 0.000001);
}

void MapWidgetHelper::centerOnDiveSite(struct dive_site *ds)
{
	updateEditMode();
	QString diveKey = current_dive ? QStringLiteral("%1-%2")
		.arg(current_dive->number).arg(current_dive->when) : QString();
	if (m_currentDiveKey != diveKey) {
		m_currentDiveKey = diveKey;
		emit currentDiveChanged();
		emit diveImageOverlayChanged();
	}
	if (m_currentDs != ds) {
		m_currentDs = ds;
		emit siteImagePathChanged();
	}
	if (!dive_site_has_gps_location(ds)) {
		// dive site with no GPS
		m_mapLocationModel->setSelected(ds);
		QMetaObject::invokeMethod(m_map, "deselectMapLocation");
	} else {
		// dive site with GPS
		m_mapLocationModel->setSelected(ds);
		QGeoCoordinate dsCoord (ds->location.lat.udeg * 0.000001, ds->location.lon.udeg * 0.000001);
		QMetaObject::invokeMethod(m_map, "centerOnCoordinate", Q_ARG(QVariant, QVariant::fromValue(dsCoord)));
	}
}

void MapWidgetHelper::setCurrentDive(struct dive *dive)
{
	QString diveKey = dive ? QStringLiteral("%1-%2").arg(dive->number).arg(dive->when) : QString();
	if (m_currentDiveKey == diveKey)
		return;
	m_currentDiveKey = diveKey;
	emit currentDiveChanged();
	emit diveImageOverlayChanged();
}

void MapWidgetHelper::setSelected(const QVector<dive_site *> &divesites)
{
	m_mapLocationModel->setSelected(divesites);
	m_mapLocationModel->selectionChanged();
	updateEditMode();
}

void MapWidgetHelper::centerOnSelectedDiveSite()
{
	QVector<struct dive_site *> selDS = m_mapLocationModel->selectedDs();

	if (selDS.isEmpty()) {
		if (m_currentDs) {
			m_currentDs = nullptr;
			emit siteImagePathChanged();
		}
		// no selected dives with GPS coordinates
		QMetaObject::invokeMethod(m_map, "deselectMapLocation");
		return;
	}
	if (selDS.size() == 1) {
		if (m_currentDs != selDS[0]) {
			m_currentDs = selDS[0];
			emit siteImagePathChanged();
		}
	} else if (m_currentDs) {
		m_currentDs = nullptr;
		emit siteImagePathChanged();
	}

	// find the most top-left and bottom-right dive sites on the map coordinate system.
	qreal minLat = 0.0, minLon = 0.0, maxLat = 0.0, maxLon = 0.0;
	int count = 0;
	for(struct dive_site *dss: selDS) {
		if (!has_location(&dss->location))
			continue;
		qreal lat = dss->location.lat.udeg * 0.000001;
		qreal lon = dss->location.lon.udeg * 0.000001;
		if (++count == 1) {
			minLat = maxLat = lat;
			minLon = maxLon = lon;
			continue;
		}
		if (lat < minLat)
			minLat = lat;
		else if (lat > maxLat)
			maxLat = lat;
		if (lon < minLon)
			minLon = lon;
		else if (lon > maxLon)
			maxLon = lon;
	}

	// Pass coordinates to QML, either as a point or as a rectangle.
	// If we didn't find any coordinates, do nothing.
	if (count == 1) {
		QGeoCoordinate dsCoord (selDS[0]->location.lat.udeg * 0.000001, selDS[0]->location.lon.udeg * 0.000001);
		QMetaObject::invokeMethod(m_map, "centerOnCoordinate", Q_ARG(QVariant, QVariant::fromValue(dsCoord)));
	} else if (count > 1) {
		QGeoCoordinate coordTopLeft(minLat, minLon);
		QGeoCoordinate coordBottomRight(maxLat, maxLon);
		QGeoCoordinate coordCenter(minLat + (maxLat - minLat) * 0.5, minLon + (maxLon - minLon) * 0.5);
		QMetaObject::invokeMethod(m_map, "centerOnRectangle",
					  Q_ARG(QVariant, QVariant::fromValue(coordTopLeft)),
					  Q_ARG(QVariant, QVariant::fromValue(coordBottomRight)),
					  Q_ARG(QVariant, QVariant::fromValue(coordCenter)));
	}
}

void MapWidgetHelper::updateEditMode()
{
#ifndef SUBSURFACE_MOBILE
	// The filter being set to dive site is the signal that we are in dive site edit mode.
	// This is the case when either the dive site edit tab or the dive site list tab are active.
	bool old = m_editMode;
	m_editMode = DiveFilter::instance()->diveSiteMode();
	if (old != m_editMode)
		emit editModeChanged();
#endif
}

void MapWidgetHelper::reloadMapLocations()
{
	updateEditMode();
	m_mapLocationModel->reload(m_map);
}

void MapWidgetHelper::selectedLocationChanged(struct dive_site *ds_in)
{
	int idx;
	struct dive *dive;
	QList<int> selectedDiveIds;

	if (!ds_in)
		return;
	MapLocation *location = m_mapLocationModel->getMapLocation(ds_in);
	if (!location)
		return;
	QGeoCoordinate locationCoord = location->coordinate;

	for_each_dive (idx, dive) {
		struct dive_site *ds = get_dive_site_for_dive(dive);
		if (!dive_site_has_gps_location(ds))
			continue;
#ifndef SUBSURFACE_MOBILE
		const qreal latitude = ds->location.lat.udeg * 0.000001;
		const qreal longitude = ds->location.lon.udeg * 0.000001;
		QGeoCoordinate dsCoord(latitude, longitude);
		if (locationCoord.distanceTo(dsCoord) < m_smallCircleRadius)
			selectedDiveIds.append(idx);
	}
#else // the mobile version doesn't support multi-dive selection
		if (ds == location->divesite)
			selectedDiveIds.append(dive->id); // use id here instead of index
	}
	int last; // get latest dive chronologically
	if (!selectedDiveIds.isEmpty()) {
		 last = selectedDiveIds.last();
		 selectedDiveIds.clear();
		 selectedDiveIds.append(last);
	}
#endif
	emit selectedDivesChanged(selectedDiveIds);
}

void MapWidgetHelper::selectVisibleLocations()
{
	int idx;
	struct dive *dive;
	QList<int> selectedDiveIds;
	for_each_dive (idx, dive) {
		struct dive_site *ds = get_dive_site_for_dive(dive);
		if (!dive_site_has_gps_location(ds))
			continue;
		const qreal latitude = ds->location.lat.udeg * 0.000001;
		const qreal longitude = ds->location.lon.udeg * 0.000001;
		QGeoCoordinate dsCoord(latitude, longitude);
		QPointF point;
		QMetaObject::invokeMethod(m_map, "fromCoordinate", Q_RETURN_ARG(QPointF, point),
		                          Q_ARG(QGeoCoordinate, dsCoord));
		if (!qIsNaN(point.x()))
#ifndef SUBSURFACE_MOBILE // indices on desktop
			selectedDiveIds.append(idx);
	}
#else // use id on mobile instead of index
			selectedDiveIds.append(dive->id);
	}
	int last; // get latest dive chronologically
	if (!selectedDiveIds.isEmpty()) {
		 last = selectedDiveIds.last();
		 selectedDiveIds.clear();
		 selectedDiveIds.append(last);
	}
#endif
	emit selectedDivesChanged(selectedDiveIds);
}

/*
 * Based on a 2D Map widget circle with center "coord" and radius SMALL_CIRCLE_RADIUS_PX,
 * obtain a "small circle" with radius m_smallCircleRadius in meters:
 *     https://en.wikipedia.org/wiki/Circle_of_a_sphere
 *
 * The idea behind this circle is to be able to select multiple nearby dives, when clicking on
 * the map. This code can be in QML, but it is in C++ instead for performance reasons.
 *
 * This can be made faster with an exponential regression [a * exp(b * x)], with a pretty
 * decent R-squared, but it becomes bound to map provider zoom level mappings and the
 * SMALL_CIRCLE_RADIUS_PX value, which makes the code hard to maintain.
 */
void MapWidgetHelper::calculateSmallCircleRadius(QGeoCoordinate coord)
{
	QPointF point;
	QMetaObject::invokeMethod(m_map, "fromCoordinate", Q_RETURN_ARG(QPointF, point),
	                          Q_ARG(QGeoCoordinate, coord));
	QPointF point2(point.x() + SMALL_CIRCLE_RADIUS_PX, point.y());
	QGeoCoordinate coord2;
	QMetaObject::invokeMethod(m_map, "toCoordinate", Q_RETURN_ARG(QGeoCoordinate, coord2),
	                          Q_ARG(QPointF, point2));
	m_smallCircleRadius = coord2.distanceTo(coord);
}

static location_t mk_location(QGeoCoordinate coord)
{
	return create_location(coord.latitude(), coord.longitude());
}

void MapWidgetHelper::copyToClipboardCoordinates(QGeoCoordinate coord, bool formatTraditional)
{
	bool savep = prefs.coordinates_traditional;
	prefs.coordinates_traditional = formatTraditional;
	location_t location = mk_location(coord);
	QApplication::clipboard()->setText(printGPSCoords(&location), QClipboard::Clipboard);

	prefs.coordinates_traditional = savep;
}

void MapWidgetHelper::updateCurrentDiveSiteCoordinatesFromMap(struct dive_site *ds, QGeoCoordinate coord)
{
	MapLocation *loc = m_mapLocationModel->getMapLocation(ds);
	if (loc)
		loc->coordinate = coord;
	location_t location = mk_location(coord);
	emit coordinatesChanged(ds, location);
}

QString MapWidgetHelper::siteImagePath() const
{
	return m_currentDs && m_currentDs->imagepath ? QString::fromUtf8(m_currentDs->imagepath) : QString();
}

QString MapWidgetHelper::importSiteImage(const QString &path)
{
	if (!m_currentDs || path.isEmpty())
		return QString();

	QFileInfo source(path);
	if (!source.isFile())
		return QString();

	QDir imageDirectory(QString::fromUtf8(system_default_directory()));
	if (!imageDirectory.mkpath(QStringLiteral("site-images")) ||
	    !imageDirectory.cd(QStringLiteral("site-images")))
		return QString();

	QString extension = source.suffix().toLower();
	QString destination = imageDirectory.filePath(QStringLiteral("site-%1.%2")
			.arg(m_currentDs->uuid, 8, 16, QLatin1Char('0')).arg(extension));
	if (source.absoluteFilePath() != QFileInfo(destination).absoluteFilePath()) {
		QFile::remove(destination);
		if (!QFile::copy(source.absoluteFilePath(), destination))
			return QString();
	}

	setSiteImage(destination);
	return destination;
}

static QString siteImageViewSettingsKey(const dive_site *ds)
{
	return QStringLiteral("SiteImageView/%1/%2")
		.arg(ds->uuid, 8, 16, QLatin1Char('0'))
		.arg(qHash(QString::fromUtf8(ds->imagepath)));
}

QVariantMap MapWidgetHelper::siteImageViewState() const
{
	QVariantMap state;
	if (!m_currentDs || !m_currentDs->imagepath)
		return state;

	QSettings settings;
	QString key = siteImageViewSettingsKey(m_currentDs);
	if (!settings.contains(key + QStringLiteral("/scale")))
		return state;
	state[QStringLiteral("scale")] = settings.value(key + QStringLiteral("/scale"));
	state[QStringLiteral("x")] = settings.value(key + QStringLiteral("/x"));
	state[QStringLiteral("y")] = settings.value(key + QStringLiteral("/y"));
	return state;
}

void MapWidgetHelper::saveSiteImageViewState(qreal scale, qreal x, qreal y)
{
	if (!m_currentDs || !m_currentDs->imagepath)
		return;
	QSettings settings;
	QString key = siteImageViewSettingsKey(m_currentDs);
	settings.setValue(key + QStringLiteral("/scale"), scale);
	settings.setValue(key + QStringLiteral("/x"), x);
	settings.setValue(key + QStringLiteral("/y"), y);
}

QString MapWidgetHelper::diveImageOverlay() const
{
	if (m_currentDiveKey.isEmpty())
		return QStringLiteral("[]");
	QSettings settings;
	return settings.value(QStringLiteral("DiveImageOverlay/%1").arg(m_currentDiveKey),
			QStringLiteral("[]")).toString();
}

QString MapWidgetHelper::currentDiveKey() const
{
	return m_currentDiveKey;
}

void MapWidgetHelper::saveDiveImageOverlay(const QString &overlay)
{
	if (m_currentDiveKey.isEmpty())
		return;
	QJsonParseError error;
	QJsonDocument document = QJsonDocument::fromJson(overlay.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || !document.isArray())
		return;
	QSettings settings;
	settings.setValue(QStringLiteral("DiveImageOverlay/%1").arg(m_currentDiveKey), overlay);
}

QUrl MapWidgetHelper::siteImageUrl() const
{
	QString path = siteImagePath();
	if (path.isEmpty())
		return QUrl();

	if (QFileInfo(path).suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0) {
#ifndef SUBSURFACE_MOBILE
		if (m_renderedPdfSource != path || !QFile::exists(m_renderedPdfPath)) {
			m_renderedPdfSource.clear();
			m_renderedPdfPath.clear();
			QTemporaryDir renderDir;
			if (renderDir.isValid()) {
				QString outputPath = renderDir.path() + QStringLiteral("/site-image");
				QStringList args{ QStringLiteral("-f"), QStringLiteral("1"),
					QStringLiteral("-singlefile"), QStringLiteral("-png"),
					QStringLiteral("-scale-to"), QStringLiteral("2400"), path, outputPath };
				if (QProcess::execute(QStringLiteral("pdftoppm"), args) == 0 &&
				    QFile::exists(outputPath + QStringLiteral(".png"))) {
					// The temporary directory must outlive the QML image load.
					QString persistentPath = QDir::tempPath() + QStringLiteral("/subsurface-site-image.png");
					QFile::remove(persistentPath);
					if (QFile::copy(outputPath + QStringLiteral(".png"), persistentPath)) {
						m_renderedPdfSource = path;
						m_renderedPdfPath = persistentPath;
					}
				}
			}
		}
		if (!m_renderedPdfPath.isEmpty())
			return QUrl::fromLocalFile(m_renderedPdfPath);
#endif
		return QUrl();
	}
	return QUrl::fromLocalFile(path);
}

void MapWidgetHelper::setSiteImage(const QString &path)
{
	if (!m_currentDs)
		return;
	Command::editDiveSiteImagePath(m_currentDs, path);
}

void MapWidgetHelper::clearSiteImage()
{
	setSiteImage(QString());
}

QString MapWidgetHelper::chooseSiteImageFile()
{
#ifndef SUBSURFACE_MOBILE
	if (!m_currentDs)
		return QString();
	QString fn = QFileDialog::getOpenFileName(nullptr, tr("Select an image for this dive site"),
						  QString(), tr("Readable image files (*.png *.jpg *.jpeg *.bmp *.gif *.svg *.pdf);;All files (*)"));
	return fn.isEmpty() ? QString() : importSiteImage(fn);
#else
	return QString();
#endif
}

void MapWidgetHelper::diveSiteChanged(struct dive_site *ds, int field)
{
	if (ds == m_currentDs && field == LocationInformationModel::IMAGEPATH) {
		m_renderedPdfSource.clear();
		m_renderedPdfPath.clear();
		emit siteImagePathChanged();
	}
}

void MapWidgetHelper::divesSelected(const QVector<dive *> &, dive *currentDive, int)
{
	QString diveKey = currentDive ? QStringLiteral("%1-%2")
		.arg(currentDive->number).arg(currentDive->when) : QString();
	if (m_currentDiveKey == diveKey)
		return;
	m_currentDiveKey = diveKey;
	emit currentDiveChanged();
	emit diveImageOverlayChanged();
}

bool MapWidgetHelper::editMode() const
{
	return m_editMode;
}

QString MapWidgetHelper::pluginObject()
{
	QString lang = getUiLanguage().replace('_', '-');
	QString cacheFolder = QString(system_default_directory()).append("/googlemaps").replace("\\", "/");
	return QStringLiteral("import QtQuick 2.0;"
			      "import QtLocation 5.3;"
			      "Plugin {"
			      "    id: mapPlugin;"
			      "    name: 'googlemaps';"
			      "    PluginParameter { name: 'googlemaps.maps.language'; value: '%1' }"
			      "    PluginParameter { name: 'googlemaps.cachefolder'; value: '%2' }"
			      "    Component.onCompleted: {"
			      "        if (availableServiceProviders.indexOf(name) === -1) {"
			      "            console.warn('MapWidget.qml: cannot find a plugin named: ' + name);"
			      "        }"
			      "    }"
			      "}").arg(lang, cacheFolder);
}
