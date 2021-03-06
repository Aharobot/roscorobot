#include "Ros.h"
#include "corobot.h"
#include <QImage>
#include <QGraphicsTextItem>
#include <QPainter>
#include <QGraphicsView>
#include <math.h>
#include <QImageReader>

#include <vector>

using namespace std;

Ros::Ros(){

	speed_value = 100;
        speed_x = 0;
        speed_a = 0;

        arm_px = (double)UPPER_ARM_LENGTH_INCHES * (double)INCHES_TO_METERS;
        arm_py = (double)LOWER_ARM_LENGTH_INCHES * (double)INCHES_TO_METERS;

        scenes_rear_cam = NULL;
        scenes_ptz_cam = NULL;//front camera scene
        scenes = NULL;
        scenes_kinect_rgb = NULL;
        scenes_kinect_depth = NULL;

        pan = 0;
        tilt = 0;
        move_speed_level = 0;
        turning_speed_level = 0;

	turningRight = false;
	turningLeft = false;


        hokuyo_points_ = new Hokuyo_Points [683];
        speed_left = 0;
        speed_right = 0;

	initialized = false;
}



Ros::~Ros(){
    delete [] hokuyo_points_;
    hokuyo_points_ = NULL;
    if(driveControl_pub)
	this->motor_stop();
}

void Ros::subscribe()
//function that initialize every ros variables declared

{
    ros::NodeHandle n;
    ros::NodeHandle nh("~");
    nh.param("cameraRear_jpeg_compression", cameraRear_jpeg_compression, false);
    nh.param("cameraFront_jpeg_compression", cameraFront_jpeg_compression, false);
    nh.param("Lynxmotion_al5a", arm_al5a, false);
    nh.param("PhantomX_Pincher", arm_pincher, false);
    nh.param("PhantomX_Reactor", arm_reactor, false);
    nh.param("corobot_arm_ssc32", arm_old_ssc32, false);
    nh.param("corobot_arm_phidget", arm_old_phidget, false);


    Q_EMIT arm_model(arm_al5a, arm_pincher, arm_reactor, arm_old_ssc32 || arm_old_phidget);

    setOdom_client = n.serviceClient<corobot_srvs::SetOdom>("set_odom");
    //driveControl_client = n.serviceClient<Coroware_teleop::SSC32NodeSetSpeed>("ssc32control/ssc32node_set_speed");


    //Advertise topics
    driveControl_pub = n.advertise<corobot_msgs::MotorCommand>("PhidgetMotor", 100);
    velocityValue_pub = n.advertise<corobot_msgs::velocityValue>("velocityValue", 100);
    moveArm_pub = n.advertise<corobot_msgs::MoveArm>("armPosition", 100);
    pan_tilt_control = n.advertise<corobot_msgs::PanTilt>("pantilt",5);


    //Subscribe to topics
    spatial=n.subscribe("spatial_data",1000, &Ros::spatialCallback,this);
    velocity= n.subscribe("odometry", 1000, &Ros::velocityCallback,this);
    ir= n.subscribe("infrared_data", 1000, &Ros::irCallback,this);
    power= n.subscribe<corobot_msgs::PowerMsg>("power_data", 1000, &Ros::powerCallback,this);
    bumper= n.subscribe("bumper_data", 1000, &Ros::bumperCallback,this);
    gripper= n.subscribe("gripper_data", 1000, &Ros::gripperCallback,this);
    cameraInfo= n.subscribe("camera_info", 1000, &Ros::cameraInfoCallback,this);
    cameraImage= n.subscribe("image_raw", 1000, &Ros::cameraImageCallback,this);
    gps= n.subscribe("fix", 1000, &Ros::gpsCallback,this);
    scan= n.subscribe("scan", 1000, &Ros::scanCallback,this);
    kinect_rgb = n.subscribe("/camera/rgb/image_color/compressed",100,&Ros::kinectrgbCallback,this);
    kinect_depth = n.subscribe("/camera/depth/image",100,&Ros::kinectdepthCallback,this);
  //  kinect_skel = n.subscribe("/skeletons",10,&Ros::kinectskelCallback,this);
    ssc32_info_sub = n.subscribe("ssc32_info", 10, &Ros::ssc32infoCallback, this);
    phidget_info_sub = n.subscribe("phidget_info", 10, &Ros::phidgetinfoCallback, this);
    takepic_sub = n.subscribe("takepicture", 10, &Ros::takepicCallback, this);
    map_image = n.subscribe("/map_image/full_with_position/compressed",10, &Ros::mapCallback,this);

    //Modified to compressed image
    if(cameraRear_jpeg_compression)
    	rear_cam = n.subscribe("REAR/image_raw/compressed",10, &Ros::rear_camCallback_compressed,this);
    else
	rear_cam = n.subscribe("REAR/image_raw",10, &Ros::rear_camCallback,this);
    if(cameraFront_jpeg_compression)
    	ptz_cam = n.subscribe("PTZ/image_raw/compressed",10, &Ros::ptz_camCallback_compressed,this);
    else
    	ptz_cam = n.subscribe("PTZ/image_raw",10, &Ros::ptz_camCallback,this);



    kinect_selected = true;
    pan = 0;
    tilt = 0;
    timer = nh.createTimer(ros::Duration(2), &Ros::timerCallback, this);
    initialized = true;
}

