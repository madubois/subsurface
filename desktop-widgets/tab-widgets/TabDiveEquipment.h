// SPDX-License-Identifier: GPL-2.0
#ifndef TAB_DIVE_EQUIPMENT_H
#define TAB_DIVE_EQUIPMENT_H

#include "TabBase.h"
#include "ui_TabDiveEquipment.h"
#include "qt-models/completionmodels.h"
#include "desktop-widgets/divelistview.h"
#include "desktop-widgets/modeldelegates.h"

namespace Ui {
	class TabDiveEquipment;
};

class WeightModel;
class CylindersModel;

class TabDiveEquipment : public TabBase {
	Q_OBJECT
public:
	TabDiveEquipment(MainTab *parent);
	~TabDiveEquipment();
	void updateData(const std::vector<dive *> &selection, dive *currentDive, int currentDC) override;
	void clear() override;
	void closeWarning();

private slots:
	void divesChanged(const QVector<dive *> &dives, DiveField field);
	void addCylinder_clicked();
	void toggleTriggeredColumn();
	void editCylinderWidget(const QModelIndex &index);
	void divesEdited(int count);

private:
	Ui::TabDiveEquipment ui;
	CylindersModel *cylindersModel;

	TankInfoDelegate tankInfoDelegate;
	TankUseDelegate tankUseDelegate;
	SensorDelegate sensorDelegate;
};

#endif // TAB_DIVE_EQUIPMENT_H
