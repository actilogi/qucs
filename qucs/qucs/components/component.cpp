/***************************************************************************
                               component.cpp
                              ---------------
    begin                : Sat Aug 23 2003
    copyright            : (C) 2003 by Michael Margraf
    email                : michael.margraf@alumni.tu-berlin.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <stdlib.h>
#include <cmath>

#include "componentdialog.h"
#include "components.h"
#include "node.h"
#include "qucs.h"
#include "schematicview.h"
#include "schematicscene.h"
#include "module.h"
#include "misc.h"

#include <QPen>
#include <QString>
#include <QMessageBox>
#include <QPainter>
#include <QDebug>

#include <assert.h>

/*!
 * \file component.cpp
 * \brief Implementation of the Component class.
 */


/*!
 * \class Component
 * \brief The Component class implements a generic analog component
 */
Component::Component() : GraphicItem()
{
  ElemType = isAnalogComponent;

  mirroredX = false;
  rotated = 0;
  isActive = COMP_IS_ACTIVE;
  showName = true;

  cx = 0;
  cy = 0;
  tx = 0;
  ty = 0;

  Props.setAutoDelete(true);

  containingSchematic = NULL;

}

// -------------------------------------------------------
Component* Component::newOne()
{
  return new Component();
}

// -------------------------------------------------------
void Component::Bounding(int& _x1, int& _y1, int& _x2, int& _y2)
{
  _x1 = x1+cx;
  _y1 = y1+cy;
  _x2 = x2+cx;
  _y2 = y2+cy;
}

// -------------------------------------------------------
// Size of component text.
int Component::textSize(int& _dx, int& _dy)
{
  // get size of text using the screen-compatible metric
  QFontMetrics metrics(QucsSettings.font, 0);

  int tmp, count=0;
  _dx = _dy = 0;
  if(showName) {
    _dx = metrics.width(Name);
    _dy = metrics.height();
    count++;
  }
  for(Property *pp = Props.first(); pp != 0; pp = Props.next())
    if(pp->display) {
      // get width of text
      tmp = metrics.width(pp->Name+"="+pp->Value);
      if(tmp > _dx)  _dx = tmp;
      _dy += metrics.height();
      count++;
    }
  return count;
}

// -------------------------------------------------------
// Boundings including the component text.
void Component::entireBounds(int& _x1, int& _y1, int& _x2, int& _y2, float Corr)
{
  _x1 = x1+cx;
  _y1 = y1+cy;
  _x2 = x2+cx;
  _y2 = y2+cy;

  // text boundings
  if(tx < x1) _x1 = tx+cx;
  if(ty < y1) _y1 = ty+cy;

  int dx, dy, ny;
  ny = textSize(dx, dy);
  dy = int(float(ny) / Corr);  // correction for unproportional font scaling

  if((tx+dx) > x2) _x2 = tx+dx+cx;
  if((ty+dy) > y2) _y2 = ty+dy+cy;
}

// -------------------------------------------------------
void Component::setCenter(int x, int y, bool relative)
{
  if(relative) { cx += x;  cy += y; }
  else { cx = x;  cy = y; }
}

// -------------------------------------------------------
void Component::getCenter(int& x, int& y)
{
  x = cx;
  y = cy;
}

// -------------------------------------------------------
int Component::getTextSelected(int x_, int y_, float Corr)
{
  x_ -= cx;
  y_ -= cy;
  if(x_ < tx) return -1;
  if(y_ < ty) return -1;

  x_ -= tx;
  y_ -= ty;
  int w, dy = int(float(y_) * Corr);  // correction for font scaling
  // use the screen-compatible metric
  QFontMetrics  metrics(QucsSettings.font, 0);
  if(showName) {
    w  = metrics.width(Name);
    if(dy < 1) {
      if(x_ < w) return 0;
      return -1;
    }
    dy--;
  }

  Property *pp;
  for(pp = Props.first(); pp != 0; pp = Props.next())
    if(pp->display)
      if((dy--) < 1) break;
  if(!pp) return -1;

  // get width of text
  w = metrics.width(pp->Name+"="+pp->Value);
  if(x_ > w) return -1; // clicked past the property text end - selection invalid
  return Props.at()+1;  // number the property
}

// -------------------------------------------------------
bool Component::getSelected(int x_, int y_)
{
  x_ -= cx;
  y_ -= cy;
  if(x_ >= x1) if(x_ <= x2) if(y_ >= y1) if(y_ <= y2)
    return true;

  return false;
}


QRectF Component::boundingRect() const
{
  return QRectF(x1, y1, x2-x1, y2-y1);
}