void Ros::init(){
    int argc=0;
    char** argv= NULL;
    ros::init(argc, argv,"GUI");
  //  ros::start();

    this->subscribe();
    this->start();
}

//subscribe to only the camera topics we are interested in
void Ros::currentCameraTabChanged(int index)
{
    if (initialized == true)
    {
	    ros::NodeHandle n;
	    if(index == 0) // PTZ Camera
	    {
		if(cameraFront_jpeg_compression)
	    	    ptz_cam = n.subscribe("PTZ/image_raw/compressed",10, &Ros::ptz_camCallback_compressed,this);
		else
	    	    ptz_cam = n.subscribe("PTZ/image_raw",10, &Ros::ptz_camCallback,this);
		rear_cam.shutdown();
		kinect_rgb.shutdown();
		kinect_depth.shutdown();
	    }
	    else if(index == 1) // rear Camera
	    {
		if(cameraRear_jpeg_compression)
	    	    rear_cam = n.subscribe("REAR/image_raw/compressed",10, &Ros::rear_camCallback_compressed,this);
		else
		    rear_cam = n.subscribe("REAR/image_raw",10, &Ros::rear_camCallback,this);
		ptz_cam.shutdown();
		kinect_rgb.shutdown();
		kinect_depth.shutdown();
	    }
	    else if (index == 2)
	    {
		rear_cam.shutdown();
		ptz_cam.shutdown();
		kinect_rgb.shutdown();
		kinect_depth.shutdown();
	    }
	    else if(index == 3) // Kinect RGB
	    {
		kinect_rgb = n.subscribe("/camera/rgb/image_color/compressed",100,&Ros::kinectrgbCallback,this);
		rear_cam.shutdown();
		ptz_cam.shutdown();
		kinect_depth.shutdown();
	    }
	    else if(index == 4) // Kinect Depth
	    {
		kinect_depth = n.subscribe("/camera/depth/image",100,&Ros::kinectdepthCallback,this);
		rear_cam.shutdown();
		kinect_rgb.shutdown();
		ptz_cam.shutdown();
	    }
    }
}

void Ros::init(const std::string & master,const std::string & host){
		std::map<std::string,std::string> remappings;
		remappings["__master"] = master;
		remappings["__ip"] = host;
		ros::init(remappings,"GUI");
	 //  ros::start();

		this->subscribe();
		this->start();
}

void Ros::setSpeedFast(bool toggled){
	if(toggled)
	{
		speed_value = 100;
		corobot_msgs::velocityValue msg;
		msg.velocity = speed_value;
		velocityValue_pub.publish(msg);
	}
}
void Ros::setSpeedModerate(bool toggled){
	if(toggled)
	{
		speed_value = 75;
		corobot_msgs::velocityValue msg;
		msg.velocity = speed_value;
		velocityValue_pub.publish(msg);
	}
}
void Ros::setSpeedSlow(bool toggled){
	if(toggled)
	{
		speed_value = 50;
		corobot_msgs::velocityValue msg;
		msg.velocity = speed_value;
		velocityValue_pub.publish(msg);
	}
}

