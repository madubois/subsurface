// SPDX-License-Identifier: GPL-2.0
/*
 * maintab.cpp
 *
 * "notebook" area of the main window of Subsurface
 *
 */
#include "maintab.h"

#include "TabDiveEquipment.h"
#include "TabDiveExtraInfo.h"
#include "TabDiveInformation.h"
#include "TabDiveNotes.h"
#include "TabDivePhotos.h"
#include "TabDiveStatistics.h"
#include "TabDiveSite.h"

#include "core/selection.h"
#include "desktop-widgets/simplewidgets.h" // for isGnome3Session()
#include "qt-models/diveplannermodel.h"

#include <QShortcut>

static bool paletteIsDark(const QPalette &p)
{
	// we consider a palette dark if the text color is lighter than the windows background
	return p.window().color().valueF() < p.windowText().color().valueF();
}

MainTab::MainTab(QWidget *parent) : QTabWidget(parent),
	lastSelectedDive(true),
	lastTabSelectedDive(0),
	lastTabSelectedDiveTrip(0)
{
	ui.setupUi(this);

	extraWidgets << new TabDiveNotes(this);
	ui.tabWidget->addTab(extraWidgets.last(), tr("Notes"));
	extraWidgets << new TabDiveEquipment(this);
	ui.tabWidget->addTab(extraWidgets.last(), tr("Equipment"));
	extraWidgets << new TabDiveInformation(this);
	ui.tabWidget->addTab(extraWidgets.last(), tr("Information"));
	extraWidgets << new TabDiveStatistics(this);
	ui.tabWidget->addTab(extraWidgets.last(), tr("Summary"));
	extraWidgets << new TabDivePhotos(this);
	ui.tabWidget->addTab(extraWidgets.last(), tr("Media"));
	extraWidgets << new TabDiveExtraInfo(this);
	ui.tabWidget->addTab(extraWidgets.last(), tr("Extra Info"));
	extraWidgets << new TabDiveSite(this);
	ui.tabWidget->addTab(extraWidgets.last(), tr("Dive sites"));

	// make sure we know if this is a light or dark mode
	isDark = paletteIsDark(palette());

	// call colorsChanged() for the initial setup now that the extraWidgets are loaded
	colorsChanged();

	connect(&diveListNotifier, &DiveListNotifier::settingsChanged, this, &MainTab::updateDiveInfo);

	if (qApp->style()->objectName() == "oxygen")
		setDocumentMode(true);
	else
		setDocumentMode(false);

	// Current display of things on Gnome3 looks like shit, so
	// let's fix that.
	if (isGnome3Session()) {
		// TODO: Either do this for all scroll areas or none
		//QPalette p;
		//p.setColor(QPalette::Window, QColor(Qt::white));
		//ui.scrollArea->viewport()->setPalette(p);

		// GroupBoxes in Gnome3 looks like I'v drawn them...
		static const QString gnomeCss = QStringLiteral(
			"QGroupBox {"
				"background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
				"stop: 0 #E0E0E0, stop: 1 #FFFFFF);"
				"border: 2px solid gray;"
				"border-radius: 5px;"
				"margin-top: 1ex;"
			"}"
			"QGroupBox::title {"
				"subcontrol-origin: margin;"
				"subcontrol-position: top center;"
				"padding: 0 3px;"
				"background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
				"stop: 0 #E0E0E0, stop: 1 #FFFFFF);"
			"}");
		for (QGroupBox *box: findChildren<QGroupBox *>())
			box->setStyleSheet(gnomeCss);
	}
	// QLineEdit and QLabels should have minimal margin on the left and right but not waste vertical space
	QMargins margins(3, 2, 1, 0);
	for (QLabel *label: findChildren<QLabel *>())
		label->setContentsMargins(margins);
}

void MainTab::nextInputField(QKeyEvent *event)
{
	keyPressEvent(event);
}

