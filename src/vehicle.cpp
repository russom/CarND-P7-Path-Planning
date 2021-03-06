#include "vehicle.h"
#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include "spline.h"

using std::string;
using std::vector;
using namespace std;

/**
 * Constructor
 */
Vehicle::Vehicle(){}

/**
 * Constructor with parameters
 *
 * @param lane = id for the lane occupied by the vehicle
 * @param s = longitudinal Frenet coordinates for the vehicle
 * @param d = transverse Frenet coordinates for the vehicle
 * @param v = speed of the vehicle (in m/s)
 * @param a = acceleration of the vehicle
 * @param x = x-coordinate of the vehicle
 * @param y = y-coordinate of the vehicle
 * @param state = state of the vehicle (default = "CS")
 */
Vehicle::Vehicle(int v_lane, float s, float d, float v, float a, float x, float y, float yaw, string state) {
  this->lane = v_lane;
  this->s = s;
  this->d = d;
  this->v = v;
  this->a = a;
  this->x = x;
  this->y = y;
  this->yaw = yaw;
  this->state = state;
}

/**
 * Destructor
 */
Vehicle::~Vehicle() {}

/**
 * Generate a Trajectory
 *
 * @param next_vals_x = vector of x-coordinates for the GENERATED trajectory
 * @param next_vals_y = vector of y-coordinates for the GENERATED trajectory
 * @param previous_x_path = vector of x-coordinates for the PREVIOUS trajectory
 * @param previous_y_path = vector of y-coordinates for the PREVIOUS trajectory
 * @param map_s_waypoints = vector of reference waypoints in Frenet s coordinates
 * @param map_x_waypoints = vector of reference waypoints in Cartesian x coordinates
 * @param map_y_waypoints = vector of reference waypoints in Cartesian y coordinates
 * @param r_vel = reference speed of the vehicle (in mph)
 * @param target_lane = id for the target lane
 */
void Vehicle::generateXYTrajectory(vector<double> &next_vals_x, vector<double> &next_vals_y, vector<double> &previous_x_path, vector<double> &previous_y_path,
                        const vector<double> &map_s_waypoints, const vector<double> &map_x_waypoints, const vector<double> &map_y_waypoints,
                        double r_vel, int target_lane) {

  double car_x = this->x;
  double car_y = this->y;
  double car_s = this->s;
  double car_yaw = this->yaw;

  // Initialize  trajectory with previous path
  for (int i = 0; i < previous_x_path.size(); ++i) {
    next_vals_x.push_back(previous_x_path[i]);
    next_vals_y.push_back(previous_y_path[i]);
  }

  // generate anchor points

  // vectors of sparse points to intepolate using spline
  vector<double> ptsx;
  vector<double> ptsy;

  // reference state for the car
  double ref_x = car_x;
  double ref_y = car_y;
  double ref_yaw = deg2rad(car_yaw);

    // past points
    if (previous_x_path.size()<2){
        ptsx.push_back(car_x);
        ptsy.push_back(car_y);
    }
    else {
        ref_x = previous_x_path[previous_x_path.size() - 1];
        ref_y = previous_y_path[previous_x_path.size() - 1];

        double ref_x_prev = previous_x_path[previous_x_path.size() - 2];
        double ref_y_prev = previous_y_path[previous_x_path.size() - 2];
        ref_yaw = atan2(ref_y - ref_y_prev,ref_x - ref_x_prev);

        ptsx.push_back(ref_x_prev);
        ptsx.push_back(ref_x);

        ptsy.push_back(ref_y_prev);
        ptsy.push_back(ref_y);
    }

  // future points
  vector<double> next_wp0 = getXY(car_s + TRAJ_HORIZ_1,((LANE_WIDTH/2)+LANE_WIDTH*target_lane),map_s_waypoints,map_x_waypoints,map_y_waypoints);
  vector<double> next_wp1 = getXY(car_s + TRAJ_HORIZ_2,((LANE_WIDTH/2)+LANE_WIDTH*target_lane),map_s_waypoints,map_x_waypoints,map_y_waypoints);
  vector<double> next_wp2 = getXY(car_s + TRAJ_HORIZ_3,((LANE_WIDTH/2)+LANE_WIDTH*target_lane),map_s_waypoints,map_x_waypoints,map_y_waypoints);

  ptsx.push_back(next_wp0[0]);
  ptsx.push_back(next_wp1[0]);
  ptsx.push_back(next_wp2[0]);

  ptsy.push_back(next_wp0[1]);
  ptsy.push_back(next_wp1[1]);
  ptsy.push_back(next_wp2[1]);

  // shift in car's ref frame
  for(int i = 0; i<ptsx.size(); i++){
    double shift_x = ptsx[i] - ref_x;
    double shift_y = ptsy[i] - ref_y;

    ptsx[i] = (shift_x * cos(0 - ref_yaw) - shift_y * sin(0 - ref_yaw));
    ptsy[i] = (shift_x * sin(0 - ref_yaw) + shift_y * cos(0 - ref_yaw));
  }

  // generate a spline
  tk::spline s;
  s.set_points(ptsx,ptsy);

  //respace according to target vel
  double target_x = TRAJ_HORIZ_1;
  double target_y = s(target_x);
  double target_distance = distance(0,0,target_x,target_y);

  double x_add_on = 0;

  // add them to path
  for (int i = 1; i <= 50 - previous_x_path.size(); i++){

    double N = target_distance/(DELTA_T*r_vel*MPH2MS);
    double x_point = x_add_on + target_x/N;
    double y_point = s(x_point);

    x_add_on = x_point;

    double x_ref = x_point;
    double y_ref = y_point;

    x_point = (x_ref * cos(ref_yaw) - y_ref * sin(ref_yaw));
    y_point = (x_ref * sin(ref_yaw) + y_ref * cos(ref_yaw));

    x_point += ref_x;
    y_point += ref_y;

    next_vals_x.push_back(x_point);
    next_vals_y.push_back(y_point);
  }
}