void Ros::kinectdepthCallback(const sensor_msgs::Image::ConstPtr& msg){
    if(kinect_selected){
        unsigned char * copy;
        copy = (unsigned char*)malloc(msg->width*msg->height*3);
        float* tmp;

        for(int j=0;j<(msg->width*msg->height*3);j+=3){
            tmp = (float*)(&msg->data[0]+(j/3)*4);
            copy[j] = (char)((float)64*(float)(*tmp));
            copy[j+1] = (char)((float)64*(float)(*tmp));
            copy[j+2] = (char)((float)64*(float)(*tmp));
        }

    QImage *i = new QImage(copy,msg->width,msg->height,3*msg->width,QImage::Format_RGB888);
    QImage icopy = i->copy(0,0,msg->width,msg->height);
    if(i!=NULL){
            if(scenes_kinect_depth != NULL)
            {

               // scenes_kinect_depth->setBackgroundBrush(QBrush(i->copy(0,0,msg->width,msg->height)));
                Q_EMIT update_kinectDepthcam(icopy); //we can't allow a thread to use the scene for the zoom and another one to modify the image as it would crash, so we do everything in one thread
            }
        }
    free(copy);
    delete i;

}
}


/*
  kinect hand gesture detection
void Ros::kinectskelCallback(const body_msgs::Skeletons::ConstPtr& msg){
    if(kinect_selected){
    scenes_kinect_rgb->clear();

    if((msg->skeletons[0].left_hand.position.x - last_x_pos)>0.05)
        ROS_INFO("right");
    else if ((-msg->skeletons[0].left_hand.position.x + last_x_pos)>0.05)
         ROS_INFO("left");
    if((msg->skeletons[0].left_hand.position.y - last_y_pos)>0.05)
        ROS_INFO("up");
    else if ((-msg->skeletons[0].left_hand.position.y + last_y_pos)>0.05)
         ROS_INFO("down");corobot_teleop::SSC32NodeSetSpeed
    last_x_pos = msg->skeletons[0].left_hand.position.x;
    last_x_pos = msg->skeletons[0].left_hand.position.y;
}
}
*/
void Ros::kinectrgbCallback(const sensor_msgs::CompressedImage::ConstPtr& msg){
    if(kinect_selected){
    QImage *i = new QImage();
    i->loadFromData(&msg->data[0],msg->data.size(),"JPEG");
    QImage icopy = i->scaled(640,480);
    if(i!=NULL){
            if(scenes_kinect_rgb != NULL)
            {

                scenes_kinect_rgb->setSceneRect(0,0,640,480);
                Q_EMIT update_kinectRGBcam(icopy); //we can't allow a thread to use the scene for the zoom and another one to modify the image as it would crash, so we do everything in one thread
            }
        }

    delete i;
    }
}
void Ros::select_kinect(bool value){
    kinect_selected = value;
}


void Ros::velocityCallback(const nav_msgs::Odometry::ConstPtr& msg){
    double linear, angular;
    linear = sqrt(msg->twist.twist.linear.x *msg->twist.twist.linear.x + msg->twist.twist.linear.y * msg->twist.twist.linear.y);
    angular = msg->twist.twist.angular.z;

    Q_EMIT velocity_info(linear,angular);

}
void Ros::irCallback(const corobot_msgs::IrMsg::ConstPtr& msg){

    double ir01, ir02;

    ir01 = (double)msg->range1;
    ir02 = (double)msg->range2;

    ROS_INFO("ir01 = %f", ir01);
    ROS_INFO("ir02 = %f\n", ir02);

    Q_EMIT irData(ir01,ir02);

}
void Ros::powerCallback(const corobot_msgs::PowerMsgConstPtr& msg){
    //ROS_INFO("Got in power Callback!!");
    int percent;
    if(msg->volts>= 12)
        percent = 100;
    else
       percent = ((float)(msg->volts - BATTERY_EMPTY)/(float)(12-BATTERY_EMPTY))*100;
    Q_EMIT battery_percent(percent);
    Q_EMIT battery_volts((double)msg->volts);
}
void Ros::bumperCallback(const corobot_msgs::BumperMsg::ConstPtr& msg){
     Q_EMIT bumper_update(msg->value0, msg->value1, msg->value2, msg->value3);
}

