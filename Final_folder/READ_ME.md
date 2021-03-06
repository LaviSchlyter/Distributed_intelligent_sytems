# DIS Semester Project 

This project aims at implementing a navigation strategy for a multi-robot system formed by a group of simulated e-pucks. The robots must be able to position themselves using odometry and Kalman; they must be able to move together as a group towards a specific target avoiding static and dynamic obstacles while maitaining a given formation.

The code is written in C and is runnable on Weebots. The metrics and plots and performed in Matlab.

**AUTHORS** = Clément Cosson, Paco Mermoud, Tiffany Pereira and Lavinia Schlyter


The Final folder contains three directories:

``` bash
Final Folder
├── Matlab
├── controllers
│   └── localization_controller
│   │    
│   └── crossing_leader
│   │    
│   └── obstacle leader
│   ... etc 
│  
│
├── worlds
│   └── localization.wbt
│   │    
│   └── obstacles.wbt
│   │    
│   └── crossing.wbt
│   ...etc    
│       
```
------------------------------- OPTIMAL SETTINGS FOR THE DIFFERENT WORLDS -----------------------------

World "localization": Kalman filter with wheel encoder and low-frequency GPS returns the best localization.
World "crossing": Mataric PI controller, with non-PSO weights for both leader and followers.
World "obstacles": Laplacian with maximum number of edges, with PSO weights for followers only.

------------------------------- LOCALIZATION CONTROLLER -----------------------------

**localization_controller.c**
We run a main file which computes the position of the robot of interest using odometry with the accelerometer as well and the wheel encoder but also the updated version which uses GPS coordinates at every second (NOTE: Starts at second =1 and not zero) for both odometries. 

## Calibrate accelerometer (localization_controller.c)
In order to calibrate the accelerometer, that is to compute its bias, please follow the following instructions:
1) Set the TIME_INIT_ACC = 120 at line 43
2) Set the VERBOSE_CALIBRATION to "true" at line 46
3) Change the trajectory at line 193 to 3 (It will keep the robot into position) and comment the other ones.
4) A message will be printed with the mean accelerations in all directions which are then hardcoded from line 136-140
5) Copy the values in line 137 to 140