void Component::paint(QPainter *painter, const QStyleOptionGraphicsItem *item, QWidget *widget)
{
  Q_UNUSED(item);
  Q_UNUSED(widget);


  if(drawScheme) {
    paintScheme(painter);
    return;
  }

  /// \todo move the paint down to each simulation component?
  // is simulation component (dc, ac, ...)
  bool isSimulation = false;
  /// \bug vacomponent crashes, Model empty?
  if(Model.size() && Model.at(0) == '.')
    isSimulation = true;

  QFont f = painter->font();   // save current font
  QFont newFont = f;

  if(isSimulation) {
    /// \todo offset mouse to center, or grab item on any location

    newFont.setPointSizeF(Texts.first()->Size);
    newFont.setWeight(QFont::DemiBold);
    painter->setFont(newFont);
    //p->map(cx, cy, x, y);

    painter->setPen(QPen(Qt::darkBlue,2));

    // compute size of text
    int w=0; // texts width
    int h=0; // texts height
    QRect r, t;
    foreach(Text *pt, Texts) {
      int flags =  Qt::AlignLeft|Qt::TextDontClip;
      t.setRect(0, 0+h, 0, 0);
      painter->drawText(t, flags, pt->s, &r);
      h += r.height();
      if(w < r.width())
        w = r.width();
    }

    // Draw simulation block
    //  diagonal line
    //  horizontal line
    //  diagonal line
    //  vertical line
    //  diagonal line
    //
    // 1 ----- 2
    // | .c    | \
    // |       |  5
    // 4 ----- 3  |
    //   \      \ |
    //    7 ----- 6

    // augmented text box size
    int bx = w + 12;
    int by = h + 10;

    // new origin from center, easier to draw
    int xn = -6;
    int yn = -5;

    // box depth (sort of)
    int dz = 5;

    // update relative boundings and text position
    x2 = x1 + 25 + w;
    y2 = y1 + 23 + h;
    if(ty < y2+1)
      if(ty > y1-r.height())
        ty = y2 + 1;

    painter->drawPoint(0,0); // center
    painter->drawRect(xn,       yn,       bx,       by);
    painter->drawLine(xn,       yn+by,    xn+dz,    yn+by+dz); //diag 4-7
    painter->drawLine(xn+dz,    yn+by+dz, xn+dz+bx, yn+by+dz); //hori 4-3
    painter->drawLine(xn+bx,    yn+by,    xn+bx+dz, yn+by+dz); //diag 3-6
    painter->drawLine(xn+bx+dz, yn+dz,    xn+bx+dz, yn+dz+by); //vert 5-6
    painter->drawLine(xn+bx,    yn,       xn+bx+dz, yn+dz);    //diag 2-5
  }

  if (! isSimulation) { /// \todo move to simulation component?

    // paint all lines
    foreach (Line *l, Lines) {
      QPen pen(l->style);
      pen.setCosmetic(true); // do not scale thickness
      painter->setPen(pen);
      painter->drawLine(l->x1, l->y1, l->x2, l->y2);
    }
    // paint all arcs
    foreach(Arc *a, Arcs) {
      QPen pen(a->style);
      pen.setCosmetic(true); // do not scale thickness
      painter->setPen(pen);
      painter->drawArc(a->x, a->y, a->w, a->h, a->angle, a->arclen);
    }
    // paint all rectangles
    foreach(Area *a, Rects) {
      painter->setPen(a->Pen);
      painter->setBrush(a->Brush);
      painter->drawRect(a->x, a->y, a->w, a->h);
    }
    // paint all ellipses
    foreach(Area *a, Ellips) {
      painter->setPen(a->Pen);
      painter->setBrush(a->Brush);
      painter->drawEllipse(a->x, a->y, a->w, a->h);
    }

    painter->setBrush(Qt::NoBrush);
    newFont.setWeight(QFont::Light);

    /// \todo components with rotated text?? subcircuit maybe?
    /// code similar to portions of GrahicText::paint
/*
    // keep track of painter state
    p->Painter->save();

  // rotate text acordingly
    QMatrix wm = p->Painter->worldMatrix();
*/
    // paint all texts (on the symbol)
    foreach(Text *pt, Texts) {
      //qDebug() << "component text:" << pt->s;
      /*
      p->Painter->setWorldMatrix(
          QMatrix(pt->mCos, -pt->mSin, pt->mSin, pt->mCos,
                   p->DX + float(cx+pt->x) * p->Scale,
                   p->DY + float(cy+pt->y) * p->Scale));
      newFont.setPointSizeF(p->Scale * pt->Size);
      newFont.setOverline(pt->over);
      newFont.setUnderline(pt->under);
      p->Painter->setFont(newFont);
      p->Painter->setPen(pt->Color);
      if (0) {
        p->Painter->drawText(0, 0, 0, 0, Qt::AlignLeft|Qt::TextDontClip, pt->s);
      } else {
        int w, h;
        w = p->drawTextMapped (pt->s, 0, 0, &h);
      }
      */
      /// \todo figure out the rotation and transformation matrix stuff.
      newFont.setPointSize(pt->Size);
      newFont.setOverline(pt->over);
      newFont.setUnderline(pt->under);
      painter->setFont(newFont);
      painter->setPen(pt->Color);
      /// \todo crude text placement
      int flags = Qt::AlignLeft|Qt::TextDontClip;
      painter->drawText(pt->x, pt->y, 0, 0, flags, pt->s);
    }
/*
    p->Painter->setWorldMatrix(wm);
    p->Painter->setWorldMatrixEnabled(false);

    // restore painter state
    p->Painter->restore();
*/

  } // not simulation

  // restore previous font
  painter->setFont(f);
  painter->setPen(QPen(Qt::black,1));

  // keep track of text vertical displacement
  int y=0;
  if(showName) {
    painter->drawText(tx, ty+y, 0, 0, Qt::TextDontClip, Name);
    y += painter->fontMetrics().lineSpacing();
  }

  // write all properties
  for(Property* pp : Props) {
    Property p=*pp;
    if(p.display) {
      painter->drawText(tx, ty+y, 0, 0, Qt::TextDontClip, p.Name+"="+p.Value);
      y += painter->fontMetrics().lineSpacing();
    }
  }

  // draw crossed box for active/inactive/shorted state
  if(isActive == COMP_IS_OPEN)
    painter->setPen(QPen(Qt::red,0));
  else if(isActive & COMP_IS_SHORTEN)
    painter->setPen(QPen(Qt::darkGreen,0));
  if(isActive != COMP_IS_ACTIVE) {
    painter->drawRect(x1, y1, x2-x1+1, y2-y1+1);
    painter->drawLine(x1, y1, x2, y2);
    painter->drawLine(x1, y2, x2, y1);
  }

  // draw component bounding box
  if(isSelected()) {
    painter->setPen(QPen(Qt::darkGray,3));
    painter->drawRoundedRect(boundingRect(), 5.0, 5.0);
  }
  else {
    // else visualize boundingRect
    boundingBoxColor.setCosmetic(true); // do not scale thickness
    painter->setPen(boundingBoxColor);
    painter->drawRect(boundingRect());
  }
}