void Ros::gripperCallback(const corobot_msgs::GripperMsg::ConstPtr& msg){
    Q_EMIT this->griperState(msg->state);
}


void Ros::cameraInfoCallback(const sensor_msgs::CameraInfo::ConstPtr& msg){

}
void Ros::cameraImageCallback(const sensor_msgs::Image::ConstPtr& msg){
//    QImage *i = new QImage(&msg->data[0],msg->width,msg->height,msg->step,QImage::Format_RGB888);
//    QImage icopy = i->copy(0,0,msg->width,msg->height);
//    if(i!=NULL){
//            if(scenes != NULL)
//            {
//                scenes->setSceneRect(0,0,msg->width,msg->height);
//                image_camera.setCoroware_gui::Image(icopy);
//            }

//            if(scenes_all_cam !=NULL){
//                *i=i->scaled(320,240);
//                bottom_left.setImage(*i);
//            }Mode
//            Q_EMIT update_alvoid Pan_control(int value);lcam()gpsCoroware_gui::;

//        }gpsCoroware_gui::
//    delete i;
}

void Ros::rear_camCallback(const sensor_msgs::Image::ConstPtr& msg){
    QImage *i = new QImage(&msg->data[0],msg->width,msg->height,msg->step,QImage::Format_RGB888);
    QImage icopy = i->copy(0,0,msg->width,msg->height);

    if(i!=NULL){
            if(scenes_rear_cam != NULL)
            {
                //scenes_rear_cam->setSceneRect(0,0,msg->width,msg->height);
                scenes_rear_cam->setSceneRect(0,0,640,480);
                Q_EMIT update_rearcam(icopy); //we can't allow a thread to use the scene for the zoom and another one to modify the image as it would crash, so we do everything in one thread
            }
        }
    delete i;
}

void Ros::rear_camCallback_compressed(const sensor_msgs::CompressedImage::ConstPtr& msg){
   // QImage *i = new QImage(&msg->data[0],msg->width,msg->height,msg->step,QImage::Format_RGB888);
   // QImage icopy = i->copy(0,0,msg->width,msg->height);

    QImage *i = new QImage();
    i->loadFromData(&msg->data[0],msg->data.size(),"JPEG");
    //icopy = i->copy(0,0,msg->width,msg->height);
    QImage icopy = i->copy(0,0,640,480);
    if(i!=NULL){
            if(scenes_rear_cam != NULL)
            {
                //scenes_rear_cam->setSceneRect(0,0,msg->width,msg->height);
                scenes_rear_cam->setSceneRect(0,0,640,480);
                Q_EMIT update_rearcam(icopy); //we can't allow a thread to use the scene for the zoom and another one to modify the image as it would crash, so we do everything in one thread
            }
        }
    delete i;
}

void Ros::ptz_camCallback(const sensor_msgs::Image::ConstPtr& msg){
    QImage *i = new QImage(&msg->data[0],msg->width,msg->height,msg->step,QImage::Format_RGB888);
    QImage icopy = i->copy(0,0,msg->width,msg->height);

    if(i!=NULL){
            if(scenes_ptz_cam != NULL)
            {
                //scenes_ptz_cam->setSceneRect(0,0,msg->width,msg->height);
                scenes_ptz_cam->setSceneRect(0,0,640,480);
                Q_EMIT update_ptzcam(icopy); //we can't allow a thread to use the scene for the zoom and another one to modify the image as it would crash, so we do everything in one thread
            }

        }
    delete i;
}

void Ros::ptz_camCallback_compressed(const sensor_msgs::CompressedImage::ConstPtr& msg){
  //  QImage *i = new QImage(&msg->data[0],msg->width,msg->height,msg->step,QImage::Format_RGB888);
   // QImage icopy = i->copy(0,0,msg->width,msg->height);

    QImage *i = new QImage();
    i->loadFromData(&msg->data[0],msg->data.size(),"JPEG");
   // icopy = i->copy(0,0,msg->width,msg->height);
    QImage icopy = i->copy(0,0,640,480);
    if(i!=NULL){
            if(scenes_ptz_cam != NULL)
            {
                //scenes_ptz_cam->setSceneRect(0,0,msg->width,msg->height);
                scenes_ptz_cam->setSceneRect(0,0,640,480);
             	Q_EMIT update_ptzcam(icopy); //we can't allow a thread to use the scene for the zoom and another one to modify the image as it would crash, so we do everything in one thread
            }
        }
    delete i;
}

