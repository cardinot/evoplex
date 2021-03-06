/**
 *  This file is part of Evoplex.
 *
 *  Evoplex is a multi-agent system for networks.
 *  Copyright (C) 2016 - Marcos Cardinot <marcos@cardinot.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>

#include "experiment.h"
#include "attrsgenerator.h"
#include "node.h"
#include "project.h"

namespace evoplex
{

Experiment::Experiment(MainApp* mainApp, ExpInputs* inputs, ProjectPtr project)
    : m_mainApp(mainApp)
    , m_id(inputs->general(GENERAL_ATTRIBUTE_EXPID).toInt())
    , m_project(project)
    , m_inputs(nullptr)
    , m_expStatus(INVALID)
{
    QString error;
    init(inputs, error);
    Q_ASSERT_X(m_project, "Experiment", "tried to create an experiment from a null project");
}

Experiment::~Experiment()
{
    Q_ASSERT_X(m_expStatus != RUNNING && m_expStatus != QUEUED,
               "~Experiment", "tried to delete a running experiment");
    deleteTrials();
    m_outputs.clear();
    delete m_inputs;
    m_project.clear();
}

bool Experiment::init(ExpInputs* inputs, QString& error)
{
    if (m_expStatus == RUNNING || m_expStatus == QUEUED) {
        error = "Tried to initialize a running experiment.\n"
                "Please, pause it and try again.";
        qWarning() << error;
        return false;
    }

    m_outputs.clear();
    delete m_inputs;
    m_inputs = inputs;

    m_filePathPrefix.clear();
    m_fileHeader.clear();
    if (!m_inputs->fileCaches().empty()) {
        m_filePathPrefix = QString("%1/%2_e%3_t")
                .arg(m_inputs->general(OUTPUT_DIR).toQString())
                .arg(m_project->name())
                .arg(m_id);

        for (const Cache* cache : m_inputs->fileCaches()) {
            Q_ASSERT_X(cache->inputs().size() > 0, "Experiment::init", "a file cache must have inputs");
            m_fileHeader += cache->printableHeader(',', false) + ",";
            m_outputs.insert(cache->output());
        }
        m_fileHeader.chop(1);
        m_fileHeader += "\n";
    }

    m_numTrials = m_inputs->general(GENERAL_ATTRIBUTE_TRIALS).toInt();
    m_autoDeleteTrials = m_inputs->general(GENERAL_ATTRIBUTE_AUTODELETE).toBool();

    m_graphPlugin = m_mainApp->graph(m_inputs->general(GENERAL_ATTRIBUTE_GRAPHID).toQString());
    m_modelPlugin = m_mainApp->model(m_inputs->general(GENERAL_ATTRIBUTE_MODELID).toQString());

    reset();

    return true;
}

void Experiment::reset()
{
    if (m_expStatus == RUNNING || m_expStatus == QUEUED) {
        qWarning() << "tried to reset a running experiment. You should pause it first.";
        return;
    }

    deleteTrials();

    QMutexLocker locker(&m_mutex);

    for (OutputPtr o : m_outputs) {
        o->flushAll();
    }

    m_trials.reserve(m_numTrials);
    m_delay = m_mainApp->defaultStepDelay();
    m_stopAt = m_inputs->general(GENERAL_ATTRIBUTE_STOPAT).toInt();
    m_pauseAt = m_stopAt;
    m_progress = 0;

    m_expStatus = READY;
    emit (statusChanged(m_expStatus));

    emit (restarted());
}

void Experiment::deleteTrials()
{
    QMutexLocker locker(&m_mutex);

    for (auto& trial : m_trials) {
        delete trial.second;
    }
    m_trials.clear();

    m_clonableNodes.clear();
}

void Experiment::updateProgressValue()
{
    quint16 lastProgress = m_progress;
    if (m_expStatus == FINISHED) {
        m_progress = 360;
    } else if (m_expStatus == INVALID) {
        m_progress = 0;
    } else if (m_expStatus == RUNNING) {
        float p = 0.f;
        for (auto& trial : m_trials) {
            p += static_cast<float>(trial.second->m_currStep / m_pauseAt);
        }
        m_progress = static_cast<quint16>(std::ceil(p * 360.f / m_numTrials));
    }

    if (lastProgress != m_progress) {
        emit (progressUpdated());
    }
}

void Experiment::toggle()
{
    if (m_expStatus == RUNNING) {
        pause();
    } else if (m_expStatus == READY) {
        play();
    } else if (m_expStatus == QUEUED) {
        m_mainApp->expMgr()->removeFromQueue(this);
    }
}

void Experiment::playNext()
{
    if (m_expStatus != READY) {
        return;
    } else if (m_trials.empty()) {
        setPauseAt(-1); // just create and set the trials
    } else {
        int maxCurrStep = 0;
        for (const auto& trial : m_trials) {
            int currStep = trial.second->m_currStep;
            if (currStep > maxCurrStep) maxCurrStep = currStep;
        }
        setPauseAt(maxCurrStep + 1);
    }
    m_mainApp->expMgr()->play(this);
}

void Experiment::processTrial(const quint16 trialId)
{
    if (m_expStatus == INVALID) {
        return;
    } else if (m_trials.find(trialId) == m_trials.end()) {
        AbstractModel* trial = createTrial(trialId);
        if (!trial) {
            setExpStatus(INVALID);
            pause();
            return;
        }
        m_trials.insert({trialId, trial});
        emit (trialCreated(trialId));
    }

    AbstractModel* trial = m_trials.at(trialId);
    if (trial->m_status != READY) {
        return;
    }

    trial->m_status = RUNNING;

    QElapsedTimer t;
    t.start();

    bool algorithmConverged = false;
    while (trial->m_currStep < m_pauseAt && !algorithmConverged) {
        algorithmConverged = trial->algorithmStep();
        ++trial->m_currStep;

        for (const OutputPtr& output : m_outputs)
            output->doOperation(trialId, trial);

        if (m_inputs->fileCaches().size()
                && trial->m_currStep % m_mainApp->stepsToFlush() == 0
                && !writeCachedSteps(trialId)) {
            trial->m_status = INVALID;
            setExpStatus(INVALID);
            pause();
            return;
        }

        if (m_delay > 0)
            QThread::msleep(m_delay);
    }

    qDebug() << QString("%1 (E%2:T%3) - %4s")
                .arg(m_project->name()).arg(m_id).arg(trialId)
                .arg(t.elapsed() / 1000);

    if (trial->m_currStep >= m_stopAt || algorithmConverged) {
        if (writeCachedSteps(trialId)) {
            trial->m_status = FINISHED;
        } else {
            trial->m_status = INVALID;
            setExpStatus(INVALID);
            pause();
        }
    } else {
        trial->m_status = READY;
    }
}

AbstractModel* Experiment::createTrial(const quint16 trialId)
{
    // Make it thread-safe to make sure that we create one trial at a time.
    // Thus, if one trial fail, then the other will be aborted earlier.
    QMutexLocker locker(&m_mutex);

    if (m_expStatus == INVALID || m_pauseAt == 0) {
        return nullptr;
    } if (static_cast<int>(m_trials.size()) == m_numTrials) {
        QString e = QString("FATAL! all the trials for this experiment have already been created."
                            "It should never happen!\n Project: %1; Exp: %2; Trial: %3 (max=%4)\n")
                            .arg(m_project->name()).arg(m_id).arg(trialId).arg(m_numTrials);
        qFatal("%s", qPrintable(e));
    }

    const QString& gType = m_inputs->general(GENERAL_ATTRIBUTE_GRAPHTYPE).toString();
    Nodes nodes = createNodes(AbstractGraph::enumFromString(gType));
    if (nodes.empty()) {
        return nullptr;
    }

    const quint16 seed = static_cast<quint16>(m_inputs->general(GENERAL_ATTRIBUTE_SEED).toInt());
    PRG* prg = new PRG(seed + trialId);

    AbstractGraph* graphObj = m_graphPlugin->create();
    if (!graphObj || !graphObj->setup(prg, m_inputs->graph(), nodes, gType) || !graphObj->init()) {
        qWarning() << "unable to create the trials."
                   << "The graph could not be initialized."
                   << "Project:" << m_project->name() << "Experiment:" << m_id;
        delete graphObj;
        delete prg;
        return nullptr;
    }
    graphObj->reset();

    AbstractModel* modelObj = m_modelPlugin->create();
    if (!modelObj || !modelObj->setup(prg, m_inputs->model(), graphObj) || !modelObj->init()) {
        qWarning() << "unable to create the trials."
                   << "The model could not be initialized."
                   << "Project:" << m_project->name() << "Experiment:" << m_id;
        delete modelObj;
        return nullptr;
    }

    if (!m_inputs->fileCaches().empty()) {
        const QString fpath = m_filePathPrefix + QString("%4.csv").arg(trialId);
        QFile file(fpath);
        if (file.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream stream(&file);
            stream << m_fileHeader;
            file.close();
        } else {
            qWarning() << "unable to create the trials. Could not write in " << fpath;
            delete modelObj;
            return nullptr;
        }

        // write this initial step to file
        for (OutputPtr output : m_outputs) {
            output->doOperation(trialId, modelObj);
        }
        writeCachedSteps(trialId);
    }

    modelObj->m_status = READY;
    return modelObj;
}

Nodes Experiment::createNodes(const AbstractGraph::GraphType gType)
{
    if (m_expStatus == INVALID || gType == AbstractGraph::Invalid_Type) {
        return Nodes();
    } else if (!m_clonableNodes.empty()) {
        if (static_cast<int>(m_trials.size()) == m_numTrials - 1) {
            Nodes nodes = m_clonableNodes;
            Nodes().swap(m_clonableNodes);
            return nodes;
        }
        return Utils::clone(m_clonableNodes);
    }

    Q_ASSERT_X(m_trials.empty(), "Experiment::createNodes",
               "if there is no trials to run, why is it trying to create nodes?");

    QString errMsg;
    const QString& cmd = m_inputs->general(GENERAL_ATTRIBUTE_NODES).toQString();
    Nodes nodes = Nodes::fromCmd(cmd, m_modelPlugin->nodeAttrsScope(), gType, &errMsg);
    if (!errMsg.isEmpty() || nodes.empty()) {
        errMsg = QString("unable to create the trials."
                         "The set of nodes could not be created.\n %1 \n"
                         "Project: %2 Experiment: %3")
                         .arg(errMsg).arg(m_project->name()).arg(m_id);
        qWarning() << errMsg;
        return Nodes();
    }

    if (m_numTrials > 1) {
        m_clonableNodes = Utils::clone(nodes);
    }
    return nodes;
}

AbstractModel* Experiment::trial(int trialId) const
{
    auto it = m_trials.find(trialId);
    if (it == m_trials.end() || !it->second)
        return nullptr;
    return it->second;
}

bool Experiment::writeCachedSteps(const int trialId)
{
    if (m_inputs->fileCaches().empty() || m_inputs->fileCaches().front()->isEmpty(trialId)) {
        return true;
    }

    const QString fpath = m_filePathPrefix + QString("%1.csv").arg(trialId);
    QFile file(fpath);
    if (!file.open(QFile::WriteOnly | QFile::Append)) {
        qWarning() << "unable to create the trials. Could not write in " << fpath;
        return false;
    }

    QTextStream stream(&file);
    do {
        QString row;
        for (Cache* cache : m_inputs->fileCaches()) {
            Values vals = cache->readFrontRow(trialId).second;
            cache->flushFrontRow(trialId);
            for (Value val : vals) {
                row += val.toQString() + ",";
            }
        }
        row.chop(1);
        stream << row << "\n";

    // we synchronously flush all the io stuff. So, it's safe to say
    // that if the front Output is empty, then all others are also empty.
    } while (!m_inputs->fileCaches().front()->isEmpty(trialId));

    file.close();
    return true;
}

bool Experiment::removeOutput(OutputPtr output)
{
    if (m_expStatus != Experiment::READY) {
        qWarning() << "tried to remove an 'Output' from a running experiment. You should pause it first.";
        return false;
    }

    if (!output->isEmpty()) {
        qWarning() << "tried to remove an 'Output' that seems to be used somewhere. It should be cleaned first.";
        return false;
    }

    std::unordered_set<OutputPtr>::iterator it = m_outputs.find(output);
    if (it == m_outputs.end()) {
        qWarning() << "tried to remove a non-existent 'Output'.";
        return false;
    }

    m_outputs.erase(it);
    return true;
}

OutputPtr Experiment::searchOutput(const OutputPtr find)
{
    for (OutputPtr output : m_outputs) {
        if (output->operator==(find)) {
            return output;
        }
    }
    return nullptr;
}

void Experiment::setExpStatus(Status s)
{
    QMutexLocker locker(&m_mutex);
    m_expStatus = s;
    emit (statusChanged(m_expStatus));
}

} // evoplex