// -------------------------------------------------------
// Paints the component when moved with the mouse.
// Draw relative to cx, cy, which are updated on mouse move event
void Component::paintScheme(QPainter *painter)
{
  if(Model.at(0) == '.') {   // is simulation component (dc, ac, ...)
    int a, b, xb, yb;
    QFont newFont = painter->font();

    float Scale =
          ((SchematicView*)QucsMain->DocumentTab->currentWidget())->Scale;
    newFont.setPointSizeF(float(Scale) * QucsSettings.largeFontSize);
    newFont.setWeight(QFont::DemiBold);
    // here the font metric is already the screen metric, since the font
    // is the current font the painter is using
    QFontMetrics  metrics(newFont);

    a = b = 0;
    QSize r;
    foreach(Text *pt, Texts) {
      r = metrics.size(0, pt->s);
      b += r.height();
      if(a < r.width())  a = r.width();
    }
    xb = a + int(12.0*Scale);
    yb = b + int(10.0*Scale);
    x2 = x1+25 + int(float(a) / Scale);
    y2 = y1+23 + int(float(b) / Scale);
    if(ty < y2+1) if(ty > y1-r.height())  ty = y2 + 1;

    painter->drawRect(cx-6,    cy-5,  xb, yb);
    painter->drawLine(cx-1,    cy+yb, cx-6,    cy+yb-5);
    painter->drawLine(cx+xb-2, cy+yb, cx-1,    cy+yb);
    painter->drawLine(cx+xb-2, cy+yb, cx+xb-6, cy+yb-5);
    painter->drawLine(cx+xb-2, cy+yb, cx+xb-2, cy);
    painter->drawLine(cx+xb-2, cy,    cx+xb-6, cy-5);
    return;
  }

  // paint all lines
  foreach(Line *p1, Lines)
    painter->drawLine(cx+p1->x1, cy+p1->y1, cx+p1->x2, cy+p1->y2);

  // paint all ports
  foreach(Port *p2, Ports)
    if(p2->avail) painter->drawEllipse(cx+p2->x-4, cy+p2->y-4, 8, 8);

  foreach(Arc *p3, Arcs)   // paint all arcs
    painter->drawArc(cx+p3->x, cy+p3->y, p3->w, p3->h, p3->angle, p3->arclen);


  foreach(Area *pa, Rects) // paint all rectangles
    painter->drawRect(cx+pa->x, cy+pa->y, pa->w, pa->h);

  foreach(Area *pa, Ellips) // paint all ellipses
    painter->drawEllipse(cx+pa->x, cy+pa->y, pa->w, pa->h);
}

// -------------------------------------------------------
// For output on a printer device.
/// \todo is Component::print needed?
void Component::print(QPainter *p, float FontScale)
{
  foreach(Text *pt, Texts)
    pt->Size *= FontScale;

  /// \todo paint(p);

 foreach(Text *pt, Texts)
    pt->Size /= FontScale;
}

void Component::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
  qDebug() << "hoverEnter" << this->Name;
  boundingBoxColor = QPen(Qt::blue,2);

  // show properties as tooltip
  QString str = QString(
    "Description : %1\n"
    "Name        : %2\n"
    "Model       : %3\n"
    "Ports       : %4\n"
    "Props       : %5\n"
    "mirroredX   : %6\n"
    "rotated     : %7\n"
    "isActive    : %8\n"
    "cx,cy       : %9, %10"
     )
       .arg(Description)
       .arg(Name)
       .arg(Model)
       .arg(QString::number(Ports.size()))
       .arg(QString::number(Props.count()))
       .arg(QString::number(mirroredX))
       .arg(QString::number(rotated))
       .arg(QString::number(isActive))
       .arg(QString::number(cx))
       .arg(QString::number(cy));

  setToolTip(str);
}

void Component::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
  qDebug() << "hoverLeave" << this->Name;
  boundingBoxColor = QPen(Qt::magenta,1);
}

// -------------------------------------------------------
// Rotates the component 90 counter-clockwise around its center
void Component::rotate()
{
  // Port count only available after recreate, createSymbol
  if ((Model != "Sub") && (Model !="VHDL") && (Model != "Verilog")) // skip port count
    if(Ports.count() < 1) return;  // do not rotate components without ports
  int tmp, dx, dy;

  // rotate all lines
  foreach(Line *p1, Lines) {
    tmp = -p1->x1;
    p1->x1 = p1->y1;
    p1->y1 = tmp;
    tmp = -p1->x2;
    p1->x2 = p1->y2;
    p1->y2 = tmp;
  }

  // rotate all ports
  foreach(Port *p2, Ports) {
    tmp = -p2->x;
    p2->x = p2->y;
    p2->y = tmp;
  }

  // rotate all arcs
  foreach(Arc *p3, Arcs) {
    tmp = -p3->x;
    p3->x = p3->y;
    p3->y = tmp - p3->w;
    tmp = p3->w;
    p3->w = p3->h;
    p3->h = tmp;
    p3->angle += 16*90;
    if(p3->angle >= 16*360) p3->angle -= 16*360;;
  }

  // rotate all rectangles
  foreach(Area *pa, Rects) {
    tmp = -pa->x;
    pa->x = pa->y;
    pa->y = tmp - pa->w;
    tmp = pa->w;
    pa->w = pa->h;
    pa->h = tmp;
  }

  // rotate all ellipses
  foreach(Area *pa, Ellips) {
    tmp = -pa->x;
    pa->x = pa->y;
    pa->y = tmp - pa->w;
    tmp = pa->w;
    pa->w = pa->h;
    pa->h = tmp;
  }

  // rotate all text
  float ftmp;
  foreach(Text *pt, Texts) {
    tmp = -pt->x;
    pt->x = pt->y;
    pt->y = tmp;

    ftmp = -pt->mSin;
    pt->mSin = pt->mCos;
    pt->mCos = ftmp;
  }

  tmp = -x1;   // rotate boundings
  x1  = y1; y1 = -x2;
  x2  = y2; y2 = tmp;

  tmp = -tx;    // rotate text position
  tx  = ty;
  ty  = tmp;
  // use the screen-compatible metric
  QFontMetrics  metrics(QucsSettings.font, 0);   // get size of text
  dx = dy = 0;
  if(showName) {
    dx = metrics.width(Name);
    dy = metrics.lineSpacing();
  }
  for(Property *pp = Props.first(); pp != 0; pp = Props.next())
    if(pp->display) {
      // get width of text
      tmp = metrics.width(pp->Name+"="+pp->Value);
      if(tmp > dx) dx = tmp;
      dy += metrics.lineSpacing();
    }
  if(tx > x2) ty = y1-ty+y2;    // rotate text position
  else if(ty < y1) ty -= dy;
  else if(tx < x1) { tx += dy-dx;  ty = y1-ty+y2; }
  else ty -= dx;

  rotated++;  // keep track of what's done
  rotated &= 3;
}

