#include <stdio.h>
#include <string.h>
#include <math.h>

/*MACRO*/
#define CATCH(X,Y)      X = X || Y
#define CATCH_ERR(X,Y)  controller_error(X, Y, __LINE__, __FILE__)

// Include "home-made" header files
#include "utils.h"
#include "odometry.h"
#include "kalman.h"
#include "trajectories.h"

#include <webots/robot.h>
#include <webots/motor.h>
#include <webots/gps.h>
#include <webots/accelerometer.h>
#include <webots/position_sensor.h>



//
/*CONSTANTES*/
#define MAX_SPEED 1000          // Maximum speed
#define INC_SPEED 5             // Increment not expressed in webots
#define MAX_SPEED_WEB 6.28      // Maximum speed webots

// If you would like to calibrate please define TIME_INIT_ACC 120 (for 120 seconds) else 0
// Set VERBOSE_CALIBRATION to true and pick the third trajectory in main
#define TIME_INIT_ACC 0       // Time in second
#define true 1
#define false 0
#define VERBOSE_CALIBRATION false

//---------------
/*VARIABLES*/ 
static pose_t         _pose, _odo_acc, _odo_enc, _kal_wheel, _kal_acc;
static kalman_t        pos_kalman, pos_kalman_better;
// Initial position found using the Robot node , then position
static pose_t         _pose_origin = {-2.9, 0.0, 0.0};
static FILE *fp;


static bool controller_init_log(const char* filename);
static bool controller_init();
static void controller_print_log(double time);
static bool controller_error(bool test, const char * message, int line, const char * fileName);


static measurement_t _meas; // See class above
double last_gps_time_s = 0.0f;
// Should we define a structure for the robot just like in the labs _robot.gps instead of dev_gps
int time_step; // Introduce the time step

WbDeviceTag dev_gps;
WbDeviceTag dev_acc;
WbDeviceTag dev_left_encoder;
WbDeviceTag dev_right_encoder;
WbDeviceTag dev_left_motor; 
WbDeviceTag dev_right_motor; 

void init_devices(int ts);

void init_devices(int ts) {
  dev_gps = wb_robot_get_device("gps");
  wb_gps_enable(dev_gps, 1000); // Enable GPS every 1000ms <=> 1s
  
  dev_acc = wb_robot_get_device("accelerometer");
  wb_accelerometer_enable(dev_acc, ts); // Time step frequency (ts)
  
  
  dev_left_encoder = wb_robot_get_device("left wheel sensor");
  dev_right_encoder = wb_robot_get_device("right wheel sensor");
  wb_position_sensor_enable(dev_left_encoder,  ts);
  wb_position_sensor_enable(dev_right_encoder, ts);

  dev_left_motor = wb_robot_get_device("left wheel motor");
  dev_right_motor = wb_robot_get_device("right wheel motor");
  wb_motor_set_position(dev_left_motor, INFINITY);
  wb_motor_set_position(dev_right_motor, INFINITY);
  wb_motor_set_velocity(dev_left_motor, 0.0);
  wb_motor_set_velocity(dev_right_motor, 0.0);
}

/*FUNCTIONS*/ 
// Make static to limit its scope

static void controller_get_pose_gps();
static void controller_get_gps();
static double controller_get_heading_gps();
static void controller_get_acc();

static void controller_get_encoder();
static void controller_compute_mean_acc();


