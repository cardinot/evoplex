/**
 * Copyright (C) 2018 - Marcos Cardinot
 * @author Marcos Cardinot <mcardinot@gmail.com>
 */

#ifndef GRAPHVIEW_H
#define GRAPHVIEW_H

#include "graphwidget.h"

namespace evoplex
{

class GraphView : public GraphWidget
{
public:
    explicit GraphView(MainGUI* mainGUI, Experiment* exp, QWidget* parent);

    virtual void updateCache();

protected:
    void paintEvent(QPaintEvent*) override;
    virtual const Agent* selectAgent(const QPoint& pos) const;

private:
    struct Cache {
        Agent* agent = nullptr;
        QPointF xy;
        std::vector<QLineF> edges;
    };
    std::vector<Cache> m_cache;

    int m_edgeAttr;
    float m_edgeSizeRate;

    bool m_showAgents;
    bool m_showEdges;

    void setEdgeAttr(int idx);
};

} // evoplex
#endif // GRAPHVIEW_H