// -------------------------------------------------------
// Mirrors the component about the x-axis.
void Component::mirrorX()
{
  // Port count only available after recreate, createSymbol
  if ((Model != "Sub") && (Model !="VHDL") && (Model != "Verilog")) // skip port count
    if(Ports.count() < 1) return;  // do not rotate components without ports

  // mirror all lines
  foreach(Line *p1, Lines) {
    p1->y1 = -p1->y1;
    p1->y2 = -p1->y2;
  }

  // mirror all ports
  foreach(Port *p2, Ports)
    p2->y = -p2->y;

  // mirror all arcs
  foreach(Arc *p3, Arcs) {
    p3->y = -p3->y - p3->h;
    if(p3->angle > 16*180) p3->angle -= 16*360;
    p3->angle  = -p3->angle;    // mirror
    p3->angle -= p3->arclen;    // go back to end of arc
    if(p3->angle < 0) p3->angle += 16*360;  // angle has to be > 0
  }

  // mirror all rectangles
  foreach(Area *pa, Rects)
    pa->y = -pa->y - pa->h;

  // mirror all ellipses
  foreach(Area *pa, Ellips)
    pa->y = -pa->y - pa->h;

  QFont f = QucsSettings.font;
  // mirror all text
  foreach(Text *pt, Texts) {
    f.setPointSizeF(pt->Size);
    // use the screen-compatible metric
    QFontMetrics  smallMetrics(f, 0);
    QSize s = smallMetrics.size(0, pt->s);   // use size for more lines
    pt->y = -pt->y - int(pt->mCos)*s.height() + int(pt->mSin)*s.width();
  }

  int tmp = y1;
  y1  = -y2; y2 = -tmp;   // mirror boundings
  // use the screen-compatible metric
  QFontMetrics  metrics(QucsSettings.font, 0);   // get size of text
  int dy = 0;
  if(showName)
    dy = metrics.lineSpacing();   // for "Name"
  for(Property *pp = Props.first(); pp != 0; pp = Props.next())
    if(pp->display)  dy += metrics.lineSpacing();
  if((tx > x1) && (tx < x2)) ty = -ty-dy;     // mirror text position
  else ty = y1+ty+y2;

  mirroredX = !mirroredX;    // keep track of what's done
  rotated += rotated << 1;
  rotated &= 3;
}

// -------------------------------------------------------
// Mirrors the component about the y-axis.
void Component::mirrorY()
{
  // Port count only available after recreate, createSymbol
  if ((Model != "Sub") && (Model !="VHDL") && (Model != "Verilog")) // skip port count
    if(Ports.count() < 1) return;  // do not rotate components without ports

  // mirror all lines
  foreach(Line *p1, Lines) {
    p1->x1 = -p1->x1;
    p1->x2 = -p1->x2;
  }

  // mirror all ports
  foreach(Port *p2, Ports)
    p2->x = -p2->x;

  // mirror all arcs
  foreach(Arc *p3, Arcs) {
    p3->x = -p3->x - p3->w;
    p3->angle = 16*180 - p3->angle - p3->arclen;  // mirror
    if(p3->angle < 0) p3->angle += 16*360;   // angle has to be > 0
  }

  // mirror all rectangles
  foreach(Area *pa, Rects)
    pa->x = -pa->x - pa->w;

  // mirror all ellipses
  foreach(Area *pa, Ellips)
    pa->x = -pa->x - pa->w;

  int tmp;
  QFont f = QucsSettings.font;
  // mirror all text
  foreach(Text *pt, Texts) {
    f.setPointSizeF(pt->Size);
    // use the screen-compatible metric
    QFontMetrics  smallMetrics(f, 0);
    QSize s = smallMetrics.size(0, pt->s);   // use size for more lines
    pt->x = -pt->x - int(pt->mSin)*s.height() - int(pt->mCos)*s.width();
  }

  tmp = x1;
  x1  = -x2; x2 = -tmp;   // mirror boundings
  // use the screen-compatible metric
  QFontMetrics  metrics(QucsSettings.font, 0);   // get size of text
  int dx = 0;
  if(showName)
    dx = metrics.width(Name);
  for(Property *pp = Props.first(); pp != 0; pp = Props.next())
    if(pp->display) {
      // get width of text
      tmp = metrics.width(pp->Name+"="+pp->Value);
      if(tmp > dx)  dx = tmp;
    }
  if((ty > y1) && (ty < y2)) tx = -tx-dx;     // mirror text position
  else tx = x1+tx+x2;

  mirroredX = !mirroredX;   // keep track of what's done
  rotated += rotated << 1;
  rotated += 2;
  rotated &= 3;
}

// -------------------------------------------------------
QString Component::netlist()
{
  QString s = Model+":"+Name;
  int i=-1;
  // output all node names
  // This only works in cases where the resistor would be a series
  // with the component, as for the other components, they're accounted
  // as a resistor as well, and the changes were made to their .cpp
  foreach(Port *p1, Ports){
    i++;
    s += " " + p1->Connection->Name;   // node names
  }

  // output all properties
  for (Property *p2 = Props.first(); p2 != 0; p2 = Props.next()){
    if (p2->Name != "Symbol"){
      s += " " + p2->Name + "=\"" + p2->Value + "\"";
    }else{
      // BUG: what is this?
      // doing name dependent stuff
    }
  }

  s += '\n';

  return s;
}

