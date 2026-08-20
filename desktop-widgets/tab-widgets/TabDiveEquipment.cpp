// SPDX-License-Identifier: GPL-2.0
#include "TabDiveEquipment.h"
#include "maintab.h"
#include "desktop-widgets/modeldelegates.h"
#include "core/dive.h"
#include "core/selection.h"
#include "commands/command.h"

#include "qt-models/cylindermodel.h"

#include <QMessageBox>
#include <QSettings>
#include <QCompleter>

static bool ignoreHiddenFlag(int i)
{
	return i == CylindersModel::REMOVE || i == CylindersModel::TYPE ||
	       i == CylindersModel::WORKINGPRESS_INT || i == CylindersModel::SIZE_INT;
}

static bool hiddenByDefault(int i)
{
	switch (i) {
	case CylindersModel::SENSORS:
		return true;
	}
	return false;
}

TabDiveEquipment::TabDiveEquipment(MainTab *parent) : TabBase(parent),
	cylindersModel(new CylindersModel(false, this))
{
	ui.setupUi(this);

	// This makes sure we only delete the models
	// after the destructor of the tables,
	// this is needed to save the column sizes.
	cylindersModel->setParent(ui.cylinders);

	ui.cylinders->setModel(cylindersModel);

	connect(&diveListNotifier, &DiveListNotifier::divesChanged, this, &TabDiveEquipment::divesChanged);
	connect(ui.cylinders, &TableView::itemClicked, this, &TabDiveEquipment::editCylinderWidget);
	connect(cylindersModel, &CylindersModel::divesEdited, this, &TabDiveEquipment::divesEdited);

	ui.cylinders->view()->setItemDelegateForColumn(CylindersModel::TYPE, &tankInfoDelegate);
	ui.cylinders->view()->setItemDelegateForColumn(CylindersModel::USE, &tankUseDelegate);
	ui.cylinders->view()->setItemDelegateForColumn(CylindersModel::SENSORS, &sensorDelegate);
	ui.cylinders->view()->setColumnHidden(CylindersModel::DEPTH, true);
	ui.cylinders->view()->setColumnHidden(CylindersModel::WORKINGPRESS_INT, true);
	ui.cylinders->view()->setColumnHidden(CylindersModel::SIZE_INT, true);

	ui.cylinders->setTitle(tr("Cylinders"));
	ui.cylinders->setBtnToolTip(tr("Add cylinder"));
	connect(ui.cylinders, &TableView::addButtonClicked, this, &TabDiveEquipment::addCylinder_clicked);

	QAction *action = new QAction(tr("OK"), this);
	connect(action, &QAction::triggered, this, &TabDiveEquipment::closeWarning);
	ui.multiDiveWarningMessage->addAction(action);

	action = new QAction(tr("Undo"), this);
	connect(action, &QAction::triggered, Command::undoAction(this), &QAction::trigger);
	connect(action, &QAction::triggered, this, &TabDiveEquipment::closeWarning);
	ui.multiDiveWarningMessage->addAction(action);

	ui.multiDiveWarningMessage->hide();

	QSettings s;
	s.beginGroup("cylinders_dialog");
	for (int i = 0; i < CylindersModel::COLUMNS; i++) {
		if (ignoreHiddenFlag(i))
			continue;
		auto setting = s.value(QString("column%1_hidden").arg(i));
		bool checked = setting.isValid() ? setting.toBool() : hiddenByDefault(i) ;
		QAction *action = new QAction(cylindersModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), ui.cylinders->view());
		action->setCheckable(true);
		action->setData(i);
		action->setChecked(!checked);
		connect(action, &QAction::triggered, this, &TabDiveEquipment::toggleTriggeredColumn);
		ui.cylinders->view()->setColumnHidden(i, checked);
		ui.cylinders->view()->horizontalHeader()->addAction(action);
	}
	ui.cylinders->view()->horizontalHeader()->setContextMenuPolicy(Qt::ActionsContextMenu);
}

TabDiveEquipment::~TabDiveEquipment()
{
	QSettings s;
	s.beginGroup("cylinders_dialog");
	for (int i = 0; i < CylindersModel::COLUMNS; i++) {
		if (ignoreHiddenFlag(i))
			continue;
		s.setValue(QString("column%1_hidden").arg(i), ui.cylinders->view()->isColumnHidden(i));
	}
}

// This function gets called if a field gets updated by an undo command.
// Refresh the corresponding UI field.
void TabDiveEquipment::divesChanged(const QVector<dive *> &dives, DiveField field)
{
	if (!parent.includesCurrentDive(dives))
		return;

}

void TabDiveEquipment::toggleTriggeredColumn()
{
	QAction *action = qobject_cast<QAction *>(sender());
	int col = action->data().toInt();
	QTableView *view = ui.cylinders->view();

	if (action->isChecked()) {
		view->showColumn(col);
		if (view->columnWidth(col) <= 15)
			view->setColumnWidth(col, 80);
	} else {
		view->hideColumn(col);
	}
}

void TabDiveEquipment::updateData(const std::vector<dive *> &, dive *currentDive, int currentDC)
{
	divecomputer *dc = get_dive_dc(currentDive, currentDC);

	cylindersModel->updateDive(currentDive, currentDC);
	sensorDelegate.setCurrentDC(dc);
	tankUseDelegate.setCurrentDC(dc);
}

void TabDiveEquipment::clear()
{
	cylindersModel->clear();
}

void TabDiveEquipment::addCylinder_clicked()
{
	divesEdited(Command::addCylinder(false));
}

void TabDiveEquipment::editCylinderWidget(const QModelIndex &index)
{
	if (!index.isValid())
		return;

	if (index.column() == CylindersModel::REMOVE) {
		for (dive *d: getDiveSelection()) {
			if (cylinder_with_sensor_sample(d, index.row())) {
				if (QMessageBox::warning(this, tr("Remove cylinder?"),
							 tr("The deleted cylinder has sensor readings, which will be lost.\n"
							    "Do you want to continue?"),
							 QMessageBox::Yes|QMessageBox::No) != QMessageBox::Yes)
					return;
			}
		}
		divesEdited(Command::removeCylinder(index.row(), false));
	} else {
		ui.cylinders->edit(index);
	}
}

void TabDiveEquipment::divesEdited(int i)
{
	// No warning if only one dive was edited
	if (i <= 1)
		return;
	ui.multiDiveWarningMessage->setCloseButtonVisible(false);
	ui.multiDiveWarningMessage->setText(tr("Warning: edited %1 dives").arg(i));
	ui.multiDiveWarningMessage->show();
}

void TabDiveEquipment::closeWarning()
{
	ui.multiDiveWarningMessage->hide();
}