int main() 
{
  wb_robot_init();
  if(CATCH_ERR(controller_init(), "Controller fails to init \n"))
    return 1;
  time_step = wb_robot_get_basic_time_step();
  init_devices(time_step);
  odo_reset(time_step);
  
  while (wb_robot_step(time_step) != -1)  
  {
    // Localization from GPS
    // True position stored in _pose vector 
    controller_get_pose_gps();
    
    // Get the acceleration from webots
    controller_get_acc();
    
    // Get the encoder values (wheel motor values)
    controller_get_encoder();

    _meas.acc_mean[0] = 6.44938e-05 ; //y
    _meas.acc_mean[1] = 0.00766816; // x
    _meas.acc_mean[2] = 9.62942 ; // z
    
  
  // Uncomment the if else block in order to calibrate the accelerometer
  
  if( wb_robot_get_time() < TIME_INIT_ACC )
  {

    controller_compute_mean_acc();
  }
   else
   
  {
  

    if (!VERBOSE_CALIBRATION) {
    
    // Localization Odometry from wheel encoders
    odo_compute_encoders(&_odo_enc, _meas.left_enc - _meas.prev_left_enc, _meas.right_enc - _meas.prev_right_enc);
    // Localization Odometry from accelerometer with heading from wheel encoders
    odo_compute_acc(&_odo_acc, _meas.acc, _meas.acc_mean,_odo_enc.heading);
    
    // Kalman with wheel encoders
    double time_now_s = wb_robot_get_time();
    const int time_step_ = wb_robot_get_basic_time_step();
    compute_kalman_wheels(&_kal_wheel, time_step_, time_now_s, _pose, _odo_enc.heading, _meas.left_enc - _meas.prev_left_enc, _meas.right_enc - _meas.prev_right_enc);
    compute_kalman_acc(&_kal_acc, time_step_ , time_now_s, _pose, _odo_enc.heading, _meas);
    
   }


  
  // Use one of the two trajectories.
    //trajectory_1(dev_left_motor, dev_right_motor);
    trajectory_2(dev_left_motor, dev_right_motor);
    //trajectory_3(dev_left_motor, dev_right_motor);
    //trajectory_4(dev_left_motor, dev_right_motor);
    //trajectory_5(dev_left_motor, dev_right_motor);
  }
  controller_print_log(wb_robot_get_time());
  }
  // Close log file
  if(fp != NULL)
    fclose(fp);
  // End of the simulation
  wb_robot_cleanup();

  return 0;
  
  
  

}

void controller_get_pose_gps()
{
  double time_now_s = wb_robot_get_time();
  
  if (time_now_s - last_gps_time_s > 1.0f) {
    
    last_gps_time_s = time_now_s;
    // Update gps measurements
    controller_get_gps();
    _pose.x = _meas.gps[0] - _pose_origin.x;
      
    _pose.y = -(_meas.gps[2] - _pose_origin.y);
      
    _pose.heading = controller_get_heading_gps() + _pose_origin.heading;
    printf("ROBOT pose : %g %g %g\n", _pose.x , _pose.y , RAD2DEG(_pose.heading));
  
  }
  

}
/**
 *
 * @brief Get the GPS measurements for position of robots
 */

void controller_get_gps()
{
    // Stores in memory at address of _meas.prev_gps, the data of _meas.gps
    memcpy(_meas.prev_gps, _meas.gps, sizeof(_meas.gps));
    // Get position
    const double * gps_position = wb_gps_get_values(dev_gps);
    // Stores in memory at address of _meas.gps, the data of computed gps_position
    memcpy(_meas.gps, gps_position, sizeof(_meas.gps));

}
/**
 * @brief      Compute the heading (orientation) of the robot based on the gps position values.
 *
 * @return     return the computed angle
 */
double controller_get_heading_gps()
{
    // Orientation of the robot
    double delta_x = _meas.gps[0] - _meas.prev_gps[0];

    double delta_y = -(_meas.gps[2] - _meas.prev_gps[2]);

    // Compute the heading of the robot
    double heading = atan2(delta_y, delta_x);
    return heading;
}


/**
 * @brief      Read the acceleration values from the sensor
 */
void controller_get_acc()
{
    // Call the function to get the accelerometer measurements.
    const double * acc_values = wb_accelerometer_get_values(dev_acc);

    // Copy the acc_values into the measurment structure (static variable)
    memcpy(_meas.acc, acc_values, sizeof(_meas.acc));
}



/**
 * @brief      Read the encoders values from the sensors
 */
void controller_get_encoder()
{
    // Store previous value of the left encoder
    _meas.prev_left_enc = _meas.left_enc;

    _meas.left_enc = wb_position_sensor_get_value(dev_left_encoder);

    // Store previous value of the right encoder
    _meas.prev_right_enc = _meas.right_enc;

    _meas.right_enc = wb_position_sensor_get_value(dev_right_encoder);

}