// -------------------------------------------------------
QString Component::getNetlist()
{
  switch(isActive) {
    case COMP_IS_ACTIVE:
      return netlist();
    case COMP_IS_OPEN:
      return QString("");
  }

  // Component is shortened.
  int z=0;
  QListIterator<Port *> iport(Ports);
  Port *pp = iport.next();
  QString Node1 = pp->Connection->Name;
  QString s = "";
  while (iport.hasNext())
    s += "R:" + Name + "." + QString::number(z++) + " " +
      Node1 + " " + iport.next()->Connection->Name + " R=\"0\"\n";
  return s;
}

// -------------------------------------------------------
QString Component::verilogCode(int)
{
  return QString("");   // no digital model
}

// -------------------------------------------------------
QString Component::get_Verilog_Code(int NumPorts)
{
  switch(isActive) {
    case COMP_IS_OPEN:
      return QString("");
    case COMP_IS_ACTIVE:
      return verilogCode(NumPorts);
  }

  // Component is shortened.
  QListIterator<Port *> iport(Ports);
  Port *pp = iport.next();
  QString Node1 = pp->Connection->Name;
  QString s = "";
  while (iport.hasNext())
    s += "  assign " + iport.next()->Connection->Name + " = " + Node1 + ";\n";
  return s;
}

// -------------------------------------------------------
QString Component::vhdlCode(int)
{
  return QString("");   // no digital model
}

// -------------------------------------------------------
QString Component::get_VHDL_Code(int NumPorts)
{
  switch(isActive) {
    case COMP_IS_OPEN:
      return QString("");
    case COMP_IS_ACTIVE:
      return vhdlCode(NumPorts);
  }

  // Component is shortened.
  // This puts the signal of the second port onto the first port.
  // This is logically correct for the inverter only, but makes
  // some sense for the gates (OR, AND etc.).
  // Has anyone a better idea?
  QString Node1 = Ports.at(0)->Connection->Name;
  return "  " + Node1 + " <= " + Ports.at(1)->Connection->Name + ";\n";
}
// *******************************************************************

// ***  The following functions are used to load the schematic symbol
// ***  from file. (e.g. subcircuit, library component)