Note: The relevant frame used in this project for the robot is shown below (for a robot starting from the left). If the robot starts from the right (that is, it is heading to the left) the x axis is inverted. This is done in order to ease computation with relative positions. The angle is computed w.r.t the x-axis anti-clockwards for positive.

   
Because we are updating using the GPS but we want it in the relative to robot frame, we shift the GPS using the initial position of the robot stored in _pose_origin = {x, y, heading}; 
You may note that throughout the worlds these are hard coded and must be changed if needed (expect when the supervisor is present such as in PSO) where the start positions are shifted on the y-axis 
and these new start positions are sent to the controller.
Note: Because updating the heading with GPS is not optimal as discussed in the report; the _pose.heading is not used in the updating step (refer to kalman.c, where the z vector representing the update vector, the third component (for the wheel encoder) keeps its own heading.


**odometry.c**
Use the accelerometer and wheel encoder in order to compute the position of the robots. The heading for accelerometer is not computed and uses the wheel encoders instead.

**kalman.c**
Updating both odometry measures by using the shifted GPS (_pose vector) every second 



## Adding kalman and odometry to Makefile
In order to use the odometry and the kalman in controllers, they must be added with the relative path to the Makefile in the C_SOURCES. 
(eg. C_SOURCES= ../localization_controller/odometry.c ../localization_controller/kalman.c obstacle_leader.c)


------------------------------- FLOCKING CONTROLLER ----------------------------------
In the "robot_flock.c" you find the controller which pushes the robot to flock together in no particular order/formation
The flock size is set to 5 and the various thresholds for the rules discussed in the report may be changed between line 41 and 54
You may remove the migration urge by setting MIGRATION_WEIGHT 0 at line 56.

If you would like to test the code for 5 robots with different origin positions, this must be done within the line 91-97 (GPS frame coordinates)  


------------------------------- FORMATION CONTROLLERS --------------------------------

The leader-follower architecture is implemented to move in formation toward a common goal. In each flock, there is always one leader and the other robots are followers. 
For the leader, one controller has been implemented for each world (crossing_leader, obstacle_leader) 
.For the followers, two controllers have been implemented for each world (crossing_follower_laplacian, crossing_follower_mataric, obstacle_follower_laplacian, obstacle_follower_mataric).
Each one of these controllers has several functionalities, that will be described for each world below: 

-- "Obstacles" and "test_obstacles" world -- mataric controller:
1) Assign the controller "obstacle_leader" to robot ID 0. The migration urge can be changed at line 89.
2) Assign the controller "obstacle_follower_mataric" to robot ID 1,2,3 and 4.
3) The flock size can be changed as follows:
    - To have a flock size of 7 : add two robots to the world (copy-paste a robot) and place them somewhere in front of the flock, change their DEF and their name to "epuck5" and "epuck6", and change FLOCK_SIZE to 7 in "obstacle_follower_mataric" at line 30.
    - To have a flock size of 6 : add a robot to the world (copy-paste a robot) and place it somewhere in front of the flock, change its DEF and its name to "epuck5", and change FLOCK_SIZE to 6 in "obstacle_follower_mataric" at line 30.
    - To have a flock size of 5 : don't do anything, that's the default size.
    - To have a flock size of 4 : remove robot ID 4 from the world and change FLOCK_SIZE to 4 in "obstacle_follower_mataric" at line 30.
    - To have a flock size of 3 : remove robot ID 4, 3 from the world and change FLOCK_SIZE to 3 in "obstacle_follower_mataric" at line 30.
    - To have a flock size of 2 : remove robot ID 4, 3, 2 from the world and change FLOCK_SIZE to 2 in "obstacle_follower_mataric" at line 30.
4) The controller type can be changed at line 36 of "obstacle_follower_mataric" by changing CONTROLLER_TYPE to P (proportional), PI (proportional and integral) or NPI (non linear proportional and integral). 


-- "Obstacles" and "test_obstacles" world  -- laplacian controller:
1) Assign the controller "obstacle_leader" to robot ID 0. The migration urge can be changed at line 89.
2) Assign the controller "obstacle_follower_laplacian" to robot ID 1,2,3 and 4.
3) With this controller you can change the number of edges in the graph by changing NB_EDGES (4,8 or 10) for a fixed FLOCK_SIZE of 5. Here are the possible combinations (change the values accordingly at line 35-36 of "obstacle_follower_laplacian"):
    - (FLOCK_SIZE = 5, NB_EDGES = 4)
    - (FLOCK_SIZE = 5, NB_EDGES = 8)
    - (FLOCK_SIZE = 5, NB_EDGES = 10)
4) With this controller you can change the number of robots in the formation by changing FLOCK_SIZE (3,4 or 5) for a fully connected graph. Here are the possible combinations (change the values accordingly at line 35-36 of "obstacle_follower_laplacian"):
    - (FLOCK_SIZE = 3, NB_EDGES = 3) and remove robot ID 4 and 3 from the world.
    - (FLOCK_SIZE = 4, NB_EDGES = 6) and remove robot ID 4 from the world.
    - (FLOCK_SIZE = 5, NB_EDGES = 10)


