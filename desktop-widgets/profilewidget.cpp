// SPDX-License-Identifier: GPL-2.0

#include "profilewidget.h"
#include "profile-widget/profilewidget2.h"
#include "commands/command.h"
#include "core/color.h"
#include "core/settings/qPrefTechnicalDetails.h"
#include "core/settings/qPrefPartialPressureGas.h"
#include "core/subsurface-string.h"
#include "qt-models/diveplannermodel.h"

#include <QToolBar>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QKeyEvent>

// A resizing display of the Subsurface logo when no dive is shown
class EmptyView : public QLabel {
public:
	EmptyView(QWidget *parent = nullptr);
	~EmptyView();
private:
	QPixmap logo;
	void update();
	void resizeEvent(QResizeEvent *) override;
};

EmptyView::EmptyView(QWidget *parent) : QLabel(parent),
	logo(":poster-icon")
{
	QPalette pal;
	pal.setColor(QPalette::Window, getColor(::BACKGROUND));
	setAutoFillBackground(true);
	setPalette(pal);
	setMinimumSize(1,1);
	setAlignment(Qt::AlignHCenter);
	update();
}

EmptyView::~EmptyView()
{
}

void EmptyView::update()
{
	setPixmap(logo.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void EmptyView::resizeEvent(QResizeEvent *)
{
	update();
}

ProfileWidget::ProfileWidget() : originalDive(nullptr)
{
	ui.setupUi(this);

	// what is a sane order for those icons? we should have the ones the user is
	// most likely to want towards the top so they are always visible
	// and the ones that someone likely sets and then never touches again towards the bottom
	toolbarActions = { ui.profCalcCeiling, ui.profCalcAllTissues, // start with various ceilings
			   ui.profIncrement3m, ui.profDcCeiling,
			   ui.profPhe, ui.profPn2, ui.profPO2, // partial pressure graphs
			   ui.profRuler, ui.profScaled, // measuring and scaling
			   ui.profTogglePicture, ui.profTankbar,
			   ui.profMod, ui.profDeco, ui.profNdl_tts, // various values that a user is either interested in or not
			   ui.profEad, ui.profSAC,
			   ui.profHR, // very few dive computers support this
			   ui.profTissues }; // maybe less frequently used

	emptyView.reset(new EmptyView);

	view.reset(new ProfileWidget2(DivePlannerPointsModel::instance(), 1.0, this));
	QToolBar *toolBar = new QToolBar(this);
	for (QAction *a: toolbarActions)
		toolBar->addAction(a);
	toolBar->setOrientation(Qt::Vertical);
	toolBar->setIconSize(QSize(24, 24));

	stack = new QStackedWidget(this);
	stack->addWidget(emptyView.get());
	stack->addWidget(view.get());

	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setSpacing(0);
	layout->setMargin(0);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(toolBar);
	layout->addWidget(stack);
	setLayout(layout);

	// Toolbar Connections related to the Profile Update
	auto tec = qPrefTechnicalDetails::instance();
	connect(ui.profCalcAllTissues, &QAction::triggered, tec, &qPrefTechnicalDetails::set_calcalltissues);
	connect(ui.profCalcCeiling,    &QAction::triggered, tec, &qPrefTechnicalDetails::set_calcceiling);
	connect(ui.profDcCeiling,      &QAction::triggered, tec, &qPrefTechnicalDetails::set_dcceiling);
	connect(ui.profEad,            &QAction::triggered, tec, &qPrefTechnicalDetails::set_ead);
	connect(ui.profIncrement3m,    &QAction::triggered, tec, &qPrefTechnicalDetails::set_calcceiling3m);
	connect(ui.profMod,            &QAction::triggered, tec, &qPrefTechnicalDetails::set_mod);
	connect(ui.profNdl_tts,        &QAction::triggered, tec, &qPrefTechnicalDetails::set_calcndltts);
	connect(ui.profDeco,           &QAction::triggered, tec, &qPrefTechnicalDetails::set_decoinfo);
	connect(ui.profHR,             &QAction::triggered, tec, &qPrefTechnicalDetails::set_hrgraph);
	connect(ui.profRuler,          &QAction::triggered, tec, &qPrefTechnicalDetails::set_rulergraph);
	connect(ui.profSAC,            &QAction::triggered, tec, &qPrefTechnicalDetails::set_show_sac);
	connect(ui.profScaled,         &QAction::triggered, tec, &qPrefTechnicalDetails::set_zoomed_plot);
	connect(ui.profTogglePicture,  &QAction::triggered, tec, &qPrefTechnicalDetails::set_show_pictures_in_profile);
	connect(ui.profTankbar,        &QAction::triggered, tec, &qPrefTechnicalDetails::set_tankbar);
	connect(ui.profTissues,        &QAction::triggered, tec, &qPrefTechnicalDetails::set_percentagegraph);

	connect(ui.profTissues,        &QAction::triggered, this, &ProfileWidget::unsetProfHR);
	connect(ui.profHR,             &QAction::triggered, this, &ProfileWidget::unsetProfTissues);

	auto pp_gas = qPrefPartialPressureGas::instance();
	connect(ui.profPhe, &QAction::triggered, pp_gas, &qPrefPartialPressureGas::set_phe);
	connect(ui.profPn2, &QAction::triggered, pp_gas, &qPrefPartialPressureGas::set_pn2);
	connect(ui.profPO2, &QAction::triggered, pp_gas, &qPrefPartialPressureGas::set_po2);

	connect(&diveListNotifier, &DiveListNotifier::settingsChanged, view.get(), &ProfileWidget2::settingsChanged);
	connect(view.get(), &ProfileWidget2::editCurrentDive, this, &ProfileWidget::editDive);
	connect(view.get(), &ProfileWidget2::stopAdded, this, &ProfileWidget::stopAdded);
	connect(view.get(), &ProfileWidget2::stopRemoved, this, &ProfileWidget::stopRemoved);
	connect(view.get(), &ProfileWidget2::stopMoved, this, &ProfileWidget::stopMoved);

	ui.profCalcAllTissues->setChecked(qPrefTechnicalDetails::calcalltissues());
	ui.profCalcCeiling->setChecked(qPrefTechnicalDetails::calcceiling());
	ui.profDcCeiling->setChecked(qPrefTechnicalDetails::dcceiling());
	ui.profEad->setChecked(qPrefTechnicalDetails::ead());
	ui.profIncrement3m->setChecked(qPrefTechnicalDetails::calcceiling3m());
	ui.profMod->setChecked(qPrefTechnicalDetails::mod());
	ui.profNdl_tts->setChecked(qPrefTechnicalDetails::calcndltts());
	ui.profDeco->setChecked(qPrefTechnicalDetails::decoinfo());
	ui.profPhe->setChecked(pp_gas->phe());
	ui.profPn2->setChecked(pp_gas->pn2());
	ui.profPO2->setChecked(pp_gas->po2());
	ui.profHR->setChecked(qPrefTechnicalDetails::hrgraph());
	ui.profRuler->setChecked(qPrefTechnicalDetails::rulergraph());
	ui.profSAC->setChecked(qPrefTechnicalDetails::show_sac());
	ui.profTogglePicture->setChecked(qPrefTechnicalDetails::show_pictures_in_profile());
	ui.profTankbar->setChecked(qPrefTechnicalDetails::tankbar());
	ui.profTissues->setChecked(qPrefTechnicalDetails::percentagegraph());
	ui.profScaled->setChecked(qPrefTechnicalDetails::zoomed_plot());
}

ProfileWidget::~ProfileWidget()
{
}

void ProfileWidget::setEnabledToolbar(bool enabled)
{
	for (QAction *b: toolbarActions)
		b->setEnabled(enabled);
}

void ProfileWidget::setDive(const struct dive *d)
{
	// If the user was currently editing a dive, exit edit mode.
	stack->setCurrentIndex(1); // show profile

	bool freeDiveMode = d->dc.divemode == FREEDIVE;
	ui.profCalcCeiling->setDisabled(freeDiveMode);
	ui.profCalcCeiling->setDisabled(freeDiveMode);
	ui.profCalcAllTissues ->setDisabled(freeDiveMode);
	ui.profIncrement3m->setDisabled(freeDiveMode);
	ui.profDcCeiling->setDisabled(freeDiveMode);
	ui.profPhe->setDisabled(freeDiveMode);
	ui.profPn2->setDisabled(freeDiveMode); //TODO is the same as scuba?
	ui.profPO2->setDisabled(freeDiveMode); //TODO is the same as scuba?
	ui.profTankbar->setDisabled(freeDiveMode);
	ui.profMod->setDisabled(freeDiveMode);
	ui.profNdl_tts->setDisabled(freeDiveMode);
	ui.profDeco->setDisabled(freeDiveMode);
	ui.profEad->setDisabled(freeDiveMode);
	ui.profSAC->setDisabled(freeDiveMode);
	ui.profTissues->setDisabled(freeDiveMode);

	ui.profRuler->setDisabled(false);
	ui.profScaled->setDisabled(false); // measuring and scaling
	ui.profTogglePicture->setDisabled(false);
	ui.profHR->setDisabled(false);
}

void ProfileWidget::escPressed()
{
	if (!editedDive)
		return;

	exitEditMode();
	plotCurrentDive();
}

void ProfileWidget::plotCurrentDive()
{
	// Exit edit mode if the dive changed
	if (editedDive && originalDive != current_dive)
		exitEditMode();

	setEnabledToolbar(current_dive != nullptr);
	if (editedDive) {
		view->plotDive(editedDive.get(), editedDc);
	} else if (current_dive) {
		setDive(current_dive);
		view->setProfileState(current_dive, dc_number);
		view->resetZoom(); // when switching dive, reset the zoomLevel
		view->plotDive(current_dive, dc_number);
	} else {
		view->clear();
		stack->setCurrentIndex(0);
	}
}

void ProfileWidget::setPlanState(const struct dive *d, int dc)
{
	exitEditMode();
	setDive(d);
	view->setPlanState(d, dc);
}

void ProfileWidget::unsetProfHR()
{
	ui.profHR->setChecked(false);
	qPrefTechnicalDetails::set_hrgraph(false);
}

void ProfileWidget::unsetProfTissues()
{
	ui.profTissues->setChecked(false);
	qPrefTechnicalDetails::set_percentagegraph(false);
}

void ProfileWidget::editDive()
{
	// We only allow editing of the profile for manually added dives
	// and when no other editing is in progress.
	if (!current_dive ||
	   (!same_string(current_dive->dc.model, "manually added dive") && current_dive->dc.samples) ||
	   (DivePlannerPointsModel::instance()->currentMode() != DivePlannerPointsModel::NOTHING) ||
	   editedDive)
		return;

	editedDive.reset(alloc_dive());
	editedDc = dc_number;
	copy_dive(current_dive, editedDive.get()); // Work on a copy of the dive
	originalDive = current_dive;
	DivePlannerPointsModel::instance()->setPlanMode(DivePlannerPointsModel::ADD);
	DivePlannerPointsModel::instance()->loadFromDive(editedDive.get());
	view->setEditState(editedDive.get(), 0);
}

void ProfileWidget::exitEditMode()
{
	if (!editedDive)
		return;
	DivePlannerPointsModel::instance()->setPlanMode(DivePlannerPointsModel::NOTHING);
	editedDive.reset();
	originalDive = nullptr;
}

// Update depths of edited dive
static void calcDepth(dive &d, int dcNr)
{
	d.maxdepth.mm = get_dive_dc(&d, dcNr)->maxdepth.mm = 0;
	fixup_dive(&d);
}

void ProfileWidget::stopAdded()
{
	if (!editedDive)
		return;
	calcDepth(*editedDive, editedDc);
	Command::editProfile(editedDive.get(), Command::EditProfileType::ADD, 0);
}

void ProfileWidget::stopRemoved(int count)
{
	if (!editedDive)
		return;
	calcDepth(*editedDive, editedDc);
	Command::editProfile(editedDive.get(), Command::EditProfileType::REMOVE, count);
}

void ProfileWidget::stopMoved(int count)
{
	if (!editedDive)
		return;
	calcDepth(*editedDive, editedDc);
	Command::editProfile(editedDive.get(), Command::EditProfileType::MOVE, count);
}
