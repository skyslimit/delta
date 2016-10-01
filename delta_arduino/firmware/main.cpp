#include <ros.h>
#include <delta_arduino/cmdAngle.h>
#include <delta_arduino/GetInfo.h>
#include <ros/time.h>
#include <ros/duration.h>
#include <sensor_msgs/JointState.h>
#include <std_msgs/String.h>
#include <Arduino.h>
#include <AccelStepper.h>
#include <MultiStepper.h>

#define TRUE                1
#define FALSE               0
#define LED_PIN             13
#define A_STEP_PIN         	54
#define A_DIR_PIN          	55
#define A_ENABLE_PIN       	38
#define A_END_PIN           3
#define B_STEP_PIN         	60
#define B_DIR_PIN          	61
#define B_ENABLE_PIN       	56
#define B_END_PIN          	14
#define C_STEP_PIN        	26
#define C_DIR_PIN          	28
#define C_ENABLE_PIN        24
#define C_END_PIN 	        18

#define EE_RADIUS           30
#define BICEPS              100
#define BASE_RADIUS         80
#define FOREARM             296.71
#define STEPMODE            8

//angle to set robot on top of workspace -15.4315 is for z=-285
#define RESETANGLE          11



static float stepsCircle = 400 * STEPMODE;
bool set_reset = false;
bool enable = false;
float factor =0.28;
double lastTime;
double stepTime;

enum state
{
    STATE_OFF,
    STATE_INIT,
    STATE_RESET,
    STATE_WAITING,
    STATE_MOVING,

}state_;
state oldstate_;

ros::NodeHandle nh;
sensor_msgs::JointState joint_state;
ros::Publisher JointState("delta/joint_state",&joint_state);

AccelStepper 	a_stepper(AccelStepper::DRIVER, A_STEP_PIN, A_DIR_PIN);
AccelStepper 	b_stepper(AccelStepper::DRIVER, B_STEP_PIN, B_DIR_PIN);
AccelStepper 	c_stepper(AccelStepper::DRIVER, C_STEP_PIN, C_DIR_PIN);
MultiStepper  steppers;

void initialize(AccelStepper *stepper, int enable, int end) {

  stepper->setEnablePin(enable);
  stepper->setMinPulseWidth(20);
  stepper->setPinsInverted(false, false, true);
  stepper->setMaxSpeed(stepsCircle/3);
  stepper->setSpeed(stepsCircle*factor*0.9);
  stepper->setAcceleration(20000);
  steppers.addStepper(*stepper);
  pinMode(end, INPUT);
}
void startCtrl(){
  digitalWrite(LED_PIN, HIGH);
  enable = true;
  a_stepper.enableOutputs();
  b_stepper.enableOutputs();
  c_stepper.enableOutputs();
  nh.loginfo("Motor Control enabled");
}
void stopCtrl(){
  digitalWrite(LED_PIN, LOW);
  enable = false;
  a_stepper.disableOutputs();
  b_stepper.disableOutputs();
  c_stepper.disableOutputs();
  nh.loginfo("Motor Control disabled");
}

void checkEndstops(){
  //Funktion stoppt jeweiligen Motor, wenn Endstop erreicht wurde
  //Falls Resetroutine durchgefuehrt werden soll, fahre Kniehebel in vorgegebene Position
  if (digitalRead(A_END_PIN) == LOW){
    a_stepper.setCurrentPosition(0);
    if (set_reset) a_stepper.moveTo(-RESETANGLE*stepsCircle/360);
  }
  if (digitalRead(B_END_PIN) == LOW){
    b_stepper.setCurrentPosition(0);
    if (set_reset) b_stepper.moveTo(-RESETANGLE*stepsCircle/360);
  }
  if (digitalRead(C_END_PIN) == LOW){
    c_stepper.setCurrentPosition(0);
    if (set_reset) c_stepper.moveTo(-RESETANGLE*stepsCircle/360);
  }
}
bool checkWorkspace(float a_angle, float b_angle, float c_angle ){
//Liefert false zurück, falls einer der Motorwinkel nicht im Intervall [0,-90°] liegen sollte
if (a_angle >= -90.0 && b_angle >= -90.0 && c_angle >= -90.0){
    if (a_angle <= 0.0 && b_angle <= 0.0 && c_angle <= 0.0){
      return true;
    }
    else return false;
  }
  else return false;
}