void Ros::mapCallback(const sensor_msgs::CompressedImage::ConstPtr& msg){
    QImage *i = new QImage();
    i->loadFromData(&msg->data[0],msg->data.size(),"JPEG");
    QImage icopy = i->copy(0,0,i->height(),i->width());
    if(i!=NULL){
            if(scenes_map_image != NULL)
            {
          //       scenes_map_image->setSceneRect(0,0,msg->width,msg->height);
                scenes_rear_cam->setSceneRect(0,0,i->height(),i->width());
                Q_EMIT update_mapimage(icopy); //we can't allow a thread to use the scene for the zoom and another one to modify the image as it would crash, so we do everything in one thread
            }
        }
    delete i;
}

void Ros::gpsCallback(const sensor_msgs::NavSatFix::ConstPtr& msg){
    double lat = msg->latitude;
    double lon = msg->longitude;

    //ROS_INFO("GUI has received latitude =  %f  and  longtitude =  %f \n",lat,lon);

    //if(msg->status.status != 0)
    //{
        Q_EMIT gps_lat(lat);
        Q_EMIT gps_lon(lon);
        Q_EMIT gps_coord(lat,lon);
    //}
}
void Ros::scanCallback(const sensor_msgs::LaserScan::ConstPtr& msg){

//convert from (angle,range) into (x,y)

    //vector<qrk::Point<long> > points;

      int index = 0;
/*      for (vector<float>::const_iterator it = 44;/*msg->ranges.begin();*/
           //it != 726;/*msg->ranges.end();*/ ++it, ++index) */

      for(int it = 44; it<726; it++)
      {
//        long distance = msg->ranges[it];
//        if ((distance <= msg->range_min) || (distance >= msg->range_max)) {
//          continue;
//        }

//        double radian = msg->angle_min +msg->angle_increment*index  + M_PI/2;
//        long x = static_cast<long>(distance * cos(radian));
//        long y = static_cast<long>(distance * sin(radian));

//        points.push_back(qrk::Point<long>(x, y));

          hokuyo_points_[it-44].distance = msg->ranges[it];
          hokuyo_points_[it-44].angle_step = msg->angle_increment;
          hokuyo_points_[it-44].index = it - 44;

          double radian = (index - 384 + 44) * msg->angle_increment;
                  //msg->angle_min +msg->angle_increment*it;// + M_PI/2;
          //cos (radian);
          //sin (radian);

          hokuyo_points_[it-44].x = -(hokuyo_points_[it-44].distance * sin (radian)); //(long)(distance* cos (radian));
          hokuyo_points_[it-44].y = hokuyo_points_[it-44].distance * cos (radian); //(long)(distance* sin (radian));

          index ++;

      }
      //ROS_INFO("Recieved one set of /scan data!! %d", index );
      //rangeWidget.setUrgData(points,msg->header.stamp.toSec());
      //rangeWidget2.setUrgData(points,msg->header.stamp.toSec());

      Q_EMIT hokuyo_update(hokuyo_points_);

}

void Ros::ssc32infoCallback(const corobot_msgs::ssc32_info::ConstPtr &msg)
{
    if(msg->connected == 1)
        ROS_INFO("SSC32 is successfully connectted");
    else ROS_INFO("SSC32 failed to connect");

}

void Ros::phidgetinfoCallback(const corobot_msgs::phidget_info::ConstPtr &msg)
{
    if(msg->phidget888_connected == 1)
        ROS_INFO("phidget 888 is successfully connectted");
    else ROS_INFO("phidget 888 failed to connect");

    if(msg->phidget_encoder_connected == 1)
        ROS_INFO("phidget encoder is successfully connectted");
    else ROS_INFO("phidget encoder failed to connect");
}

void Ros::takepicCallback(const corobot_msgs::takepic::ConstPtr &msg)
{
    if(msg->take)
    {
        image_ptz_cam.saveImage();
    }
}

void Ros::run(){
    ros::spin();
}

