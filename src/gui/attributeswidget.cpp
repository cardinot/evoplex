/**
 * Copyright (C) 2017 - Marcos Cardinot
 * @author Marcos Cardinot <mcardinot@gmail.com>
 */

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QVariant>
#include <QRadioButton>

#include "attributeswidget.h"
#include "outputwidget.h"
#include "ui_attributeswidget.h"

#define STRING_NULL_PLUGINID "--"

namespace evoplex {

AttributesWidget::AttributesWidget(MainApp* mainApp, Project* project, QWidget *parent)
    : QDockWidget(parent)
    , m_mainApp(mainApp)
    , m_project(project)
    , m_exp(nullptr)
    , m_selectedGraphId(STRING_NULL_PLUGINID)
    , m_selectedModelId(STRING_NULL_PLUGINID)
    , m_ui(new Ui_AttributesWidget)
{
    m_ui->setupUi(this);

    m_ui->treeWidget->setFocusPolicy(Qt::NoFocus);
    m_ui->bEdit->hide();
    connect(m_ui->bSubmit, SIGNAL(clicked(bool)), SLOT(slotCreateExperiment()));
    connect(m_ui->bEdit, SIGNAL(clicked(bool)), SLOT(slotEditExperiment()));

    // setup the tree widget: model
    m_treeItemModels = new QTreeWidgetItem(m_ui->treeWidget);
    m_treeItemModels->setText(0, "Model");
    m_treeItemModels->setExpanded(true);
    // --  models available
    QComboBox* cb = new QComboBox(m_ui->treeWidget);
    connect(cb, SIGNAL(currentIndexChanged(QString)), SLOT(slotModelSelected(QString)));
    addTreeWidget(m_treeItemModels, GENERAL_ATTRIBUTE_MODELID, QVariant::fromValue(cb));

    // setup the tree widget: graph
    m_treeItemGraphs = new QTreeWidgetItem(m_ui->treeWidget);
    m_treeItemGraphs->setText(0, "Graph");
    m_treeItemGraphs->setExpanded(false);
    // -- add custom widget -- agents from file
    QLineEdit* agentsPath = new QLineEdit(m_project->getDest());
    QPushButton* btBrowseFile = new QPushButton("...");
    btBrowseFile->setMaximumWidth(20);
    connect(btBrowseFile, SIGNAL(clicked(bool)), SLOT(slotAgentFile()));
    QHBoxLayout* agentsLayout = new QHBoxLayout(new QWidget(m_ui->treeWidget));
    agentsLayout->setMargin(0);
    agentsLayout->insertWidget(0, agentsPath);
    agentsLayout->insertWidget(1, btBrowseFile);
    m_widgetFields.insert(GENERAL_ATTRIBUTE_AGENTS, QVariant::fromValue(agentsPath));
    QTreeWidgetItem* item = new QTreeWidgetItem(m_treeItemGraphs);
    item->setText(0, GENERAL_ATTRIBUTE_AGENTS);
    m_ui->treeWidget->setItemWidget(item, 1, agentsLayout->parentWidget());
    // --  graphs available
    cb = new QComboBox(m_ui->treeWidget);
    connect(cb, SIGNAL(currentIndexChanged(QString)), SLOT(slotGraphSelected(QString)));
    addTreeWidget(m_treeItemGraphs, GENERAL_ATTRIBUTE_GRAPHID, QVariant::fromValue(cb));
    // --  graph type
    cb = new QComboBox(m_ui->treeWidget);
    cb->insertItem(0, "undirected", AbstractGraph::Undirected);
    cb->insertItem(1, "directed", AbstractGraph::Directed);
    m_customGraphIdx = m_treeItemGraphs->childCount();
    addTreeWidget(m_treeItemGraphs, GENERAL_ATTRIBUTE_GRAPHTYPE, QVariant::fromValue(cb));

    // setup the tree widget: general attributes
    m_treeItemGeneral = new QTreeWidgetItem(m_ui->treeWidget);
    m_treeItemGeneral->setText(0, "Simulation");
    m_treeItemGeneral->setExpanded(false);
    // -- seed
    addTreeWidget(m_treeItemGeneral, GENERAL_ATTRIBUTE_SEED, QVariant::fromValue(newSpinBox(0, INT32_MAX)));
    // --  stop at
    addTreeWidget(m_treeItemGeneral, GENERAL_ATTRIBUTE_STOPAT, QVariant::fromValue(newSpinBox(1, EVOPLEX_MAX_STEPS)));
    // --  trials
    addTreeWidget(m_treeItemGeneral, GENERAL_ATTRIBUTE_TRIALS, QVariant::fromValue(newSpinBox(1, EVOPLEX_MAX_STEPS)));
    // --  auto delete
    QCheckBox* chb = new QCheckBox(m_ui->treeWidget);
    chb->setChecked(true);
    addTreeWidget(m_treeItemGeneral, GENERAL_ATTRIBUTE_AUTODELETE, QVariant::fromValue(chb));

    // setup the tree widget: outputs
    m_treeItemOutputs = new QTreeWidgetItem(m_ui->treeWidget);
    m_treeItemOutputs->setText(0, "File Outputs");
    m_treeItemOutputs->setExpanded(false);
    // -- enabled
    m_enableOutputs = new QCheckBox("save to file");
    QTreeWidgetItem* itemEnabled = new QTreeWidgetItem(m_treeItemOutputs);
    itemEnabled->setText(0, "enable");
    m_ui->treeWidget->setItemWidget(itemEnabled, 1, m_enableOutputs);
    // -- add custom widget: output directory
    QLineEdit* outDir = new QLineEdit(m_project->getDest());
    QPushButton* outBrowseDir = new QPushButton("...");
    outBrowseDir->setMaximumWidth(20);
    connect(outBrowseDir, SIGNAL(clicked(bool)), SLOT(slotOutputDir()));
    QHBoxLayout* outLayout = new QHBoxLayout(new QWidget(m_ui->treeWidget));
    outLayout->setMargin(0);
    outLayout->insertWidget(0, outDir);
    outLayout->insertWidget(1, outBrowseDir);
    m_widgetFields.insert(OUTPUT_DIR, QVariant::fromValue(outDir));
    QTreeWidgetItem* itemDir = new QTreeWidgetItem(m_treeItemOutputs);
    itemDir->setText(0, OUTPUT_DIR);
    m_ui->treeWidget->setItemWidget(itemDir, 1, outLayout->parentWidget());
    // -- add custom widget: output directory
    QLineEdit* outHeader = new QLineEdit(m_project->getDest());
    QPushButton* outBuildHeader = new QPushButton("...");
    outBuildHeader->setMaximumWidth(20);
    connect(outBuildHeader, SIGNAL(clicked(bool)), SLOT(slotOutputWidget()));
    QHBoxLayout* headerLayout = new QHBoxLayout(new QWidget(m_ui->treeWidget));
    headerLayout->setMargin(0);
    headerLayout->insertWidget(0, outHeader);
    headerLayout->insertWidget(1, outBuildHeader);
    m_widgetFields.insert(OUTPUT_HEADER, QVariant::fromValue(outHeader));
    QTreeWidgetItem* itemHeader = new QTreeWidgetItem(m_treeItemOutputs);
    itemHeader->setText(0, OUTPUT_HEADER);
    m_ui->treeWidget->setItemWidget(itemHeader, 1, headerLayout->parentWidget());

/* TODO: make the buttons to avgTrials and saveSteps work*/
    // -- avgTrials
    QCheckBox* outAvgTrials = new QCheckBox("average trials");
    addTreeWidget(m_treeItemOutputs, OUTPUT_AVGTRIALS, QVariant::fromValue(outAvgTrials));
/*    // -- steps to save
    QRadioButton* outAllSteps = new QRadioButton("all");
    outAllSteps->setChecked(true);
    QRadioButton* outLastSteps = new QRadioButton("last");
    QSpinBox* outNumSteps = new QSpinBox();
    outNumSteps->setButtonSymbols(QSpinBox::NoButtons);
    outNumSteps->setMinimum(0);
    outNumSteps->setMaximum(EVOPLEX_MAX_STEPS);
    QHBoxLayout* outStepsLayout = new QHBoxLayout(new QWidget(m_ui->treeWidget));
    outStepsLayout->setMargin(0);
    outStepsLayout->insertWidget(0, outAllSteps);
    outStepsLayout->insertWidget(1, outLastSteps);
    outStepsLayout->insertWidget(2, outNumSteps);
    outStepsLayout->insertSpacerItem(3, new QSpacerItem(5,5,QSizePolicy::MinimumExpanding));
    QTreeWidgetItem* itemOut = new QTreeWidgetItem(m_treeItemOutputs);
    itemOut->setText(0, OUTPUT_SAVESTEPS);
    m_ui->treeWidget->setItemWidget(itemOut, 1, outStepsLayout->parentWidget());
*/
    connect(m_enableOutputs, &QCheckBox::toggled,
            [outDir, outBrowseDir, outHeader, outBuildHeader](bool b) {
                outDir->setEnabled(b);
                outBrowseDir->setEnabled(b);
                outHeader->setEnabled(b);
                outBuildHeader->setEnabled(b);
//                outAvgTrials->setEnabled(b);
    });
    m_enableOutputs->setChecked(true);
    m_enableOutputs->setChecked(false);

    slotPluginsUpdated(AbstractPlugin::GraphPlugin);
    slotPluginsUpdated(AbstractPlugin::ModelPlugin);
}

AttributesWidget::~AttributesWidget()
{
    delete m_ui;
}

void AttributesWidget::setExperiment(Experiment* exp)
{
    if (!exp) {
        m_exp = nullptr;
        m_ui->bEdit->hide();
        return;
    }

    m_exp = exp;
    m_ui->bEdit->show();

    std::vector<QString> header = exp->generalAttrs()->names();
    foreach (QString attrName, exp->graphAttrs()->names())
        header.emplace_back(exp->graphId() + "_" + attrName);
    foreach (QString attrName, exp->modelAttrs()->names())
        header.emplace_back(exp->modelId() + "_" + attrName);

    std::vector<Value> values = exp->generalAttrs()->values();
    values.insert(values.end(), exp->graphAttrs()->values().begin(), exp->graphAttrs()->values().end());
    values.insert(values.end(), exp->modelAttrs()->values().begin(), exp->modelAttrs()->values().end());

    // ensure graphId will be filled at the end
    header.emplace_back(GENERAL_ATTRIBUTE_GRAPHID);
    values.emplace_back(Value(exp->graphId()));

    header.shrink_to_fit();
    values.shrink_to_fit();

    for (int i = 0; i < header.size(); ++i) {
        QWidget* widget = m_widgetFields.value(header.at(i)).value<QWidget*>();
        Q_ASSERT(widget);

        QSpinBox* sp = qobject_cast<QSpinBox*>(widget);
        if (sp) {
            sp->setValue(values.at(i).toInt);
            continue;
        }
        QDoubleSpinBox* dsp = qobject_cast<QDoubleSpinBox*>(widget);
        if (dsp) {
            dsp->setValue(values.at(i).toDouble);
            continue;
        }
        QComboBox* cb = qobject_cast<QComboBox*>(widget);
        if (cb) {
            cb->setCurrentIndex(cb->findText(values.at(i).toQString()));
            continue;
        }
        QCheckBox* chb = qobject_cast<QCheckBox*>(widget);
        if (chb) {
            chb->setChecked(values.at(i).toBool);
            continue;
        }
        QLineEdit* le = qobject_cast<QLineEdit*>(widget);
        if (le) {
            le->setText(values.at(i).toQString());
            continue;
        }
        qFatal("[AttributesWidget]: unable to know the widget type.");
    }
}

void AttributesWidget::slotAgentFile()
{
    QLineEdit* lineedit = m_widgetFields.value(GENERAL_ATTRIBUTE_AGENTS).value<QLineEdit*>();
    QString path = QFileDialog::getOpenFileName(this, "Initial Population", lineedit->text(), "Text Files (*.csv *.txt)");
    if (!path.isEmpty()) {
        lineedit->setText(path);
    }
}

void AttributesWidget::slotOutputDir()
{
    QLineEdit* lineedit = m_widgetFields.value(OUTPUT_DIR).value<QLineEdit*>();
    QString path = QFileDialog::getExistingDirectory(this, "Output Directory", lineedit->text());
    if (!path.isEmpty()) {
        lineedit->setText(path);
    }
}

void AttributesWidget::slotOutputWidget()
{
    if (m_selectedModelId == STRING_NULL_PLUGINID) {
        QMessageBox::warning(this, "Experiment", "Please, select a valid 'modelId' first.");
        return;
    }

    int numTrials = m_widgetFields.value(GENERAL_ATTRIBUTE_TRIALS).value<QSpinBox*>()->value();
    std::vector<int> trialIds;
    trialIds.reserve(numTrials);
    for (int id = 0; id < numTrials; ++id) {
        trialIds.emplace_back(id);
    }

    std::vector<Output*> currentOutputs;
    const ModelPlugin* model = m_project->getModels().value(m_selectedModelId);
    QString currentHeader = m_widgetFields.value(OUTPUT_HEADER).value<QLineEdit*>()->text();
    if (!currentHeader.isEmpty()) {
        QString errorMsg;
        currentOutputs = Output::parseHeader(currentHeader.split(";"), trialIds, model, errorMsg);
        if (!errorMsg.isEmpty()) {
            QMessageBox::warning(this, "Output Creator", errorMsg);
            qDeleteAll(currentOutputs);
            currentOutputs.clear();
        }
    }

    OutputWidget* ow = new OutputWidget(model);
    ow->setAttribute(Qt::WA_DeleteOnClose, true);
    ow->setWindowModality(Qt::ApplicationModal);
    ow->setTrialIds(trialIds);
    ow->fill(currentOutputs);
    ow->show();

    connect(ow, &OutputWidget::closed,
    [this](std::vector<Output*> outputs) {
        // join all Output objects which have the same function, entity and attribute
        QStringList uniqueOutputs;
        for (int o1 = 0; o1 < outputs.size(); ++o1) {
            if (!outputs.at(o1)) {
                continue;
            }
            QString outputStr = outputs.at(o1)->printableHeader(';');
            for (int o2 = 0; o2 < outputs.size(); ++o2) {
                if (o1 == o2 || !outputs.at(o2) || !outputs.at(o1)->operator ==(outputs.at(o2))) {
                    continue;
                }
                outputStr += "_" + outputs.at(o2)->allInputs().front().toQString();
                delete outputs.at(o2);
                outputs.at(o2) = nullptr;
            }
            uniqueOutputs.push_back(outputStr);
        }

        m_widgetFields.value(OUTPUT_HEADER)
                .value<QLineEdit*>()->setText(uniqueOutputs.join(';'));
    });
}

Experiment::ExperimentInputs* AttributesWidget::readInputs()
{
    if (m_selectedModelId == STRING_NULL_PLUGINID) {
        QMessageBox::warning(this, "Experiment", "Please, select a valid 'modelId'.");
        return nullptr;
    } else if (m_selectedGraphId == STRING_NULL_PLUGINID) {
        QMessageBox::warning(this, "Experiment", "Please, select a valid 'graphId'.");
        return nullptr;
    } else if (m_enableOutputs->isChecked()
               && (m_widgetFields.value(OUTPUT_DIR).value<QLineEdit*>()->text().isEmpty()
                   || m_widgetFields.value(OUTPUT_HEADER).value<QLineEdit*>()->text().isEmpty())) {
        QMessageBox::warning(this, "Experiment", "Please, insert a valid output directory and a output header.");
        return nullptr;
    }

    QStringList header;
    QStringList values;
    QVariantHash::iterator it;
    for (it = m_widgetFields.begin(); it != m_widgetFields.end(); ++it) {
        QWidget* widget = it.value().value<QWidget*>();
        Q_ASSERT(widget);
        if (!widget->isEnabled()) {
            continue;
        }

        header << it.key();

        QSpinBox* sp = qobject_cast<QSpinBox*>(widget);
        if (sp) {
            values << QString::number(sp->value());
            continue;
        }
        QDoubleSpinBox* dsp = qobject_cast<QDoubleSpinBox*>(widget);
        if (dsp) {
            values << QString::number(dsp->value(), 'f', 2);
            continue;
        }
        QComboBox* cb = qobject_cast<QComboBox*>(widget);
        if (cb) {
            values << cb->currentText();
            continue;
        }
        QCheckBox* chb = qobject_cast<QCheckBox*>(widget);
        if (chb) {
            values << QString::number(chb->isChecked());
            continue;
        }
        QLineEdit* le = qobject_cast<QLineEdit*>(widget);
        if (le) {
            values << le->text();
            continue;
        }

        qFatal("[AttributesWidget]: unable to know the widget type.");
    }

    if (!m_enableOutputs->isChecked()) {
        header << OUTPUT_DIR << OUTPUT_HEADER;
        values << "" << "";
    }

    QString errorMsg;
    Experiment::ExperimentInputs* inputs = Experiment::readInputs(m_mainApp, header, values, errorMsg);
    if (!inputs) {
        QMessageBox::warning(this, "Experiment",
                "Unable to create the experiment.\nError: \"" + errorMsg + "\"");
    }
    return inputs;
}

void AttributesWidget::slotCreateExperiment()
{
    m_exp = m_project->newExperiment(readInputs());
}

void AttributesWidget::slotEditExperiment()
{
    Q_ASSERT(m_exp);
    if (!m_project->editExperiment(m_exp->id(), readInputs())) {
        QMessageBox::warning(this, "Experiment",
                "Unable to edit the experiment.\n"
                "If it is running, you should pause it first.");
    }
}

void AttributesWidget::slotGraphSelected(const QString& graphId)
{
    m_selectedGraphId = graphId;
    pluginSelected(m_treeItemGraphs, graphId);

    bool validGraph = graphId != STRING_NULL_PLUGINID;
    m_treeItemGraphs->child(m_customGraphIdx)->setHidden(!validGraph || graphId == "customGraph");
    m_treeItemGeneral->setExpanded(validGraph);
    m_treeItemOutputs->setExpanded(validGraph);
}

void AttributesWidget::slotModelSelected(const QString& modelId)
{
    m_selectedModelId = modelId;
    pluginSelected(m_treeItemModels, modelId);

    bool nullModel = modelId == STRING_NULL_PLUGINID;
    m_treeItemGeneral->setHidden(nullModel);
    m_treeItemOutputs->setHidden(nullModel);
    m_treeItemGraphs->setHidden(nullModel);
    m_treeItemGraphs->setExpanded(!nullModel);
}

void AttributesWidget::pluginSelected(QTreeWidgetItem* itemRoot, const QString& pluginId)
{
    for (int i = 0; i < itemRoot->childCount(); ++i) {
        QTreeWidgetItem* row = itemRoot->child(i);
        QString pId = row->data(0, Qt::UserRole).toString();
        bool hide = !pId.isEmpty() && pId != pluginId;
        row->setHidden(hide);
        m_ui->treeWidget->itemWidget(row, 1)->setDisabled(hide);
    }
}

void AttributesWidget::slotPluginsUpdated(AbstractPlugin::PluginType type)
{
    QTreeWidgetItem* tree;
    QString treeId;
    QStringList keys;
    if (type == AbstractPlugin::GraphPlugin) {
        tree = m_treeItemGraphs;
        treeId = GENERAL_ATTRIBUTE_GRAPHID;
        keys = m_project->getGraphs().keys();
    } else if (type == AbstractPlugin::ModelPlugin) {
        tree = m_treeItemModels;
        treeId = GENERAL_ATTRIBUTE_MODELID;
        keys = m_project->getModels().keys();
    } else {
        qFatal("[AttributesWidget]: invalid plugin type!");
    }

    QComboBox* cb = m_widgetFields.value(treeId).value<QComboBox*>();
    cb->blockSignals(true);
    cb->clear();
    cb->insertItem(0, STRING_NULL_PLUGINID);
    cb->insertItems(1, keys);
    cb->blockSignals(false);

    auto addAttrs = [this, tree](AbstractPlugin* plugin) {
        if (plugin->pluginAttrNames().size() <= 0) {
            return; // nothing to add
        }

        QTreeWidgetItemIterator it(m_ui->treeWidget);
        while (*it) {
            if ((*it)->parent() == tree
                    && (*it)->data(0, Qt::UserRole).toString() == plugin->id()) {
                return; // plugins already exists
            }
            ++it;
        }

        insertPluginAttributes(tree, plugin->id(), plugin->pluginAttrSpace());
    };

    if (type == AbstractPlugin::GraphPlugin) {
        foreach (AbstractPlugin* plugin, m_project->getGraphs()) {
            addAttrs(plugin);
        }
        slotGraphSelected(cb->currentText());
    } else {
        foreach (AbstractPlugin* plugin, m_project->getModels()) {
            addAttrs(plugin);
        }
        slotModelSelected(cb->currentText());
    }
}

void AttributesWidget::insertPluginAttributes(QTreeWidgetItem* itemRoot,
                                              const QString& uid,
                                              const AttributesSpace& attrsSpace)
{
    const QString& uid_ = uid + "_";
    foreach (const ValueSpace* valSpace, attrsSpace) {
        QTreeWidgetItem* item = new QTreeWidgetItem(itemRoot);
        item->setText(0, valSpace->attrName());
        item->setData(0, Qt::UserRole, uid);

        QWidget* widget = nullptr;
        if (valSpace->type() == ValueSpace::Double_Interval) {
            const IntervalSpace* iSpace = dynamic_cast<const IntervalSpace*>(valSpace);
            widget = newDoubleSpinBox(iSpace->min().toDouble, iSpace->max().toDouble);
        } else if (valSpace->type() == ValueSpace::Int_Interval) {
            const IntervalSpace* iSpace = dynamic_cast<const IntervalSpace*>(valSpace);
            widget = newSpinBox(iSpace->min().toInt, iSpace->max().toInt);
        } else {
            QLineEdit* le = new QLineEdit();
            le->setText(valSpace->validValue().toQString());
            widget = le;
        }
        m_ui->treeWidget->setItemWidget(item, 1, widget);
        // add the uid as prefix to avoid clashes.
        m_widgetFields.insert(uid_ + valSpace->attrName(), QVariant::fromValue(widget));
    }
}

QSpinBox* AttributesWidget::newSpinBox(const int min, const int max)
{
    QSpinBox* sp = new QSpinBox(m_ui->treeWidget);
    sp->setMaximum(max);
    sp->setMinimum(min);
    sp->setButtonSymbols(QSpinBox::NoButtons);
    return sp;
}

QDoubleSpinBox* AttributesWidget::newDoubleSpinBox(const double min, const double max)
{
    QDoubleSpinBox* sp = new QDoubleSpinBox(m_ui->treeWidget);
    sp->setMaximum(max);
    sp->setMinimum(min);
    sp->setButtonSymbols(QDoubleSpinBox::NoButtons);
    return sp;
}

void AttributesWidget::addTreeWidget(QTreeWidgetItem* itemRoot, const QString& label, const QVariant& widget)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(itemRoot);
    m_widgetFields.insert(label, widget);
    item->setText(0, label);
    m_ui->treeWidget->setItemWidget(item, 1, widget.value<QWidget*>());
}

}
