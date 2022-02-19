// SPDX-License-Identifier: GPL-2.0
// The profile and its toolbars.

#ifndef PROFILEWIDGET_H
#define PROFILEWIDGET_H

#include "ui_profilewidget.h"

#include <vector>
#include <memory>

struct dive;
class ProfileWidget2;
class EmptyView;
class QStackedWidget;

class ProfileWidget : public QWidget {
	Q_OBJECT
public:
	ProfileWidget();
	~ProfileWidget();
	std::unique_ptr<ProfileWidget2> view;
	void plotCurrentDive();
	void setPlanState(const struct dive *d, int dc);
	void setEnabledToolbar(bool enabled);
	void escPressed();
private
slots:
	void unsetProfHR();
	void unsetProfTissues();
	void stopAdded();
	void stopRemoved(int count);
	void stopMoved(int count);
private:
	std::unique_ptr<EmptyView> emptyView;
	std::vector<QAction *> toolbarActions;
	Ui::ProfileWidget ui;
	QStackedWidget *stack;
	void setDive(const struct dive *d);
	void editDive();
	void exitEditMode();
	std::unique_ptr<dive> editedDive;
	int editedDc;
	dive *originalDive;
};

#endif // PROFILEWIDGET_H