int Component::analyseLine(const QString& Row, int numProps)
{
  QPen Pen;
  QBrush Brush;
  QColor Color;
  QString s;
  int i1, i2, i3, i4, i5, i6;

  s = Row.section(' ',0,0);    // component type
  if((s == "PortSym") || (s == ".PortSym")) {  // backward compatible
    if(!getIntegers(Row, &i1, &i2, &i3))
      return -1;
    for(i6 = Ports.count(); i6<i3; i6++)  // if ports not in numerical order
      Ports.append(new Port(0, 0, false));

    Port *po = Ports.at(i3-1);
    po->x  = i1;
    po->y = i2;
    po->avail = true;

    if(i1 < x1)  x1 = i1;  // keep track of component boundings
    if(i1 > x2)  x2 = i1;
    if(i2 < y1)  y1 = i2;
    if(i2 > y2)  y2 = i2;
    return 0;   // do not count Ports
  }
  else if(s == "Line") {
    if(!getIntegers(Row, &i1, &i2, &i3, &i4))  return -1;
    if(!getPen(Row, Pen, 5))  return -1;
    i3 += i1;
    i4 += i2;
    Lines.append(new Line(i1, i2, i3, i4, Pen));

    if(i1 < x1)  x1 = i1;  // keep track of component boundings
    if(i1 > x2)  x2 = i1;
    if(i2 < y1)  y1 = i2;
    if(i2 > y2)  y2 = i2;
    if(i3 < x1)  x1 = i3;
    if(i3 > x2)  x2 = i3;
    if(i4 < y1)  y1 = i4;
    if(i4 > y2)  y2 = i4;
    return 1;
  }
  else if(s == "EArc") {
    if(!getIntegers(Row, &i1, &i2, &i3, &i4, &i5, &i6))
      return -1;
    if(!getPen(Row, Pen, 7))  return -1;
    Arcs.append(new struct Arc(i1, i2, i3, i4, i5, i6, Pen));

    if(i1 < x1)  x1 = i1;  // keep track of component boundings
    if(i1+i3 > x2)  x2 = i1+i3;
    if(i2 < y1)  y1 = i2;
    if(i2+i4 > y2)  y2 = i2+i4;
    return 1;
  }
  else if(s == ".ID") {
    if(!getIntegers(Row, &i1, &i2))  return -1;
    tx = i1;
    ty = i2;
    Name = Row.section(' ',3,3);
    if(Name.isEmpty())  Name = "SUB";

    i1 = 1;
    Property *pp = Props.at(numProps-1);
    for(;;) {
      s = Row.section('"', i1,i1);
      if(s.isEmpty())  break;

      pp = Props.next();
      if(pp == 0) {
        pp = new Property();
        Props.append(pp);

        pp->display = (s.at(0) == '1');
        pp->Value = s.section('=', 2,2);
      }

      pp->Name  = s.section('=', 1,1);
      pp->Description = s.section('=', 3,3);
      if(pp->Description.isEmpty())
        pp->Description = " ";

      i1 += 2;
    }

    while(pp != Props.last())
      Props.remove();
    return 0;   // do not count IDs
  }
  else if(s == "Arrow") {
    if(!getIntegers(Row, &i1, &i2, &i3, &i4, &i5, &i6))  return -1;
    if(!getPen(Row, Pen, 7))  return -1;

    double beta   = atan2(double(i6), double(i5));
    double phi    = atan2(double(i4), double(i3));
    double Length = sqrt(double(i6*i6 + i5*i5));

    i3 += i1;
    i4 += i2;
    if(i1 < x1)  x1 = i1;  // keep track of component boundings
    if(i1 > x2)  x2 = i1;
    if(i3 < x1)  x1 = i3;
    if(i3 > x2)  x2 = i3;
    if(i2 < y1)  y1 = i2;
    if(i2 > y2)  y2 = i2;
    if(i4 < y1)  y1 = i4;
    if(i4 > y2)  y2 = i4;

    Lines.append(new Line(i1, i2, i3, i4, Pen));   // base line

    double w = beta+phi;
    i5 = i3-int(Length*cos(w));
    i6 = i4-int(Length*sin(w));
    Lines.append(new Line(i3, i4, i5, i6, Pen)); // arrow head
    if(i5 < x1)  x1 = i5;  // keep track of component boundings
    if(i5 > x2)  x2 = i5;
    if(i6 < y1)  y1 = i6;
    if(i6 > y2)  y2 = i6;

    w = phi-beta;
    i5 = i3-int(Length*cos(w));
    i6 = i4-int(Length*sin(w));
    Lines.append(new Line(i3, i4, i5, i6, Pen));
    if(i5 < x1)  x1 = i5;  // keep track of component boundings
    if(i5 > x2)  x2 = i5;
    if(i6 < y1)  y1 = i6;
    if(i6 > y2)  y2 = i6;

    return 1;
  }
  else if(s == "Ellipse") {
    if(!getIntegers(Row, &i1, &i2, &i3, &i4))  return -1;
    if(!getPen(Row, Pen, 5))  return -1;
    if(!getBrush(Row, Brush, 8))  return -1;
    Ellips.append(new Area(i1, i2, i3, i4, Pen, Brush));

    if(i1 < x1)  x1 = i1;  // keep track of component boundings
    if(i1 > x2)  x2 = i1;
    if(i2 < y1)  y1 = i2;
    if(i2 > y2)  y2 = i2;
    if(i1+i3 < x1)  x1 = i1+i3;
    if(i1+i3 > x2)  x2 = i1+i3;
    if(i2+i4 < y1)  y1 = i2+i4;
    if(i2+i4 > y2)  y2 = i2+i4;
    return 1;
  }
  else if(s == "Rectangle") {
    if(!getIntegers(Row, &i1, &i2, &i3, &i4))  return -1;
    if(!getPen(Row, Pen, 5))  return -1;
    if(!getBrush(Row, Brush, 8))  return -1;
    Rects.append(new Area(i1, i2, i3, i4, Pen, Brush));

    if(i1 < x1)  x1 = i1;  // keep track of component boundings
    if(i1 > x2)  x2 = i1;
    if(i2 < y1)  y1 = i2;
    if(i2 > y2)  y2 = i2;
    if(i1+i3 < x1)  x1 = i1+i3;
    if(i1+i3 > x2)  x2 = i1+i3;
    if(i2+i4 < y1)  y1 = i2+i4;
    if(i2+i4 > y2)  y2 = i2+i4;
    return 1;
  }
  else if(s == "Text") {  // must be last in order to reuse "s" *********
    if(!getIntegers(Row, &i1, &i2, &i3, 0, &i4))  return -1;
    Color.setNamedColor(Row.section(' ',4,4));
    if(!Color.isValid()) return -1;

    s = Row.mid(Row.indexOf('"')+1);    // Text (can contain " !!!)
    s = s.left(s.length()-1);
    if(s.isEmpty()) return -1;
    misc::convert2Unicode(s);

    Texts.append(new Text(i1, i2, s, Color, float(i3),
                          float(cos(float(i4)*pi/180.0)),
                          float(sin(float(i4)*pi/180.0))));

    QFont Font(QucsSettings.font);
    Font.setPointSizeF(float(i3));
    QFontMetrics  metrics(Font, 0); // use the screen-compatible metric
    QSize r = metrics.size(0, s);    // get size of text
    i3 = i1 + int(float(r.width())  * Texts.last()->mCos)
            + int(float(r.height()) * Texts.last()->mSin);
    i4 = i2 + int(float(r.width())  * -Texts.last()->mSin)
            + int(float(r.height()) * Texts.last()->mCos);

    if(i1 < x1)  x1 = i1;  // keep track of component boundings
    if(i2 < y1)  y1 = i2;
    if(i1 > x2)  x2 = i1;
    if(i2 > y2)  y2 = i2;

    if(i3 < x1)  x1 = i3;
    if(i4 < y1)  y1 = i4;
    if(i3 > x2)  x2 = i3;
    if(i4 > y2)  y2 = i4;
    return 1;
  }

  return 0;
}

// ---------------------------------------------------------------------
bool Component::getIntegers(const QString& s, int *i1, int *i2, int *i3,
			     int *i4, int *i5, int *i6)
{
  bool ok;
  QString n;

  if(!i1) return true;
  n  = s.section(' ',1,1);
  *i1 = n.toInt(&ok);
  if(!ok) return false;

  if(!i2) return true;
  n  = s.section(' ',2,2);
  *i2 = n.toInt(&ok);
  if(!ok) return false;

  if(!i3) return true;
  n  = s.section(' ',3,3);
  *i3 = n.toInt(&ok);
  if(!ok) return false;

  if(i4) {
    n  = s.section(' ',4,4);
    *i4 = n.toInt(&ok);
    if(!ok) return false;
  }

  if(!i5) return true;
  n  = s.section(' ',5,5);
  *i5 = n.toInt(&ok);
  if(!ok) return false;

  if(!i6) return true;
  n  = s.section(' ',6,6);
  *i6 = n.toInt(&ok);
  if(!ok) return false;

  return true;
}

// ---------------------------------------------------------------------
bool Component::getPen(const QString& s, QPen& Pen, int i)
{
  bool ok;
  QString n;

  n = s.section(' ',i,i);    // color
  QColor co;
  co.setNamedColor(n);
  Pen.setColor(co);
  if(!Pen.color().isValid()) return false;

  i++;
  n = s.section(' ',i,i);    // thickness
  Pen.setWidth(n.toInt(&ok));
  if(!ok) return false;

  i++;
  n = s.section(' ',i,i);    // line style
  Pen.setStyle((Qt::PenStyle)n.toInt(&ok));
  if(!ok) return false;

  return true;
}