void Ros::spatialCallback(const corobot_msgs::spatial::ConstPtr &msg)
{
    double acc_x, acc_y, acc_z, ang_x, ang_y, ang_z, mag_x, mag_y, mag_z;

    acc_x = (double)msg->acc1;
    acc_y = (double)msg->acc2;
    acc_z = (double)msg->acc3;
    ang_x = (double)msg->ang1;
    ang_y = (double)msg->ang2;
    ang_z = (double)msg->ang3;
    mag_x = (double)msg->mag1;
    mag_y = (double)msg->mag2;
    mag_z = (double)msg->mag3;

    Q_EMIT spatial_data(acc_x,acc_y,acc_z,ang_x,ang_y,ang_z,mag_x,mag_y,mag_z);
}

void Ros::moveGripper(bool state){
    if(state)
        Ros::openGripper();
    else
        Ros::closeGripper();
}

void Ros::add_camera_info_scene(QGraphicsScene * scene){
//        scenes = scene;
//        scenes->setSceneRect(0,0,640,480);
//        image_camera.setPos(0,0);
//        scenes->addItem(&image_camera);
}


void Ros::add_rear_cam_scene(QGraphicsScene * scene){
        scenes_rear_cam = scene;
        scenes_rear_cam->setSceneRect(0,0,640,480);
        image_rear_cam.setPos(0,0);
        scenes_rear_cam->addItem(&image_rear_cam);
}

void Ros::add_map_image_scene(QGraphicsScene * scene){
        scenes_map_image = scene;
        scenes_map_image->setSceneRect(0,0,2048,2048);
        image_map_image.setPos(0,0);
        scenes_map_image->addItem(&image_map_image);
}

void Ros::add_front_image_scene(QGraphicsScene * scene){
        scenes_front_image = scene;
        scenes_front_image->setSceneRect(0,0,640,480);
        image_front_cam.setPos(0,0);
        scenes_front_image->addItem(&image_front_cam);
}

void Ros::add_ptz_cam_scene(QGraphicsScene * scene){
        scenes_ptz_cam = scene;
        scenes_ptz_cam->setSceneRect(0,0,640,480);
        image_ptz_cam.setPos(0,0);
        scenes_ptz_cam->addItem(&image_ptz_cam);
}

void Ros::add_kinect_rgb_scene(QGraphicsScene * scene){
        scenes_kinect_rgb = scene;
        scenes_kinect_rgb->setSceneRect(0,0,640,480);
	image_kinect_rgb.setPos(0,0);
        scenes_kinect_rgb->addItem(&image_kinect_rgb);
}



void Ros::add_kinect_depth_scene(QGraphicsScene *scene){
    scenes_kinect_depth = scene;
    scenes_kinect_depth->setSceneRect(0,0,640,480);
    image_kinect_depth.setPos(0,0);
    scenes_kinect_depth->addItem(&image_kinect_depth);

}





void Ros::corobot(bool value){
    Corobot = value;
}


bool Ros::setOdom(float x, float y){
    corobot_srvs::SetOdom srv1;

    srv1.request.px = x;
    srv1.request.py = y;

    return(this->setOdom_client.call(srv1) && srv1.response.err ==0);
}




void Ros::turnWrist(float angle){

	if(moveArm_pub)
	{
	    corobot_msgs::MoveArm msg;

	    /* Do some angle conversions. First we add PI/2 because the default position is actually at angle Pi/2. 
	       Then we make sure that the angle is not too high or too low
		Finally we convert to a value in degrees
	    */
	    double temp = -angle + M_PI/2;
	    if  (-angle > M_PI)
		temp =  - 2*M_PI - angle + M_PI/2;
	    if (temp < 0)
		temp = 2*M_PI + temp;
	    if(0 < temp || temp > M_PI)
		temp = (double)((int)(temp * 180/M_PI) % 180); // execute temp % M_PI, the conversion to int in degree is necessary as the modulus works with integers.


		//temporarily move both the flex and rotation see the GUI doesn't do the difference
	    msg.index = msg.WRIST_FLEX;
	    msg.position = temp;

	    moveArm_pub.publish(msg);

	    msg.index = msg.WRIST_ROTATION;
	    msg.position = temp;

	    moveArm_pub.publish(msg);
	}
}