-- "crossing" world -- mataric controller:
1) Assign the controller "crossing_leader" to robot ID 0 and 5. Change WORLD to CROSSING at line 38.
2) Assign the controller "crossing_follower_mataric" to robot ID 1,2,3,4,6,7,8 and 9. Change WORLD to CROSSING at line 41.
3) The flock size can be changed as follows:
    - To have a flock size of 5 : don't do anything, that's the default size.
    - To have a flock size of 4 : remove robot ID 3 and 9 from the world and change FLOCK_SIZE to 4 in "crossing_follower_mataric" at line 30 and in "crossing_leader" at line 33.
    - To have a flock size of 3 : remove robot ID 3, 4, 8 and 9 from the world and change FLOCK_SIZE to 3 in "crossing_follower_mataric" at line 30 and in "crossing_leader" at line 33.
    - To have a flock size of 2 : remove robot ID 2, 3, 4, 6, 8 and 9 from the world and change FLOCK_SIZE to 2 in "crossing_follower_mataric" at line 30 and in "crossing_leader" at line 33.
4) The controller type can be changed at line 36 of "crossing_follower_mataric" by changing CONTROLLER_TYPE to P (proportional), PI (proportional and integral) or NPI (non linear proportional and integral). The default controller is proportional.

-- "test_crossing world" -- mataric controller:
1) Assign the controller "crossing_leader" to robot ID 0 and 5. Change WORLD to TEST_CROSSING at line 38.
2) Assign the controller "crossing_follower_mataric" to robot ID 1,2,3,4,6,7,8 and 9. Change WORLD to TEST_CROSSING at line 41.
3) The flock size can be changed as follows:
    - To have a flock size of 5 : don't do anything, that's the default size.
    - To have a flock size of 4 : remove robot ID 3 and 8 from the world and change FLOCK_SIZE to 4 in "crossing_follower_mataric" at line 30 and in "crossing_leader" at line 33.
    - To have a flock size of 3 : remove robot ID 3, 4, 8 and 9 from the world and change FLOCK_SIZE to 3 in "crossing_follower_mataric" at line 30 and in "crossing_leader" at line 33.
    - To have a flock size of 2 : remove robot ID 2, 3, 4, 7, 8 and 9 from the world and change FLOCK_SIZE to 2 in "crossing_follower_mataric" at line 30 and in "crossing_leader" at line 33.
4) The controller type can be changed at line 36 of "crossing_follower_mataric" by changing CONTROLLER_TYPE to P (proportional), PI (proportional and integral) or NPI (non linear proportional and integral). 


-- "crossing" world -- laplacian controller:
1) Assign the controller "crossing_leader" to robot ID 0 and 5. Change WORLD to CROSSING at line 38.
2) Assign the controller "crossing_follower_laplacian" to robot ID 1,2,3,4,6,7,8 and 9. Change WORLD to CROSSING at line 41.
3) With this controller you can change the number of edges in the graph by changing NB_EDGES (4,8 or 10) for a fixed FLOCK_SIZE of 5. Here are the possible combinations (change the values accordingly at line 35-36 of "crossing_follower_laplacian"):
    - (FLOCK_SIZE = 5, NB_EDGES = 4)
    - (FLOCK_SIZE = 5, NB_EDGES = 8)
    - (FLOCK_SIZE = 5, NB_EDGES = 10)
4) With this controller you can change the number of robots in the formation by changing FLOCK_SIZE (3,4 or 5) for a fully connected graph. Here are the possible combinations (change the values accordingly at line 35-36 of "crossing_follower_laplacian"):
    - (FLOCK_SIZE = 3, NB_EDGES = 3) and remove robot ID 3, 4, 8 and 9 from the world, and change FLOCK_SIZE to 3 in "crossing_leader" at line 33.
    - (FLOCK_SIZE = 4, NB_EDGES = 6) and remove robot ID 4 and 8 from the world, and change FLOCK_SIZE to 4 in "crossing_leader" at line 33.
    - (FLOCK_SIZE = 5, NB_EDGES = 10) 