// ---------------------------------------------------------------------
bool Component::getBrush(const QString& s, QBrush& Brush, int i)
{
  bool ok;
  QString n;

  n = s.section(' ',i,i);    // fill color
  QColor co;
  co.setNamedColor(n);
  Brush.setColor(co);
  if(!Brush.color().isValid()) return false;

  i++;
  n = s.section(' ',i,i);    // fill style
  Brush.setStyle((Qt::BrushStyle)n.toInt(&ok));
  if(!ok) return false;

  i++;
  n = s.section(' ',i,i);    // filled
  if(n.toInt(&ok) == 0) Brush.setStyle(Qt::NoBrush);
  if(!ok) return false;

  return true;
}

// ---------------------------------------------------------------------
Property * Component::getProperty(const QString& name)
{
  for(Property *pp = Props.first(); pp != 0; pp = Props.next())
    if(pp->Name == name) {
      return pp;
    }
  return NULL;
}

// ---------------------------------------------------------------------
void Component::copyComponent(Component *pc)
{
  ElemType = pc->ElemType;
  x1 = pc->x1;
  y1 = pc->y1;
  x2 = pc->x2;
  y2 = pc->y2;

  Model = pc->Model;
  Name  = pc->Name;
  showName = pc->showName;
  Description = pc->Description;

  isActive = pc->isActive;
  rotated  = pc->rotated;
  mirroredX = pc->mirroredX;
  tx = pc->tx;
  ty = pc->ty;

  Props  = pc->Props;
  Ports  = pc->Ports;
  Lines  = pc->Lines;
  Arcs   = pc->Arcs;
  Rects  = pc->Rects;
  Ellips = pc->Ellips;
  Texts  = pc->Texts;
}


// ***********************************************************************
// ********                                                       ********
// ********          Functions of class MultiViewComponent        ********
// ********                                                       ********
// ***********************************************************************
void MultiViewComponent::recreate(SchematicScene *scene)
{

  if(scene) {
    scene->deleteComp(this);
  }

  Ellips.clear();
  Texts.clear();
  Ports.clear();
  Lines.clear();
  Rects.clear();
  Arcs.clear();
  createSymbol();

  bool mmir = mirroredX;
  int  rrot = rotated;
  if (mmir && rrot==2) // mirrorX and rotate 180 = mirrorY
    mirrorY();
  else  {
    if(mmir)
      mirrorX();   // mirror
    if (rrot)
      for(int z=0; z<rrot; z++)  rotate(); // rotate
  }

  rotated = rrot;   // restore properties (were changed by rotate/mirror)
  mirroredX = mmir;

  if(scene) {
    scene->insertRawComponent(this);
  }
}


// ***********************************************************************
// ********                                                       ********
// ********            Functions of class GateComponent           ********
// ********                                                       ********
// ***********************************************************************
GateComponent::GateComponent()
{
  ElemType = isComponent;   // both analog and digital
  Name  = "Y";

  // the list order must be preserved !!!
  Props.append(new Property("in", "2", false,
		QObject::tr("number of input ports")));
  Props.append(new Property("V", "1 V", false,
		QObject::tr("voltage of high level")));
  Props.append(new Property("t", "0", false,
		QObject::tr("delay time")));
  Props.append(new Property("TR", "10", false,
		QObject::tr("transfer function scaling factor")));

  // this must be the last property in the list !!!
  Props.append(new Property("Symbol", "old", false,
		QObject::tr("schematic symbol")+" [old, DIN40900]"));
}

// -------------------------------------------------------
QString GateComponent::netlist()
{
  QString s = Model+":"+Name;

  // output all node names
  foreach(Port *pp, Ports)
    s += " "+pp->Connection->Name;   // node names

  // output all properties
  Property *p = Props.at(1);
  s += " " + p->Name + "=\"" + p->Value + "\"";
  p = Props.next();
  s += " " + p->Name + "=\"" + p->Value + "\"";
  p = Props.next();
  s += " " + p->Name + "=\"" + p->Value + "\"\n";
  return s;
}

// -------------------------------------------------------
QString GateComponent::vhdlCode(int NumPorts)
{
  QListIterator<Port *> iport(Ports);
  Port *pp = iport.next();
  QString s = "  " + pp->Connection->Name + " <= ";  // output port

  // xnor NOT defined for std_logic, so here use not and xor
  if (Model == "XNOR") {
    QString Op = " xor ";

    // first input port
    pp = iport.next();
    QString rhs = pp->Connection->Name;

    // output all input ports with node names
    while(iport.hasNext()) {
      pp = iport.next();
      rhs = "not ((" + rhs + ")" + Op + pp->Connection->Name + ")";
    }
    s += rhs;
  }
  else {
    QString Op = ' ' + Model.toLower() + ' ';
    if(Model.at(0) == 'N') {
      s += "not (";    // nor, nand is NOT assoziative !!! but xnor is !!!
      Op = Op.remove(1, 1);
    }

    pp = iport.next();
    s += pp->Connection->Name;   // first input port

    // output all input ports with node names
    while(iport.hasNext()) {
      pp = iport.next();
      s += Op + pp->Connection->Name;
    }
    if(Model.at(0) == 'N')
      s += ')';
  }

  if(NumPorts <= 0) { // no truth table simulation ?
    QString td = Props.at(2)->Value;        // delay time
    if(!misc::VHDL_Delay(td, Name)) return td;
    s += td;
  }

  s += ";\n";
  return s;
}