void Ros::openGripper(){

	if(moveArm_pub)
	{
		corobot_msgs::MoveArm msg;

		msg.index = msg.GRIPPER;
	    	msg.position = 0;

	    	moveArm_pub.publish(msg);
	}
}

void Ros::closeGripper(){

	if(moveArm_pub)
	{
		corobot_msgs::MoveArm msg;

		msg.index = msg.GRIPPER;
	    	msg.position = 180;

	    	moveArm_pub.publish(msg);
	}
}


bool Ros::setCameraControl(int id, int value){
 /*    dynamic_uvc_cam::sResetOdometcontrol srv1;
     srv1.request.id = id;
     srv1.request.value = value;

     return(this->setCameraControl_client.call(srv1));*/
    return true;
}


bool Ros::setCameraState(bool state){
  /*  dynamic_uvc_cam::state msg1;
    if(state)
        msg1.state = "start";
    else
        msg1.state = "stop";

    this->cameraStatePub.publish(msg1);

    return true;*/return true;
}

bool Ros::setCameraMode(int width, int weight, bool immediately, int fps, bool auto_exposure)
{

  /*  dynamic_uvc_cam::videomode msg1;
     msg1.width = width;
     msg1.height = weight;
     msg1.immediately = immediately;
     msg1.fps = fps;
     msg1.auto_exposure = auto_exposure;


     this->cameraModePub.publish(msg1);
     return true;*/
    return true;
}

void Ros::timerCallback(const ros::TimerEvent& event)
{
        corobot_msgs::MotorCommand msg;

        msg.leftSpeed = speed_left;
        msg.rightSpeed = speed_right;
        msg.secondsDuration = 3;
        msg.acceleration = 50;

        driveControl_pub.publish(msg);
 
}

//Decrease speed when no robot movement buttons or key is pressed
bool Ros::decrease_speed()
{
    if(driveControl_pub)
    {

        corobot_msgs::MotorCommand msg;

        if(turningLeft)
        {
            speed_left = -speed_value;
            speed_right = speed_value;
        }
        else if(turningRight)
        {
            speed_left = speed_value;
            speed_right = -speed_value;
        }
        else
        {
            speed_left = 0;
            speed_right = 0;
        }

        msg.leftSpeed = speed_left;
        msg.rightSpeed = speed_right;
        msg.secondsDuration = 3;
        msg.acceleration = 50;

        driveControl_pub.publish(msg);

    }
    return true;
}

//Increase speed when the move forward button or key is pressed
bool Ros::increase_speed()
{

    corobot_msgs::MotorCommand msg;
	if(driveControl_pub)
	{
			speed_left += speed_value;
			speed_right += speed_value;

			if(speed_left >speed_value)
				speed_left = speed_value;
			if(speed_right >speed_value)
				speed_right = speed_value;
			msg.leftSpeed = speed_left;
			msg.rightSpeed = speed_right;
			msg.secondsDuration = 3;
			msg.acceleration = 50;


		    driveControl_pub.publish(msg);
		return true;
	}
	else
		return false;
}

//Increase speed when the move backward button or key is pressed
bool Ros::increase_backward_speed()
{
    corobot_msgs::MotorCommand msg;
	if(driveControl_pub)
	{
		speed_left -= speed_value;
		speed_right -= speed_value;

		if(speed_left <-speed_value)
			speed_left = -speed_value;
		if(speed_right <-speed_value)
			speed_right = -speed_value;

		msg.leftSpeed = speed_left;
		msg.rightSpeed = speed_right;
		msg.secondsDuration = 3;
		msg.acceleration = 50;

		driveControl_pub.publish(msg);
		return true;
	}
	else
		return false;
}

bool Ros::turn_left()
{
    corobot_msgs::MotorCommand msg;

	if(driveControl_pub)
	{
		if((speed_left+speed_right)<0)
		{
		    speed_left += speed_value;
		    speed_right -= speed_value;
		}
		else
		{
		    speed_left -= speed_value;
		    speed_right += speed_value;
		}

		if(speed_left >speed_value)
		    speed_left = speed_value;
		if(speed_right >speed_value)
		    speed_right = speed_value;
		if(speed_left <-speed_value)
		    speed_left = -speed_value;
		if(speed_right <-speed_value)
		    speed_right = -speed_value;


		msg.leftSpeed = speed_left;
		msg.rightSpeed = speed_right;
		msg.secondsDuration = 3;
		msg.acceleration = 50;

		driveControl_pub.publish(msg);
		turningLeft = true;
		turningRight = false;
		return true;
	}
	else
		return false;
}