/**
 * Implement a Trajectory
 *
 * @param vehicles = map of non-ego vehicles
 * @param predictions = map of predicted trajectories for non-ego vehicles
 * @param next_vals_x = vector of x-coordinates for the GENERATED trajectory
 * @param next_vals_y = vector of y-coordinates for the GENERATED trajectory
 * @param previous_x_path = vector of x-coordinates for the PREVIOUS trajectory
 * @param previous_y_path = vector of y-coordinates for the PREVIOUS trajectory
 * @param map_s_waypoints = vector of reference waypoints in Frenet s coordinates
 * @param map_x_waypoints = vector of reference waypoints in Cartesian x coordinates
 * @param map_y_waypoints = vector of reference waypoints in Cartesian y coordinates
 * @param r_vel = reference speed of the vehicle (in mph)
 * @param current_lane = id for the current lane
 * @param init_acc_over = boolean flag that indicates the termination of the intial acceleration phase
 */
void Vehicle::implementNextTrajectory(map<int, Vehicle> &vehicles, map<int ,vector<Vehicle> > &predictions, vector<double> &next_vals_x,
                                      vector<double> &next_vals_y, vector<double> &previous_x_path,
                                      vector<double> &previous_y_path, const vector<double> &map_s_waypoints,
                                      const vector<double> &map_x_waypoints,
                                      const vector<double> &map_y_waypoints, double &r_vel, int current_lane,
                                      bool &init_acc_over) {

  map<int ,vector<double> > genericTrajsX;
  map<int ,vector<double> > genericTrajsY;

  vector<double> genericTrajX;
  vector<double> genericTrajY;
  vector<double> costs;
  vector<int> target_lanes;

  int num_traj = 0;
  int target_lane = current_lane;

  double current_KL_vel = r_vel;
  double current_LCL_vel = r_vel;
  double current_LCR_vel = r_vel;

  bool coll_event_LCL;
  bool coll_event_LCR;

  vector<string> states = successorStates();

  for (vector<string>::iterator it = states.begin(); it != states.end(); ++it) {
    if (it->compare("KL") == 0)
    {
      // Trajectory for Keep Lane case
      // std::cout<<"KL Traj"<<std::endl;  // Uncomment for printout/debug

      genericTrajX.clear();
      genericTrajY.clear();

      // Check if speed needs changing (possible collision ahead)
      // NOTE - for KL state target_lane = current_lane
      regulateVelocity(vehicles, current_KL_vel, current_lane, current_lane, previous_x_path, init_acc_over);

      // Generate a trajectory
      generateXYTrajectory(genericTrajX, genericTrajY, previous_x_path, previous_y_path,
                           map_s_waypoints, map_x_waypoints, map_y_waypoints, current_KL_vel, current_lane);


      genericTrajsX.insert(std::pair<int, vector<double>>(num_traj, genericTrajX));
      genericTrajsY.insert(std::pair<int, vector<double>>(num_traj, genericTrajY));

      num_traj += 1;

      // cost is defined so to prefer KL to LCL to LCR
      // KL is penalized if speed has to be reduced (possible collision ahead)
      if (current_KL_vel < r_vel){
        costs.push_back(1.0);
      } else{
        costs.push_back(0.0);
      }

      target_lanes.push_back(current_lane);
    }
    else if (it->compare("LCL") ==0)
    {
      // Trajectory for Lane Change Left case
      // std::cout<<"LCRL Traj"<<std::endl;  // Uncomment for printout/debug
      genericTrajX.clear();
      genericTrajY.clear();

      if (current_lane>0) {
        target_lane = current_lane - 1;

        // Check if speed needs changing (possible collision ahead)
        regulateVelocity(vehicles, current_LCL_vel, current_lane, target_lane, previous_x_path, init_acc_over);

        // Generate a trajectory
        generateXYTrajectory(genericTrajX, genericTrajY, previous_x_path, previous_y_path,
                             map_s_waypoints, map_x_waypoints, map_y_waypoints, current_LCL_vel, target_lane);
      }

      genericTrajsX.insert(std::pair<int, vector<double>>(num_traj, genericTrajX));
      genericTrajsY.insert(std::pair<int, vector<double>>(num_traj, genericTrajY));

      num_traj += 1;

      float min_dist_LCL = 999999.9;  // Initialize to big number
      float dist_LCL = 0.0;
      // Reference distance calculated as a function of the velocity of the Ego vehicle (r_vel)
      float ref_dist_LCL = (REF_SPEED/std::max(r_vel,0.1))*REF_DIST_LC;

      // loop over predictions to check collision
      int j = 0;
      coll_event_LCL = false;

      while ((coll_event_LCL == false) && (j < predictions.size())){

        // check if the vehicle considered is in the actual or target lane
        if ((predictions[j].at(0).lane == current_lane) || (predictions[j].at(0).lane == target_lane)){

          int i = 0;
          // loop over predicted and generated trajectories until a collision is found
          while ((coll_event_LCL == false) && (i < genericTrajX.size())){

            int k = 0;

            while ((coll_event_LCL == false) && (k < predictions[j].size())) {

              dist_LCL = distance(genericTrajX[i], genericTrajY[i], predictions[j].at(k).x, predictions[j].at(k).y);

              coll_event_LCL = (dist_LCL < ref_dist_LCL);
              if (dist_LCL < min_dist_LCL) {
                min_dist_LCL = dist_LCL;
              }
              k += 1;
            }
            i += 1;
          }
        }
        j += 1;
      }

      // Assign cost
      if (coll_event_LCL){
        // std::cout<<"LCL Collision Detected"<<std::endl; // Uncomment for printout/debug
        costs.push_back(1.1);
      } else {
        costs.push_back(0.1);
      }

      target_lanes.push_back(target_lane);
    }
    else if (it->compare("LCR") == 0)
    {
      // Trajectory for Lane Change Right case
      // std::cout<<"LCR Traj"<<std::endl; // Uncomment for printout/debug
      genericTrajX.clear();
      genericTrajY.clear();
      
      if (current_lane<2){
        target_lane = current_lane + 1;

        // Check if speed needs changing (possible collision ahead)
        regulateVelocity(vehicles, current_LCR_vel, current_lane, target_lane, previous_x_path, init_acc_over);

        // Generate a trajectory
        generateXYTrajectory(genericTrajX, genericTrajY, previous_x_path, previous_y_path,
                             map_s_waypoints, map_x_waypoints, map_y_waypoints, current_LCR_vel, target_lane);
      }

      genericTrajsX.insert(std::pair<int, vector<double>>(num_traj, genericTrajX));
      genericTrajsY.insert(std::pair<int, vector<double>>(num_traj, genericTrajY));

      num_traj += 1;

      float min_dist_LCR = 999999.9;  // Initialize to big number
      float dist_LCR= 0.0;
      // Reference distance calculated as a function of the velocity of the Ego vehicle (r_vel)
      float ref_dist_LCR = (REF_SPEED/std::max(r_vel,0.1))*REF_DIST_LC;

      // loop over predictions to check collision
      int j = 0;
      coll_event_LCR = false;

      while ((coll_event_LCR == false) && (j < predictions.size())){

        // check if the vehicle considered is in the actual or target lane
        if ((predictions[j].at(0).lane == current_lane) || (predictions[j].at(0).lane == target_lane)){

          int i = 0;
          // loop over predicted and generated trajectories until a collision is found
          while ((coll_event_LCR == false) && (i < genericTrajX.size())){

            int k = 0;

            while ((coll_event_LCR == false) && (k < predictions[j].size())) {

              dist_LCR = distance(genericTrajX[i], genericTrajY[i], predictions[j].at(k).x, predictions[j].at(k).y);

              coll_event_LCR = (dist_LCR < ref_dist_LCR);
              if (dist_LCR < min_dist_LCR) {
                min_dist_LCR = dist_LCR;
              }
              k += 1;
            }
            i += 1;
          }
        }
        j += 1;
      }

      // Assign cost
      if (coll_event_LCR){
        // std::cout<<"LCR Collision Detected"<<std::endl;  // Uncomment for printout/debug
        costs.push_back(1.2);
      } else{
        costs.push_back(0.2);
      }

      target_lanes.push_back(target_lane);
    }
  }

  // find lower cost
  int min_cost_index = min_element(costs.begin(), costs.end()) - costs.begin();

  next_vals_x = genericTrajsX.at(min_cost_index);
  next_vals_y = genericTrajsY.at(min_cost_index);

  this->state = states.at(min_cost_index);
  this->goal_lane = target_lanes.at(min_cost_index);

  if (this->state == "KL"){
    r_vel = current_KL_vel;
  }
  else if (this->state == "LCL"){
    r_vel = current_LCL_vel;
  }
  else if (this->state == "LCR"){
    r_vel = current_LCR_vel;
  }
}