void moveMotorTo(const delta_arduino::cmdAngle& cmdAngle){
//Subscribercallback um Motoren mit vorgegebener Geschwindigkeit an bestimmte Position zu bewegen
  //Verschieben in State Machine?
  if(enable){
    if(state_ == STATE_WAITING || state_ == STATE_MOVING ){

      int cmd_theta1 = cmdAngle.theta1 * stepsCircle/360;
      int cmd_theta2 = cmdAngle.theta2 * stepsCircle/360;
      int cmd_theta3 = cmdAngle.theta3 * stepsCircle/360;
      int cmd_v1 = cmdAngle.vtheta1 * stepsCircle/360;
      int cmd_v2 = cmdAngle.vtheta2 * stepsCircle/360;
      int cmd_v3 = cmdAngle.vtheta3 * stepsCircle/360;
      /*long positions[3];
      positions[0]=cmd_theta1;
      positions[1]=cmd_theta2;
      positions[2]=cmd_theta3;*/

      int act_theta1 = a_stepper.currentPosition();
      int act_theta2 = b_stepper.currentPosition();
      int act_theta3 = c_stepper.currentPosition();

      int theta1_diff = cmd_theta1 - act_theta1;
      int theta2_diff = cmd_theta2 - act_theta2;
      int theta3_diff = cmd_theta3 - act_theta3;

      //Check if commanded angles are numbers and at least one motor should move more than one step
      if (!isnan(cmd_theta1) && !isnan(cmd_theta2) && !isnan(cmd_theta3)
          && checkWorkspace(cmdAngle.theta1,cmdAngle.theta2,cmdAngle.theta3)){

          if (abs(theta1_diff) >= 1|| abs(theta2_diff) >= 1 || abs(theta3_diff) >= 1){
            a_stepper.move(theta1_diff);
            b_stepper.move(theta2_diff);
            c_stepper.move(theta3_diff);
            a_stepper.setSpeed(cmd_v1);
            b_stepper.setSpeed(cmd_v2);
            c_stepper.setSpeed(cmd_v3);
           // steppers.moveTo(positions);
            oldstate_ = state_;
            state_ = STATE_MOVING;
            //nh.loginfo("STATE 'WAITING': Change to STATE 'MOVING'");
          }
      }

      else{
         nh.logerror("Commanded Joint angle not valid");
      }
    }
    else{
      nh.logwarn("Wrong State active");
    }
  }
  else{
    nh.logwarn("Motor Control not enabled");
  }
}
ros::Subscriber<delta_arduino::cmdAngle> subCmdAngle("delta/set_angle",&moveMotorTo);

void commandHandler(const std_msgs::String& cmdString){
  //Subscriber um Standardkommandos auszführen (durch Service ersetzen??)
  String command = cmdString.data;
  if (command.equals("RESET")){
    nh.loginfo("delta/command received: RESET");
    if(state_ != STATE_RESET){
      oldstate_ = state_;
      state_ = STATE_RESET;
    }
    else{
      nh.logwarn("Already resetting");
    }
  }
  else if(command.equals("CTRLSTOP")){
    nh.loginfo("delta/command received: CTRLSTOP");
    stopCtrl();
  }
  else if(command.equals("CTRLSTART")){
    nh.loginfo("delta/command received: CTRLSTART");
    if (!enable) {
      startCtrl();
     oldstate_ = state_;
     state_ = STATE_INIT;
    }
    else nh.logwarn("Motor Control already enabled");
  }
  else{
    nh.logerror("delta/command not valid!");
  }
}
ros::Subscriber<std_msgs::String> subCmdString("delta/command",&commandHandler);

void srvHandler(const delta_arduino::GetInfo::Request& req, delta_arduino::GetInfo::Response& res){
  String command  = req.in;
  if (command.equals("GETSTATE")){
    switch(state_)
    {
      case STATE_OFF:     res.out="OFF";      break;
      case STATE_INIT:    res.out="INIT";     break;
      case STATE_RESET:   res.out="RESET";    break;
      case STATE_WAITING: res.out="WAITING";  break;
      case STATE_MOVING:  res.out="MOVING";   break;
    }
  }
  else if (command.equals("GETANGLES")){
    res.theta1=a_stepper.currentPosition()/stepsCircle * 360;
    res.theta2=b_stepper.currentPosition()/stepsCircle * 360;
    res.theta3=c_stepper.currentPosition()/stepsCircle * 360;
    res.out="SENTANGLES";
  }
  else{
    nh.logerror("Service Call unknown");
  }
}

ros::ServiceServer<delta_arduino::GetInfo::Request,delta_arduino::GetInfo::Response> srvGetInfo("delta/get_info",&srvHandler);

