#ifndef IMAGEBIG_H
#define IMAGEBIG_H

#include <QGraphicsItem>
#include <QImage>


 class ImageBig : public QGraphicsItem
         //transform a Qimage in a qGraphicsItem
 {
 public:
     ImageBig();

     enum { Type = UserType + 1 };
     int type() const { return Type; }


     QRectF boundingRect() const;
//     QPainterPath shape() const;
     void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);


    void setImage(QImage& image);//set the QImage

    void saveImage();
 private :
         QImage image;

 };

#endif // IMAGEBIG_H