/**
 * Regulate Velocity
 *
 * @param vehicles = map of non-ego vehicles
 * @param ref_vel = reference speed of the vehicle (in mph)
 * @param current_lane = id of current lane where to check the position of other vehicles
 * @param target_lane = id of target lane where to check the position of other vehicles
 * @param previous_path_x = vector of x-coordinates for the PREVIOUS trajectory
 * @param init_acc_over = boolean flag that indicates the termination of the intial acceleration phase
 */
void Vehicle::regulateVelocity(map<int, Vehicle> &vehicles, double &ref_vel, int current_lane, int target_lane,
                               vector<double> &previous_path_x, bool &init_acc_over) {

  // First of all let's check if there's a chance of getting too close
  // to other cars while keeping this lane, and adapt velocity
  bool too_close = false;
  bool emergency_brake = false;

  for (map<int, Vehicle>::iterator it = vehicles.begin(); it != vehicles.end();
       ++it) {
    // Loop over vehicles
    // to find a vehicle which is in the same lane as ego AND too close

    // Get lane for the vehicle
    int ln = it->second.lane;

    // Check if there's a car in the ego current OR target lane
    if (ln == current_lane || ln == target_lane){
      // there is a car in the same lane as ego: calculate its velocity
      double check_speed = it->second.v;

      // check its position, starting from current s
      double check_car_s = it->second.s;

      // Where will the car be at the end of the path previously planned
      // considering constant velocity and sampling interval
      check_car_s +=((double)previous_path_x.size()*DELTA_T*check_speed);

      // Compare the distance between this predicted position and the
      // position of the ego. Compare with a given threshold
      if((check_car_s > this->s) && ((check_car_s - this->s) < 30)){

        too_close = true;
      }

      if((check_car_s > this->s) && ((check_car_s - this->s) < 10)){

        emergency_brake = true;
      }
    }
  }

  // If too close slow down
  if (too_close == true){
    if (emergency_brake == true){
      // std::cout<< "EMERGENCY BRAKE"<<std::endl;  // Uncomment for printout/debug
      ref_vel -= EMG_SPEED_CHANGE;
    }
    else{
      // std::cout<< "SLOWING DOWN"<<std::endl;  // Uncomment for printout/debug
      ref_vel -= REF_SPEED_CHANGE;
    }
  }
  else if (ref_vel < REF_SPEED){
    // std::cout<< "ACCELERATING"<<std::endl;  // Uncomment for printout/debug
    ref_vel+= REF_SPEED_CHANGE;
  }
  else{
    // std::cout<< "MAINTAINING SPEED"<<std::endl;  // Uncomment for printout/debug
    if (init_acc_over == false){
      init_acc_over = true;
    }
  }

  if (ref_vel <= 0){
    ref_vel = 0.0;
  }
}


