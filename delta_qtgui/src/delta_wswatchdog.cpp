#include "delta_wswatchdog.hpp"
#include <ros/ros.h>
#include <ros/package.h>

WSWatchdog::WSWatchdog(){

}

WSWatchdog::~WSWatchdog(){

}
int WSWatchdog::readWorkSpace() {
  //Read Workspace values, given in csv files. Each file describes a convex polygon in the xy-pane
  char buffer[32] = { 0 };
  char *record, *line;
  char path[100];
  char pathno[10];
  std::stringstream ss;
  std::stringstream pathstream;
  std::string s;

  for (int j = 0; j < Z_ROWS; j++) {
    int i = 0;

    std::string path_str = ros::package::getPath("delta_qtgui");
    if(path_str.compare("")==0){

      ss << "Path to delta_qtgui not found";

      s = ss.str();
      ROS_INFO(s.c_str());

      return -1;
    }
    pathstream.str("");
    pathstream << path_str<<"/resources/WorkspaceData/"<<(j + Z_MIN)<<".csv";

    snprintf(path,100,(pathstream.str()).c_str());
    FILE *fstream = fopen(path, "r");

    if (fstream == NULL)
    {
      ss.str("");
      ss << "Opening of file " << path << " failed";

      s =ss.str();
      ROS_INFO(s.c_str());

      return -1;
    }
    while ((i < XY_LEN && fgets(buffer, sizeof(buffer), fstream)) != NULL)
    {
      if (sscanf(buffer, "%d\t%d", &Workspace[j][i][0], &Workspace[j][i][1]) == 2){
        i++;
      }
    }
    Workspace[j][i][0]=9999;
    fclose(fstream);
  }
  return 1;
}
int WSWatchdog::pointInPolygon(int nvert, float *vertx, float *verty, float testx, float testy){
  //Check if testpoint (testx,testy) is in Polygon (vertx,verty) with nvert vertices
  int i, j, c = 0;
  for (i = 0, j = nvert - 1; i < nvert; j = i++) {
    vertx[i];
    verty[i];
    if (((verty[i]>testy) != (verty[j]>testy)) &&
      (testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
      c = !c;
  }
  return c;
}
int WSWatchdog::calcPointOnPolygon(int nvert, float *vertx, float *verty, float testx, float testy,float &lpointx,float &lpointy) {
  //Calculate point on Polygon boundary, which is the closest to the testpoint
  int i = 0;
  float pk1_x = 998, pk2_x = 999, pk1_y = 998, pk2_y = 999; //Two points of polygon closest to testpoint
  float d1 = 999999, d2 = 999999; //Distance to two closest points, initiliazed with absurdly high values
  float d = 0;
  //loop to calculate two closest points
  for (i = 0; i < nvert; i++) {
    d = (testx - vertx[i])*(testx - vertx[i]) + (testy - verty[i])*(testy - verty[i]);
    //check if point is closer that the two stored points
    if (d < d1 && d < d2) {

      d2 = d1;
      d1 = d;
      pk2_x = pk1_x;
      pk2_y = pk1_y;

      pk1_x = vertx[i];
      pk1_y = verty[i];
    }
    //check if point is closer than the second closest point
    else if (d < d2 && d >= d1) {
      if (pk1_x != vertx[i] || pk1_y != verty[i]) {
        d2 = d;
        pk2_x = vertx[i];
        pk2_y = verty[i];
      }
    }
  }

  //Normal vector for line between the two polygon points
  float n2 = -(pk2_x - pk1_x);
  float n1 = pk2_y - pk1_y;
  //Perpendicular point for testpoint on line between pk1 and pk2
  lpointx = testx - (((testx - pk1_x)*n1+ (testy - pk1_y)*n2) / (n1*n1 + n2*n2))*n1;
  lpointy = testy - (((testx - pk1_x)*n1 + (testy - pk1_y)*n2) / (n1*n1 + n2*n2))*n2;
  //Check if Perpendicular point lies between pk1 and pk2, if not set point to pk1;
  float dp = (pk1_x - pk2_x)*(pk1_x - pk2_x) + (pk1_y - pk2_y)*(pk1_y - pk2_y);
  if (d2 > dp){
    lpointx=pk1_x;
    lpointy=pk2_y;
    return 0;
   }
  if (isnan(lpointx) || isnan(lpointy)){

    return -1;
  }
  else return 0;
}
bool WSWatchdog::checkPoint(const std::vector<float> point){
  float x_prop = point[0];
  float y_prop = point[1];
  float z_prop = point[2];
  if(giveBoundedPoint(x_prop,y_prop,z_prop) == 1) return true;
  else return false;
}
int WSWatchdog::giveBoundedPoint(float &x_prop, float &y_prop, float &z_prop) {
  //checks if proposed point is in Workspace, else calculates point on workspace boundary
  int i;
  bool arrayEndFound = false;
  //round z value and transform to array index
  int zInArray = (int)round(z_prop) - Z_MIN;


  int countRows = 0;
  while (arrayEndFound == false) {
    if (Workspace[zInArray][countRows][0] == 9999) arrayEndFound = true;
    else countRows++;

  }

  if (countRows > 0) {
    //Create helper array, which only contains the necessary xy-pane
    float *testArrayX;
    float *testArrayY;
    testArrayX = (float*)malloc(countRows * sizeof(float));
    testArrayY = (float*)malloc(countRows * sizeof(float));

    for (i = 0; i < countRows; i++) {
      testArrayX[i] = (float)Workspace[zInArray][i][0];
      testArrayY[i] = (float)Workspace[zInArray][i][1];
    }
    //Check if proposed xy-point is in boundary
    if (pointInPolygon(countRows, testArrayX, testArrayY, x_prop, y_prop)) {

      free(testArrayX);
      free(testArrayY);
      return 1; //1 indicates that point is inside workspace
    }
    else {
      float polyPointx, polyPointy;
      calcPointOnPolygon(countRows, testArrayX, testArrayY, x_prop, y_prop, polyPointx, polyPointy);
      x_prop = polyPointx;
      y_prop = polyPointy;
      free(testArrayX);
      free(testArrayY);
      return 2;//2 indicates that point would be outside of workspace and point on boundary was calculated
    }
  }
  else return -1;

}
using namespace Eigen;
using namespace std;
bool WSWatchdog::circleInWorkspace(const Vector3d &circCenter, const Vector3d &circAxis, const double &circRadius)
{
  //Überprüfen ob ausgewählter Kreis komplett im Arbeitsraum liegt
  float theta_i = 0;
  vector<float> pos_i(3,0);
  Vector3d cao_helper(4.0+circAxis[0], 4.0+circAxis[0]+circAxis[1], 4.0+circAxis[0]+circAxis[1]+circAxis[2]);
  //Orthogonaler Vektor zu Kreisachse
  Vector3d cao = cao_helper.cross(circAxis);
  cao = cao/cao.norm();
  //Punkt auf Kreis
  Vector3d cp = circCenter + cao*circRadius;

 while(theta_i <= 2*M_PI){


    delta_math::rotatePointAroundCircleAxis(cp ,pos_i,theta_i,circAxis);
    if(giveBoundedPoint(pos_i[0],pos_i[1],pos_i[2]) == 2) return false;
    //100 verschiedene Winkel überprüfen
    theta_i += 2*M_PI/100;
  }
  return true;
}