// -------------------------------------------------------
QString GateComponent::verilogCode(int NumPorts)
{
  bool synthesize = true;
  QListIterator<Port *> iport(Ports);
  Port *pp = iport.next();
  QString s("");

  if(synthesize) {
    QString op = Model.toLower();
    if(op == "and" || op == "nand")
      op = "&";
    else if (op == "or" || op == "nor")
      op = "|";
    else if (op == "xor")
      op = "^";
    else if (op == "xnor")
      op = "^~";

    s = "  assign";

    if(NumPorts <= 0) { // no truth table simulation ?
      QString td = Props.at(2)->Value;        // delay time
      if(!misc::Verilog_Delay(td, Name)) return td;
      s += td;
    }
    s += " " + pp->Connection->Name + " = ";  // output port
    if(Model.at(0) == 'N') s += "~(";

    pp = iport.next();
    s += pp->Connection->Name;   // first input port

    // output all input ports with node names
    while (iport.hasNext()) {
      pp = iport.next();
      s += " " + op + " " + pp->Connection->Name;
    }

    if(Model.at(0) == 'N') s += ")";
    s += ";\n";
  }
  else {
    s = "  " + Model.toLower();

    if(NumPorts <= 0) { // no truth table simulation ?
      QString td = Props.at(2)->Value;        // delay time
      if(!misc::Verilog_Delay(td, Name)) return td;
      s += td;
    }
    s += " " + Name + " (" + pp->Connection->Name;  // output port

    pp = iport.next();
    s += ", " + pp->Connection->Name;   // first input port

    // output all input ports with node names
    while (iport.hasNext()) {
      pp = iport.next();
      s += ", " + pp->Connection->Name;
    }

    s += ");\n";
  }
  return s;
}

// -------------------------------------------------------
void GateComponent::createSymbol()
{
  int Num = Props.getFirst()->Value.toInt();
  if(Num < 2) Num = 2;
  else if(Num > 8) Num = 8;
  Props.getFirst()->Value = QString::number(Num);

  int xl, xr, y = 10*Num, z;
  x1 = -30; y1 = -y-3;
  x2 =  30; y2 =  y+3;

  tx = x1+4;
  ty = y2+4;

  z = 0;
  if(Model.at(0) == 'N')  z = 1;

  if(Props.getLast()->Value.at(0) == 'D') {  // DIN symbol
    xl = -15;
    xr =  15;
    Lines.append(new Line( 15,-y, 15, y,QPen(Qt::darkBlue,2)));
    Lines.append(new Line(-15,-y, 15,-y,QPen(Qt::darkBlue,2)));
    Lines.append(new Line(-15, y, 15, y,QPen(Qt::darkBlue,2)));
    Lines.append(new Line(-15,-y,-15, y,QPen(Qt::darkBlue,2)));
    Lines.append(new Line( 15, 0, 30, 0,QPen(Qt::darkBlue,2)));

    if(Model.at(z) == 'O') {
      Lines.append(new Line(-11, 6-y,-6, 9-y,QPen(Qt::darkBlue,0)));
      Lines.append(new Line(-11,12-y,-6, 9-y,QPen(Qt::darkBlue,0)));
      Lines.append(new Line(-11,14-y,-6,14-y,QPen(Qt::darkBlue,0)));
      Lines.append(new Line(-11,16-y,-6,16-y,QPen(Qt::darkBlue,0)));
      Texts.append(new Text( -4, 3-y, "1", Qt::darkBlue, 15.0));
    }
    else if(Model.at(z) == 'A')
      Texts.append(new Text( -10, 3-y, "&", Qt::darkBlue, 15.0));
    else if(Model.at(0) == 'X') {
      if(Model.at(1) == 'N') {
	Ellips.append(new Area(xr,-4, 8, 8,
                  QPen(Qt::darkBlue,0), QBrush(Qt::darkBlue)));
        Texts.append(new Text( -11, 3-y, "=1", Qt::darkBlue, 15.0));
      }
      else
        Texts.append(new Text( -11, 3-y, "=1", Qt::darkBlue, 15.0));
    }
  }
  else {   // old symbol

    if(Model.at(z) == 'O')  xl = 10;
    else  xl = -10;
    xr = 10;
    Lines.append(new Line(-10,-y,-10, y,QPen(Qt::darkBlue,2)));
    Lines.append(new Line( 10, 0, 30, 0,QPen(Qt::darkBlue,2)));
    Arcs.append(new Arc(-30,-y, 40, 30, 0, 16*90,QPen(Qt::darkBlue,2)));
    Arcs.append(new Arc(-30,y-30, 40, 30, 0,-16*90,QPen(Qt::darkBlue,2)));
    Lines.append(new Line( 10,15-y, 10, y-15,QPen(Qt::darkBlue,2)));

    if(Model.at(0) == 'X') {
      Lines.append(new Line(-5, 0, 5, 0,QPen(Qt::darkBlue,1)));
      if(Model.at(1) == 'N') {
        Lines.append(new Line(-5,-3, 5,-3,QPen(Qt::darkBlue,1)));
        Lines.append(new Line(-5, 3, 5, 3,QPen(Qt::darkBlue,1)));
      }
      else {
        Arcs.append(new Arc(-5,-5, 10, 10, 0, 16*360,QPen(Qt::darkBlue,1)));
        Lines.append(new Line( 0,-5, 0, 5,QPen(Qt::darkBlue,1)));
      }
    }
  }

  if(Model.at(0) == 'N')
    Ellips.append(new Area(xr,-4, 8, 8,
                  QPen(Qt::darkBlue,0), QBrush(Qt::darkBlue)));

  Ports.append(new Port( 30,  0));
  y += 10;
  for(z=0; z<Num; z++) {
    y -= 20;
    Ports.append(new Port(-30, y));
    if(xl == 10) if((z == 0) || (z == Num-1)) {
      Lines.append(new Line(-30, y, 9, y,QPen(Qt::darkBlue,2)));
      continue;
    }
    Lines.append(new Line(-30, y, xl, y,QPen(Qt::darkBlue,2)));
  }
}


// do something with Dialog Buttons
void Component::dialgButtStuff(ComponentDialog& d)const
{
  d.disableButtons();
}

// vim:ts=8:sw=2:noet