/**
 * @brief      Set the speed to the motors according to the user commands
 */ // We don't need this because we have the trajectories ! 

/**
 * @brief      Compute the mean of the 3-axis accelerometer. The result is stored in array _meas.acc
 */
void controller_compute_mean_acc()
{

    static int count = 0;

    count++;

    if( count > 20 ) // Remove the effects of strong acceleration at the beginning
    {
        for(int i = 0; i < 3; i++) {
            _meas.acc_mean[i] = (_meas.acc_mean[i] * (count - 1) + _meas.acc[i]) / (double) count;
            _meas.acc_mean_calibration[i] += _meas.acc_mean[i];
            }
    }
    
      
   

    // Check if the time_step works, because right now it should be a global variable initizialed in "main"
    // This just checks that all is good

    
    if( count == (int) ((TIME_INIT_ACC / (double) time_step)*1000 - 1))
    {
        printf("Accelerometer initialization Done ! \n");
        for (int i = 0; i < 3; i++) {
          _meas.acc_mean_calibration[i] = _meas.acc_mean_calibration[i]/(count);
         }
         
        printf("mean_Y = %g, meanx = %g, meanz = %g", _meas.acc_mean_calibration[0], _meas.acc_mean_calibration[1], _meas.acc_mean_calibration[2]);
        
    }
    

        
     
       
       /*
         // Found values (10 seconds)
        acc_mean[0] = 6.36027e-05 
        acc_mean[1] = 0.00756158 
        acc_mean[2] = 9.49557 
        // 120 seconds and averaged to count
            _meas.acc_mean[0] = 6.44938e-05 ; //y
            _meas.acc_mean[1] = 0.00766816; // x
          _meas.acc_mean[2] = 9.62942 ; // z
          
          */
        
        
      
        
        
       
}


/**
 * @brief      Initialize the logging of the file
 *
 * @param[in]  filename  The filename to write
 *
 * @return     return true if it fails
 */
bool controller_init_log(const char* filename)
{

  fp = fopen(filename,"w");
  
  bool err = CATCH_ERR(fp == NULL, "Fails to create a log file\n");

  if( !err )
  {
    fprintf(fp, "time; pose_x; pose_y; pose_heading;  gps_x; gps_y; gps_z; acc_0; acc_1; acc_2; right_enc; left_enc; odo_acc_x; odo_acc_y; odo_acc_heading; odo_enc_x; odo_enc_y; odo_enc_heading; kal_wheel_x; kal_wheel_y; kal_wheel_heading; kal_acc_x; kal_acc_y; kal_acc_heading\n");
  }

  return err;
}



bool controller_init()
{
  bool err = false;
  CATCH(err, controller_init_log("log_file.csv"));
  
  return err;

}

void controller_print_log(double time)
{

  if( fp != NULL)
  {
    fprintf(fp, "%g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g; %g\n",
            time, _pose.x, _pose.y , _pose.heading, _meas.gps[0], _meas.gps[1], 
      _meas.gps[2], _meas.acc[0], _meas.acc[1], _meas.acc[2], _meas.right_enc, _meas.left_enc, 
      _odo_acc.x, _odo_acc.y, _odo_acc.heading, _odo_enc.x, _odo_enc.y, _odo_enc.heading, _kal_wheel.x, _kal_wheel.y, _kal_wheel.heading, _kal_acc.x, _kal_acc.y, _kal_acc.heading);
  }

}
/**
 * @brief      Do an error test if the result is true write the message in the stderr.
 *
 * @param[in]  test     The error test to run
 * @param[in]  message  The error message
 *
 * @return     true if there is an error
 */
bool controller_error(bool test, const char * message, int line, const char * fileName)
{
  if (test) 
  {
    char buffer[256];

    sprintf(buffer, "file : %s, line : %d,  error : %s", fileName, line, message);

    fprintf(stderr,buffer);

    return(true);
  }

  return false;
}

