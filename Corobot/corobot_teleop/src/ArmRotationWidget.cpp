/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

#include "ArmRotationWidget.h"

#include <math.h>
#include <QtGui>
#include "corobot.h"


//Constant related to the drawings
const int ARM_X = 120;
const int ARM_Y = 120;
const int ARM_CENTER_X = ARM_X / 2;
const int ARM_BODY_TOP = ARM_X * 7 / 10;
const int ARM_SHOULDER_X = ARM_CENTER_X - ARM_X / 7;
const int SIZE_ARM = 25; //the value does not match the size of the real arm. 
const int LENGTH_ROBOT = 55; //the value does not match the size of the real robot.
const int WIDTH_ROBOT = 40; //the value does not match the size of the real robot.
ArmRotationWidget::ArmRotationWidget(QWidget *parent)
    : QGraphicsView(parent), timerId(0)
{

    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    scene->setSceneRect(0, 00, 210, 110);

    setScene(scene);
    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    scale(qreal(1), qreal(1));
    setMinimumSize(210, 110);
    setWindowTitle(tr("Elastic Nodes"));

    joint *joint1 = new joint(this);
    joint *joint2 = new joint(this);

    scene->addItem(joint1);
    scene->addItem(joint2);

    joint2->setFlag(joint2->ItemIsMovable);


    joint1->setPos(this->sceneRect().right()/2 + 5, this->sceneRect().bottom()-10 - LENGTH_ROBOT * 4/5 +5);
    joint2->setPos(this->sceneRect().right()/2 + 5,this->sceneRect().bottom()-10 - LENGTH_ROBOT * 4/5 +5 - SIZE_ARM);

    end_effector = QPointF(0,0);

    QGraphicsLineItem *line = new QGraphicsLineItem(joint1->pos().x()-5,joint1->pos().y()-5,joint2->pos().x()-5,joint2->pos().y()-5);
    scene->addItem(line);

 }


void ArmRotationWidget::itemMoved()
//timer starts if a movement is detected
{
    if (!timerId)
        timerId = startTimer(1000 / 25);
}



void ArmRotationWidget::moveArmLeft()
//moves the arm toward the robot
{
    this->scene()->items().at(2)->setPos(end_effector + QPointF(-5,0));
}

void ArmRotationWidget::moveArmRight()
//moves the arm far from the robot
{
    this->scene()->items().at(2)->setPos(end_effector + QPointF(5,0));
}


void ArmRotationWidget::timerEvent(QTimerEvent *event)
//execute this function when the timer is up.
 {
     Q_UNUSED(event);
     double angle;

     QList<joint *> joints;
         Q_FOREACH (QGraphicsItem *item, scene()->items()) {
             if (joint *j = qgraphicsitem_cast<joint *>(item))
                 joints << j;
         }


         if(joints.at(1)->x()!=end_effector.x() || joints.at(1)->y()!=end_effector.y()){
             double t1,t2;
	     double x = joints.at(0)->x() - joints.at(1)->x();
	     double y = joints.at(0)->y() - joints.at(1)->y();
		
	     if(y > 0)
	     {
	     	angle = atan(x/y) + M_PI/2;
		joints.at(1)->setPos(this->sceneRect().right()/2 + 5 + cos(angle)*SIZE_ARM, this->sceneRect().bottom()-10 - LENGTH_ROBOT * 4/5 +5 - sin(angle)*SIZE_ARM);
		Q_EMIT armAngle_rad(angle);
	     }
	     else
		joints.at(1)->setPos(end_effector);


             QList<QGraphicsLineItem *> lines;
             Q_FOREACH (QGraphicsItem *item, scene()->items()) {
             	if (QGraphicsLineItem *l = qgraphicsitem_cast<QGraphicsLineItem *>(item))
                	lines << l;
             }

 	     lines.at(0)->setLine(joints.at(0)->pos().x()-5,joints.at(0)->pos().y()-5,joints.at(1)->pos().x()-5,joints.at(1)->pos().y()-5);

             end_effector = joints.at(1)->pos();
         }
		

 }


 void ArmRotationWidget::drawBackground(QPainter *painter, const QRectF &rect)
 // draw the widget background
 {
     Q_UNUSED(rect);

     // Shadow
     QRectF sceneRect = this->sceneRect();
     QRectF rightShadow(sceneRect.right(), sceneRect.top() + 5, 5, sceneRect.height());
     QRectF bottomShadow(sceneRect.left() + 5, sceneRect.bottom(), sceneRect.width(), 5);
     if (rightShadow.intersects(rect) || rightShadow.contains(rect))
         painter->fillRect(rightShadow, Qt::darkGray);
     if (bottomShadow.intersects(rect) || bottomShadow.contains(rect))
         painter->fillRect(bottomShadow, Qt::darkGray);


     // Fill
     QLinearGradient gradient(sceneRect.topLeft(), sceneRect.bottomRight());
     gradient.setColorAt(0, Qt::white);
     gradient.setColorAt(1, Qt::lightGray);
     painter->fillRect(rect.intersect(sceneRect), gradient);
     painter->setBrush(Qt::NoBrush);
     painter->drawRect(sceneRect);

     //Robot
      QRectF robot((sceneRect.right() - sceneRect.left())/2 - WIDTH_ROBOT/2, sceneRect.bottom()-LENGTH_ROBOT-10, WIDTH_ROBOT, LENGTH_ROBOT);
      painter->fillRect(robot, Qt::gray);
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(robot);


     // Text
     QRectF textRect(sceneRect.left() + 4, sceneRect.top() + 4,
                     sceneRect.width() - 4, sceneRect.height() - 4);

     QFont font = painter->font();
     font.setBold(true);
     font.setPointSize(14);
     painter->setFont(font);
     painter->setPen(Qt::lightGray);
     painter->setPen(Qt::black);
 }

 void ArmRotationWidget::scaleView(qreal scaleFactor)
 //scale the view
 {
     qreal factor = transform().scale(scaleFactor, scaleFactor).mapRect(QRectF(0, 0, 1, 1)).width();
     if (factor < 0.07 || factor > 100)
         return;

     scale(scaleFactor, scaleFactor);
 }