bool Ros::turn_right()
{
    corobot_msgs::MotorCommand msg;

	if(driveControl_pub)
	{
		if((speed_left+speed_right)<0)
		{
		    speed_left -= speed_value;
		    speed_right += speed_value;
		}
		else
		{
		    speed_left += speed_value;
		    speed_right -= speed_value;
		}
		if(speed_left >speed_value)
		    speed_left = speed_value;
		if(speed_right >speed_value)
		    speed_right = speed_value;
		if(speed_left <-speed_value)
		    speed_left = -speed_value;
		if(speed_right <-speed_value)
		    speed_right = -speed_value;

		msg.leftSpeed = speed_left;
		msg.rightSpeed = speed_right;
		msg.secondsDuration = 3;
		msg.acceleration = 50;

		driveControl_pub.publish(msg);
		turningRight = true;
		turningLeft = false;
		return true;
	}
	else
		return false;
}

bool Ros::stop_turn()
{
    corobot_msgs::MotorCommand msg;

	if(driveControl_pub)
	{	
		if((speed_left+speed_right)<0)
		{
			speed_left = -speed_value;
			speed_right = -speed_value;
		}
		else if((speed_left+speed_right)>0)
		{
			speed_left = +speed_value;
			speed_right = +speed_value;
		}
		else
		{
			speed_left = 0;
			speed_right = 0;
		}
	
		msg.leftSpeed = speed_left;
		msg.rightSpeed = speed_right;
		msg.secondsDuration = 3;
		msg.acceleration = 50;

		driveControl_pub.publish(msg);
		turningLeft = false;
		turningRight = false;
		return true;
	}
	else
		return false;
}


bool Ros::motor_stop()
{
    corobot_msgs::MotorCommand msg;

	if(driveControl_pub)
	{
		msg.leftSpeed = 0;
		msg.rightSpeed = 0;
		msg.secondsDuration = 0;
		msg.acceleration = 50;
		driveControl_pub.publish(msg);
		return true;
	}
	else
		return false;
}


void Ros::moveShoulderArm(double shoulder)
{
	if(moveArm_pub)
	{
		corobot_msgs::MoveArm msg;

		msg.index = msg.SHOULDER;
	    	msg.position = shoulder *180/M_PI; //convertion from radian to degree

	    	moveArm_pub.publish(msg);
	}
}

void Ros::moveElbowArm(double elbow)
{
	if(moveArm_pub)
	{
		corobot_msgs::MoveArm msg;

		msg.index = msg.ELBOW;
	    	msg.position = elbow *180/M_PI; //convertion from radian to degree

	    	moveArm_pub.publish(msg);
	}
}

void Ros::rotateArm(double angle)
{
	if(moveArm_pub)
	{
		corobot_msgs::MoveArm msg;

		msg.index = msg.BASE_ROTATION;
	    	msg.position = angle *181/M_PI; //convertion from radian to degree

	    	moveArm_pub.publish(msg);
	}
}

void Ros::ResetArm()
{
	if(moveArm_pub)
	{
		corobot_msgs::MoveArm msg;

		msg.index = msg.SHOULDER;
	    	msg.position = 0;
	    	moveArm_pub.publish(msg);

		msg.index = msg.ELBOW;
	    	msg.position = 0; 
	    	moveArm_pub.publish(msg);

		msg.index = msg.WRIST_FLEX;
	    	msg.position = 90; 
	    	moveArm_pub.publish(msg);

		msg.index = msg.WRIST_ROTATION;
	    	msg.position = 90; 
	    	moveArm_pub.publish(msg);

		msg.index = msg.GRIPPER;
	    	msg.position = 0; 
	    	moveArm_pub.publish(msg);

		msg.index = msg.BASE_ROTATION;
	    	msg.position = 90; 
	    	moveArm_pub.publish(msg);
	}
}