/**
 * Define possible successor states, based on Finite State Machine for Ego Vehicle
 * Possible states = "KL' (Keep Lane)
 *                   "LCR" (Lane Change Right)
 *                   "LCL" (Lane Change Left)
 *
 * @output successorStates = vector of strings isentifying the possible successor states
 */
vector<string> Vehicle::successorStates() {
  // Provides the possible next states given the current state for the FSM
  vector<string> states;

  string state = this->state;

  if(state.compare("KL") == 0) {
    states.push_back("KL");
    if (this->lane != 0) {
      states.push_back("LCL");
    }
    if (this->lane != this->lanes_available - 1) {
      states.push_back("LCR");
    }
  }
  else if (state.compare("LCL") == 0) {
    if (this->lane == this->goal_lane){
      states.push_back("KL");
    }
    if (this->lane != 0) {
      states.push_back("LCL");
    }
  }
  else if (state.compare("LCR") == 0) {
    if (this->lane == this->goal_lane){
      states.push_back("KL");
    }
    if (this->lane != this->lanes_available - 1) {
      states.push_back("LCR");
    }
  }

  return states;
}


/**
  * Generate predicted trajectory for non-Ego vehicle
  *
  * @param map_s_waypoints = vector of reference waypoints in Frenet s coordinates
  * @param map_x_waypoints = vector of reference waypoints in Cartesian x coordinates
  * @param map_y_waypoints = vector of reference waypoints in Cartesian y coordinates
  * @param map_y_length = length of the trajectory (num. of samples to propagate)
  *
  * @output generatePredictions = vector of vehicles representing the trajectory
  */
vector<Vehicle> Vehicle::generatePredictions(const vector<double> &map_s_waypoints, const vector<double> &map_x_waypoints,
   const vector<double> &map_y_waypoints, int pred_size) {
  // Generates predictions for non-ego vehicles to be used in trajectory generation for the ego vehicle.
  // Hyp: constant speed for the length of the trajectory

  vector<Vehicle> predictions;
  float curr_s = this->s;
  for(int i = 0; i < pred_size; ++i) {
    float next_s = curr_s + this->v * DELTA_T;

    // NOTE: Yaw is not considered for these trajectories
    vector<double> next_xy = getXY(next_s,this->d,map_s_waypoints,map_x_waypoints,map_y_waypoints);

    predictions.push_back(Vehicle(this->lane, next_s, this->d, this->v, 0, next_xy[0], next_xy[1],-1,"CS"));

    curr_s = next_s;
  }

  return predictions;
}