void MainTab::updateDiveInfo()
{
	// don't execute this while planning a dive
	if (DivePlannerPointsModel::instance()->isPlanner())
		return;

	// If there is no current dive, disable all widgets except the last two,
	// which are the dive site tab and the dive computer tabs.
	// TODO: Conceptually, these two shouldn't even be a tabs here!
	bool enabled = current_dive != nullptr;
	for (int i = 0; i < extraWidgets.size() - 2; ++i)
		extraWidgets[i]->setEnabled(enabled);

	if (current_dive) {
		for (TabBase *widget: extraWidgets)
			widget->updateData();

		// If we're on the dive-site tab, we don't want to switch tab when entering / exiting
		// trip mode. The reason is that
		// 1) this disrupts the user-experience and
		// 2) the filter is reset, potentially erasing the current trip under our feet.
		// TODO: Don't hard code tab location!
		bool onDiveSiteTab = ui.tabWidget->currentIndex() == 6;
		if (single_selected_trip()) {
			// Remember the tab selected for last dive but only if we're not on the dive site tab
			if (lastSelectedDive && !onDiveSiteTab)
				lastTabSelectedDive = ui.tabWidget->currentIndex();
			ui.tabWidget->setTabText(0, tr("Trip notes"));
			// Recover the tab selected for last dive trip but only if we're not on the dive site tab
			if (lastSelectedDive && !onDiveSiteTab)
				ui.tabWidget->setCurrentIndex(lastTabSelectedDiveTrip);
			lastSelectedDive = false;
		} else {
			// Remember the tab selected for last dive trip but only if we're not on the dive site tab
			if (!lastSelectedDive && !onDiveSiteTab)
				lastTabSelectedDiveTrip = ui.tabWidget->currentIndex();
			ui.tabWidget->setTabText(0, tr("Notes"));
			// Recover the tab selected for last dive but only if we're not on the dive site tab
			if (!lastSelectedDive && !onDiveSiteTab)
				ui.tabWidget->setCurrentIndex(lastTabSelectedDive);
			lastSelectedDive = true;
		}
	} else {
		clearTabs();
	}
}

// Remove focus from any active field to update the corresponding value in the dive.
// Do this by setting the focus to ourself
void MainTab::stealFocus()
{
	setFocus();
}

void MainTab::clearTabs()
{
	for (auto widget: extraWidgets)
		widget->clear();
}

void MainTab::changeEvent(QEvent *ev)
{
	if (ev->type() == QEvent::PaletteChange) {
		// check if this is a light or dark mode
		bool dark = paletteIsDark(palette());
		if (dark != isDark) {
			// things have changed, so setup the colors correctly
			isDark = dark;
			colorsChanged();
		}
	}
	QTabWidget::changeEvent(ev);
}

// setup the colors of 'header' elements in the tab widget
void MainTab::colorsChanged()
{
	QString colorText = isDark ? QStringLiteral("lightblue") : QStringLiteral("mediumblue");
	QString lastpart = colorText + " ;}";

	// only set the color if the widget is enabled
	QString CSSLabelcolor = "QLabel:enabled { color: " + lastpart;
	QString CSSTitlecolor = "QGroupBox::title:enabled { color: " + lastpart ;

	// apply to all the group boxes
	QList<QGroupBox *>groupBoxes = this->findChildren<QGroupBox *>();
	for (QGroupBox *gb: groupBoxes)
		gb->setStyleSheet(QString(CSSTitlecolor));

	// apply to all labels that are marked as headers in the .ui file
	QList<QLabel *>labels = this->findChildren<QLabel *>();
	for (QLabel *ql: labels) {
		if (ql->property("isHeader").toBool())
			ql->setStyleSheet(QString(CSSLabelcolor));
	}

	// finally call the individual updateUi() functions so they can overwrite these style sheets
	for (TabBase *widget: extraWidgets)
		widget->updateUi(colorText);
}
