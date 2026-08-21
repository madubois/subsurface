// SPDX-License-Identifier: GPL-2.0
#ifndef QMLMAPWIDGETHELPER_H
#define QMLMAPWIDGETHELPER_H

#include "core/units.h"
#include "core/subsurface-qt/divelistnotifier.h"
#include <QObject>
#include <QGeoCoordinate>
#include <QUrl>
#include <QVariantMap>

#if defined(Q_OS_IOS)
#include <QtPlugin>
Q_IMPORT_PLUGIN(QGeoServiceProviderFactoryGooglemaps)
#endif

#include "qt-models/maplocationmodel.h"
class MapLocation;
struct dive_site;

class MapWidgetHelper : public QObject {

	Q_OBJECT
	Q_PROPERTY(QObject *map MEMBER m_map)
	Q_PROPERTY(MapLocationModel *model MEMBER m_mapLocationModel NOTIFY modelChanged)
	Q_PROPERTY(bool editMode MEMBER m_editMode NOTIFY editModeChanged)
	Q_PROPERTY(QString pluginObject READ pluginObject NOTIFY pluginObjectChanged)
	Q_PROPERTY(QString siteImagePath READ siteImagePath NOTIFY siteImagePathChanged)
	Q_PROPERTY(QUrl siteImageUrl READ siteImageUrl NOTIFY siteImagePathChanged)
	Q_PROPERTY(QString currentDiveKey READ currentDiveKey NOTIFY currentDiveChanged)
	Q_PROPERTY(QString diveImageOverlayData READ diveImageOverlay NOTIFY diveImageOverlayChanged)

public:
	explicit MapWidgetHelper(QObject *parent = NULL);

	void centerOnSelectedDiveSite();
	Q_INVOKABLE QGeoCoordinate getCoordinates(struct dive_site *ds);
	Q_INVOKABLE void centerOnDiveSite(struct dive_site *ds);
	Q_INVOKABLE void reloadMapLocations();
	Q_INVOKABLE void copyToClipboardCoordinates(QGeoCoordinate coord, bool formatTraditional);
	Q_INVOKABLE void calculateSmallCircleRadius(QGeoCoordinate coord);
	Q_INVOKABLE void updateCurrentDiveSiteCoordinatesFromMap(struct dive_site *ds, QGeoCoordinate coord);
	Q_INVOKABLE void selectVisibleLocations();
	Q_INVOKABLE void selectedLocationChanged(struct dive_site *ds);
	Q_INVOKABLE QString chooseSiteImageFile(); // opens a file picker, sets and returns the chosen path (desktop only)
	Q_INVOKABLE QString importSiteImage(const QString &path);
	Q_INVOKABLE void clearSiteImage();
	Q_INVOKABLE QVariantMap siteImageViewState() const;
	Q_INVOKABLE void saveSiteImageViewState(qreal scale, qreal x, qreal y);
	Q_INVOKABLE QString diveImageOverlay() const;
	Q_INVOKABLE void saveDiveImageOverlay(const QString &overlay);
	void setCurrentDive(struct dive *dive);
	void setSelected(const QVector<dive_site *> &divesites);
	QString pluginObject();
	QString siteImagePath() const;
	QUrl siteImageUrl() const;
	QString currentDiveKey() const;
	bool editMode() const;

private:
	void updateEditMode();
	void setSiteImage(const QString &path);
	QObject *m_map;
	MapLocationModel *m_mapLocationModel;
	qreal m_smallCircleRadius;
	bool m_editMode;
	struct dive_site *m_currentDs;
	QString m_currentDiveKey;
	mutable QString m_renderedPdfSource;
	mutable QString m_renderedPdfPath;

private slots:
	void diveSiteChanged(struct dive_site *ds, int field);
	void divesSelected(const QVector<dive *> &dives, dive *currentDive, int currentDC);

signals:
	void modelChanged();
	void editModeChanged();
	void selectedDivesChanged(const QList<int> &list);
	void coordinatesChanged(struct dive_site *ds, const location_t &);
	void pluginObjectChanged();
	void siteImagePathChanged();
	void currentDiveChanged();
	void diveImageOverlayChanged();
};


#endif
