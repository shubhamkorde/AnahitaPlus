#include <depth_stabilise.h>

depthStabilise::depthStabilise() : anglePIDClient("turnPID"), upwardPIDClient("upwardPID") {
    goalReceived = false;
    sub_ = nh.subscribe("/anahita/z_coordinate", 1, &depthStabilise::depthCB, this);
}

depthStabilise::~depthStabilise() {
}

void depthStabilise::setActive(bool status) {
    if (status) {
        spin_thread = new boost::thread(boost::bind(&depthStabilise::spinThread, this));
    }
    else {
        depth_mutex.lock();
        if (goalReceived) {
            anglePIDClient.cancelGoal();
        }
        depth_mutex.unlock();
        
        mtx.lock();
        close_loop = true;
        mtx.unlock();

        ROS_INFO("Depth Stabilise Server goal cancelled");
        spin_thread->join();
        nh.setParam("/kill_signal", true);
    }
}   

void depthStabilise::depthCB(const std_msgs::Float32Ptr &_msg) {
    depth_mutex.lock();
    depth = _msg->data;
    goalReceived = true;
    depth_mutex.unlock();
}

void depthStabilise::spinThread() {

    ROS_INFO("Waiting for turnPID server to start.");
    anglePIDClient.waitForServer();
    ROS_INFO("Server waiting compelted");
    ROS_INFO("turnPID server started, sending goal.");
    angle_PID_goal.target_angle = 0;
    anglePIDClient.sendGoal(angle_PID_goal);

    ROS_INFO("Waiting for upwardPID server to start, depth stabilise");
    upwardPIDClient.waitForServer();

    double then = ros::Time::now().toSec();
    while (ros::ok()) {
        double now = ros::Time::now().toSec();
        bool temp = false;

        mtx.lock();
        temp = close_loop;
        mtx.unlock();
        if (temp) { break; }

        depth_mutex.lock();
        temp = goalReceived;
        depth_mutex.unlock();
        if (temp) { break; }

        if (now - then > 5) { break; }
    }

    depth_mutex.lock();
    if (goalReceived) { 
        ROS_INFO("upwardPID server started, sending goal.");
        upward_PID_goal.target_depth = depth;
        upwardPIDClient.sendGoal(upward_PID_goal);        
        ROS_INFO("Sent the upward goal to client, depth stabilise");
    }
    depth_mutex.lock();
}