#include "Qt4ConstraintMacro.h"

#include <iostream>

using namespace action_authoring;

Qt4ConstraintMacro::
Qt4ConstraintMacro(ConstraintMacroPtr constraint) : _gui_name(new QLineEdit()),
				     _gui_robotJointType(new QComboBox()),
				     _gui_constraintType(new QComboBox()),
				     _gui_affordanceType(new QComboBox()),
				     _gui_panel(new TogglePanel(this, "test"))
{
    // constructor
    _constraint = constraint;
}

Qt4ConstraintMacro::
~Qt4ConstraintMacro() {
    //TODO; hack
    std::cout << "destructive destructor destructing" << std::endl;
    _gui_panel->setParent(NULL);
}

std::string
Qt4ConstraintMacro::
getSelectedLinkName() {
    return _gui_robotJointType->currentText().toStdString();
}

TogglePanel* 
Qt4ConstraintMacro::
getPanel() {
    QString waypointTitle = QString::fromStdString(_constraint->getName());
    std::cout << "title is " << _constraint->getName() << std::endl;

    QGroupBox* groupBox = new QGroupBox();
    QPushButton* editButton = new QPushButton(QString::fromUtf8("edit"));

    // update the global maps
    //_all_panels[waypoint_constraint->getName()] = outputPanel;
    //_all_robot_link_combos[waypoint_constraint->getName()] = radio1;

    _gui_constraintType->insertItem(0, "3D Offset");
    _gui_constraintType->insertItem(0, "tangent to");
    _gui_constraintType->insertItem(0, "normal to");
    _gui_constraintType->insertItem(0, "near");
    _gui_constraintType->insertItem(0, "point touches");
    _gui_constraintType->insertItem(0, "surface touches");
    _gui_constraintType->insertItem(0, "grasps");

    QVBoxLayout* vbox = new QVBoxLayout;
    vbox->addWidget(_gui_name);
    
    QHBoxLayout* hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel("robot"));
    hbox->addWidget(_gui_robotJointType);
    hbox->addWidget(new QLabel("relation"));
    hbox->addWidget(_gui_constraintType);
    hbox->addWidget(new QLabel("affordance"));
    hbox->addWidget(_gui_affordanceType);
    hbox->addWidget(editButton);
    hbox->addStretch(1);
    QWidget* boxcontainer = new QWidget();
    boxcontainer->setLayout(hbox);
    vbox->addWidget(boxcontainer);
    groupBox->setLayout(vbox);

    // MUST go before the QT connections have been made 
    updateElementsFromState();
    
    connect(_gui_name, SIGNAL(textChanged(QString)), this, SLOT(updateStateFromElements()));
    connect(_gui_robotJointType, SIGNAL(currentIndexChanged(int)), this, SLOT(updateStateFromElements()));
    connect(_gui_constraintType, SIGNAL(currentIndexChanged(int)), this, SLOT(updateStateFromElements()));
    connect(_gui_affordanceType, SIGNAL(currentIndexChanged(int)), this, SLOT(updateStateFromElements()));
    connect(editButton, SIGNAL(released()), this, SLOT(setActive()));

    _gui_panel->addWidget(groupBox);
    return _gui_panel;
}

void
Qt4ConstraintMacro::
setAffordances(std::vector<affordance::AffConstPtr> &leftSideAffordances, 
	       std::vector<affordance::AffConstPtr> &rightSideAffordances) {
    // set the private field
    _leftSideAffordances = leftSideAffordances;
    _rightSideAffordances = rightSideAffordances;
    updateElementsFromState();
}

ConstraintMacroPtr
Qt4ConstraintMacro::
getConstraintMacro() {
    return _constraint;
}

// todo; very primitive; need affordance UID type
void
Qt4ConstraintMacro::
updateStateFromElements() {
    _constraint->setName(_gui_name->text().toStdString());
    _gui_panel->setTitle(QString::fromStdString(_constraint->getName()));

    if (_gui_robotJointType->currentIndex() >= 0) 
      {
	
      _constraint->getAtomicConstraint()->setAffordance1(_leftSideAffordances[_gui_robotJointType->currentIndex()]);
      }

    if (_gui_affordanceType->currentIndex() >= 0) 
      {
	_constraint->getAtomicConstraint()->setAffordance2(_rightSideAffordances[_gui_affordanceType->currentIndex()]);
      }

    setActive();
}

void 
Qt4ConstraintMacro::
setActive() {
    emit activatedSignal();
}

void 
Qt4ConstraintMacro::
setSelected(bool selected) {
    _gui_panel->setSelected(selected);
}

void
Qt4ConstraintMacro::
updateElementsFromState() {
    _gui_name->setText(QString::fromStdString(_constraint->getName()));
    _gui_panel->setTitle(QString::fromStdString(_constraint->getName()));
    _gui_robotJointType->clear();

    // re-initialize the maps
    _affordance1IndexMap.clear();
    _affordance2IndexMap.clear();

    // update the left side combo box
    _gui_robotJointType->clear();
    for (int i = 0; i < _leftSideAffordances.size(); i++) {
        _gui_robotJointType->insertItem(i, QString::fromStdString(_leftSideAffordances[i]->getName()));
	_affordance1IndexMap[_leftSideAffordances[i]->getGlobalUniqueId()] = i;
    }

    // select the correct joint name
    std::map<affordance::GlobalUID, int>::const_iterator it = _affordance1IndexMap.find(
	_constraint->getAtomicConstraint()->getAffordance1()->getGlobalUniqueId());
    if (it!=_affordance1IndexMap.end()) {
	_gui_robotJointType->setCurrentIndex(it->second);
//	std::cout << "found LH affordance iterator: " << it->second << std::endl;
    } else {
//	std::cout << "failed to find LH affordance (guid: " 
//		  << 	_constraint->getAffordance1()->getGlobalUniqueId().first << ","
//		  << _constraint->getAffordance1()->getGlobalUniqueId().second << std::endl;
    }

    // update the right side combo box
    _gui_affordanceType->clear();
    for (int i = 0; i < _rightSideAffordances.size(); i++) {
        _gui_affordanceType->insertItem(i, QString::fromStdString(_rightSideAffordances[i]->getName()));
	_affordance2IndexMap[_rightSideAffordances[i]->getGlobalUniqueId()] = i;
//	std::cout << i << " : " << _rightSideAffordances[i]->getName() << " : " << _rightSideAffordances[i]->getGlobalUniqueId().first << ", " <<
//	    _rightSideAffordances[i]->getGlobalUniqueId().second << std::endl;
    }

    // select the current affordance
    std::map<affordance::GlobalUID, int>::const_iterator it2 = _affordance2IndexMap.find(
	_constraint->getAtomicConstraint()->getAffordance2()->getGlobalUniqueId());
    if (it2 != _affordance2IndexMap.end()) {
	_gui_affordanceType->setCurrentIndex(it2->second);
//	std::cout << "found RH affordance iterator ((" << it2->second << ")): " << " " <<
//	    _constraint->getAffordance2()->getGlobalUniqueId().first << ", "
//		  << _constraint->getAffordance2()->getGlobalUniqueId().second << std::endl;
    } else {

    }
    
}