void publishJointState(float freq){
  //Joint State mit Frequenz in Hz publishen
  //muss oft aufgerufen werden

  double d = nh.now().toNsec() - joint_state.header.stamp.toNsec();
  if(d >= 1000000000 / freq){

     //Joint Werte werden falsch dargestellt!
    joint_state.position[0] = (float)a_stepper.currentPosition() / stepsCircle*360;
    joint_state.position[1] = (float)b_stepper.currentPosition() / stepsCircle*360;
    joint_state.position[2] = (float)c_stepper.currentPosition() / stepsCircle*360;
    joint_state.header.stamp = nh.now();
    JointState.publish(&joint_state);
  }
}

void stateLoop()
{

    switch(state_)
    {
      case STATE_OFF:
      {
          break;
      }
      case STATE_INIT:
      {
          if(enable){
            oldstate_= state_;
            state_ = STATE_RESET;
            nh.loginfo("STATE 'INIT': Change to STATE 'RESET'");

          }
          break;
      }
      case STATE_RESET:
      {
          if (oldstate_ != state_){
            nh.loginfo("STATE 'RESET': Drive Motors to endstops");
            a_stepper.move(130*stepsCircle/360);
            b_stepper.move(130*stepsCircle/360);
            c_stepper.move(130*stepsCircle/360);
            a_stepper.setSpeed(stepsCircle/4.5);
            b_stepper.setSpeed(stepsCircle/4.5);
            c_stepper.setSpeed(stepsCircle/4.5);
            oldstate_ = state_;
          }
          if (a_stepper.distanceToGo() == 0 && b_stepper.distanceToGo() == 0 && c_stepper.distanceToGo() == 0){

            if(!set_reset){
                set_reset = true;
                nh.loginfo("STATE 'RESET': Move Motors back");
                a_stepper.setCurrentPosition(0);
                b_stepper.setCurrentPosition(0);
                c_stepper.setCurrentPosition(0);
                a_stepper.moveTo(-11*stepsCircle/360);
                b_stepper.moveTo(-11*stepsCircle/360);
                c_stepper.moveTo(-11*stepsCircle/360);
            }
            else{
                a_stepper.setCurrentPosition(0);
                b_stepper.setCurrentPosition(0);
                c_stepper.setCurrentPosition(0);
                set_reset = false;
                 oldstate_ = state_;
                state_ = STATE_WAITING;
                nh.loginfo("STATE 'RESET': Change to STATE 'WAITING'");
              }
          }
          break;
        }
        case STATE_WAITING:
         {
            if (oldstate_ != state_){
              //nh.loginfo("STATE 'WAITING': Waiting for new goal");
              oldstate_ = state_;
            }
             //publishJointState(1);

             break;
        }
        case STATE_MOVING:
        {
            if (oldstate_ != state_){
              /*char output[32];
              char msg[128] = "STATE 'MOVING': Move motors to: ";
              printAngles(output,true);
              strcat(msg,output);
              nh.loginfo(msg);*/
              oldstate_ = state_;
            }

           // publishJointState(20.0);

            if (a_stepper.distanceToGo() == 0 && b_stepper.distanceToGo() == 0 && c_stepper.distanceToGo() == 0){
              oldstate_ = state_;
              state_ = STATE_WAITING;
            }
            break;
        }
    }
}

void setup() {

  pinMode(LED_PIN, OUTPUT);
  //ROS functions
  nh.getHardware()->setBaud(115200);
  nh.initNode();
  nh.advertise(JointState);
  nh.subscribe(subCmdAngle);
  nh.subscribe(subCmdString);
  nh.advertiseService(srvGetInfo);


  joint_state.name_length =  3;
  joint_state.position_length =  3;
  joint_state.name[0] = (char*)"joint1";
  joint_state.name[1] = (char*)"joint2";
  joint_state.name[2] = (char*)"joint3";
  joint_state.header.stamp = nh.now();

  while (!nh.connected() ){
      nh.spinOnce();
  }

  //Accelstepper inits
  initialize(&a_stepper, A_ENABLE_PIN, A_END_PIN);
  initialize(&b_stepper, B_ENABLE_PIN, B_END_PIN);
  initialize(&c_stepper, C_ENABLE_PIN, C_END_PIN);

  nh.loginfo("All Motors initialized");

  //Deactivate motors at startup
  stopCtrl();
  nh.loginfo("STATE 'OFF': Arduino ready");
  state_ = STATE_OFF;

}

void loop() {


  nh.spinOnce();
  stateLoop();
  checkEndstops();
  if(enable){
     a_stepper.runSpeedToPosition();
     b_stepper.runSpeedToPosition();
     c_stepper.runSpeedToPosition();

  }
}
