/* copyright[2019] <msl>
**************************************************************************
  File Name    : pose_track_controller.cpp
  Author       : Kunal Shah
                 Multi-Robot Systems Lab (MSL), Stanford University
  Contact      : k2shah@stanford.edu
  Create Time  : Dec 3, 2018.
  Description  : Basic control + pose tracking
**************************************************************************/

#include<mslquad/traj_track_controller.h>
#include<iostream>
#include <fstream>

TrajTrackController::TrajTrackController() {
    // retrieve ROS parameter
    // load traj from topic or file
    ros::param::param<bool>("load_traj_file", loadTrajFile, false);

    if (loadTrajFile) {
        std::string trajFile;
        ros::param::param<std::string>("traj_file", trajFile, "traj.txt");
        parseTrajFile(&trajFile);
    } else {
        std::string trajTargetTopic;
        ros::param::param<std::string>("traj_target_topic",
                                        trajTargetTopic,
                                        "command/trajectory");
        trajTargetSub_ =
            nh_.subscribe<trajectory_msgs::MultiDOFJointTrajectoryPoint>(
            trajTargetTopic, 1, &TrajTrackController::trajTargetCB, this);
        ROS_INFO_STREAM("Subscribed: "<< trajTargetTopic);
    }
    // ROS subs and pub
    scambleSub_ = nh_.subscribe<std_msgs::Bool>(
        "/tower/scramble", 1, &TrajTrackController::scrambleSubCB, this);
}

TrajTrackController::~TrajTrackController() {
}

void TrajTrackController::trajTargetCB(
        const trajectory_msgs::MultiDOFJointTrajectoryPoint::ConstPtr& msg) {
    traj = *msg;
    ROS_INFO_STREAM("Trajectory Received");
}
void TrajTrackController::scrambleSubCB(
        const std_msgs::Bool::ConstPtr& msg) {
    scramble = msg-> data;
}
void TrajTrackController::parseTrajFile(std::string* trajFilePtr) {
        std::ifstream f;
    std::cout << "Opening: " << *trajFilePtr << std::endl;
    f.open(*trajFilePtr);
    if (!f) {
        std::cerr <<
        "ERROR: Unable to open traj file: "<< *trajFilePtr <<std::endl;
    }
    if (f.is_open()) {
        // init vars to parse with
        float px, py, pz;
        float vx, vy, vz;
        // parse the ines
        f >> px >> py >> pz >> vx >> vy >> vz;
        printf("%2.4f, %2.4f, %2.4f: %2.4f, %2.4f, %2.4f", px, py, pz,
                                                           vx, vy, vz);
        // fill traj
        geometry_msgs::Transform tf;
        geometry_msgs::Twist tw;
        // pos
        tf.translation.x = px;
        tf.translation.y = py;
        tf.translation.z = pz;
        // vel
        tw.linear.x = vx;
        tw.linear.y = vy;
        tw.linear.z = vz;
        // push back
        traj.transforms.push_back(tf);
        traj.velocities.push_back(tw);
    }
    // I don't think we can leave this blank
    traj.time_from_start = ros::Duration(1);
}
void TrajTrackController::takeoff(const double desx,
                                  const double desy,
                                  const double desz) {
    Eigen::Vector3d desPos(desx, desy, desz);
    Eigen::Vector3d curPos;
    Eigen::Vector3d desVel;
    double posErr = 1000;
    ros::Rate rate(10);  // change to meet the control loop?
    geometry_msgs::Twist twist;
    std::cout << "Takeoff" << std::endl;
    while (ros::ok()) {
        // take off to starting position
        posErr = calcVelCmd(desVel, desPos, maxVel_, 4.0);
        twist.linear.x = desVel(0);
        twist.linear.y = desVel(1);
        twist.linear.z = desVel(2);
        px4SetVelPub_.publish(twist);
        rate.sleep();
        ros::spinOnce();
        // check for "go" signal
        if (scramble) break;
    }
}
void TrajTrackController::controlLoop(void) {
    // follow the trajectory
    ROS_ERROR("Pose Delay Critical. Landing");
    emergencyLandPose_ = curPose_.pose;
    emergencyLandPose_.position.z = takeoffPose_.pose.position.z;
    state_ = State::EMERGENCY_LAND;
}