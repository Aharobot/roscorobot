#ifndef GPS_H
#define GPS_H


//GPS Widget, for more information see the google static map api

#include <QtGui>

class Gps: public QWidget
 {

Q_OBJECT

 public:
     Gps();
     void setList(QListWidget *l);//gives the pointer to the list widget to display the itineraries


 public Q_SLOTS:
     void set_map_type(QString value);//changes the map type
     void set_zoom(int value);//set the map zoom value
     void update_coord(double lat, double lon);//update the last coordinate value
     void start_clicked();//the button start has been clicked
     void stop_clicked();//the button stop has been clicked
     void save_clicked();//the button save has been clicked
     void load_clicked();//the button load has been clicked
     void check_size();//the size od the map if the size of the web widget
     void selection_changed();//the itinerary selection has changed, therefore change the path that is displayed

 Q_SIGNALS:
     void url_changed(QUrl value);

 private :
         QUrl url;
         bool started;
         double started_lat;
         double started_lon;
         QString path;
         QSize size;
         QStringList loaded_files;
         QListWidget *list_widget;
         bool start_pos_set;

 };

#endif // GPS_H