-- "test_crossing" world -- laplacian controller:
1) Assign the controller "crossing_leader" to robot ID 0 and 5. Change WORLD to TEST_CROSSING at line 38.
2) Assign the controller "crossing_follower_laplacian" to robot ID 1,2,3,4,6,7,8 and 9. Change WORLD to TEST_CROSSING at line 41.
3) With this controller you can change the number of edges in the graph by changing NB_EDGES (4,8 or 10) for a fixed FLOCK_SIZE of 5. Here are the possible combinations (change the values accordingly at line 35-36 of "crossing_follower_laplacian"):
    - (FLOCK_SIZE = 5, NB_EDGES = 4)
    - (FLOCK_SIZE = 5, NB_EDGES = 8)
    - (FLOCK_SIZE = 5, NB_EDGES = 10)
4) With this controller you can change the number of robots in the formation by changing FLOCK_SIZE (3,4 or 5) for a fully connected graph. Here are the possible combinations (change the values accordingly at line 35-36 of "crossing_follower_laplacian"):
    - (FLOCK_SIZE = 3, NB_EDGES = 3) and remove robot ID 3, 4, 8 and 9 from the world, and change FLOCK_SIZE to 3 in "crossing_leader" at line 33.
    - (FLOCK_SIZE = 4, NB_EDGES = 6) and remove robot ID 4 and 9 from the world, and change FLOCK_SIZE to 4 in "crossing_leader" at line 33.
    - (FLOCK_SIZE = 5, NB_EDGES = 10) 

------------------------------------------ PSO ---------------------------------------
To optimize the parameters of PSO we use the following worlds:
pso_world_simplified_avoidance.wbt : To optimize the obstacle avoidance parameters
pso_world_simplified_formation.wbt : To optimize the followers' formation controller parameters
pso_world_simplified_flocking.wbt : To optimize the flocking parameters
pso_world_flocking.wbt : To optimize both flocking and obstacle avoidance parameters


The PSO codes are standard implementations of the PSO algorithm, with a few project-specific twists. Four different codes were developed, for tuning different set of parameters (Braitenberg weights, follower rotational and forward control command, Reynold's rules weights and tresholds as well as obstacle avoidance parameters). All PSOs use the same general architectures.
Different options are available on all PSO controllers:
1) Noise resistant PSO is more robust to "lucky" bad solutions.
2) Bounded parameters search space to avoid nonsensical values.
3) Robots respawn positions can either be fixed (robots will always respawn at the same location in arena) or random (the y axis of the robot flock has a random component).
4) Initial parameter values can either be fully random or use some user knowledge and start with an empirical value set.


-------------------------------------Localization Supervisor  ---------------------------------------

The supervisor computes the true position of the robots in a log file in order to compute the fitness metrics on Matlab.

The flock size is defined at line 22, and must be changed according to the number of robots in the world before running each simulation.

Robot_ID depends on the simulation ran:

** For simulations in the obstacle or localization world or for crossing with 2 teams of 4 or more robots (each)
- line 25 must be kept 
- line 28, 30, 32 must be commented

** For simulations on the crossing world with 2 robots on each team
- Uncomment line 28 and comment 25,30,32

** For simulations on the crossing world with 3 robots on each team
- Uncomment line 30 and comment 25,28,32

** For simulations on the crossing world with 4 robots on each team
- Uncomment line 32 and comment 25,28,30

-------------------------------------Matlab codes  ---------------------------------------

The different Matlab codes are used to compute the metrics. In order to do this, they read the log files written by the supervisor (and eventually by the robots controllers themselves), extract true (and approximated) positions and compute the metrics values. These metrics values are then stored as matrices, and can be used to generate graphs.

When wanting to compute a certain metric for a certain world, follow these steps:
1) Run the simulation in webots on the desired world with the desired number of robots. Be sure to include the supervisor code, it is necessary for metric calculation.
2) Run the Matlab code to compute desired metric: compute_metric_x, x can be localization, formation or flocking. At the beginning  of the code, change the parameters for the different conditions (number of robots and teams, world and controller type, etc...). Some of the codes also automatically create figures.
3) The code will automatically save the metrics in the file metric_values. They can later be re-loaded to create comparative graphs.












