/*****************************************************************************************************************************/
/* File:         pso_sup_formation.c                                           */
/* Version:      1.0                                                           */
/* Date:         01-Mai-21                                                     */
/* Description:  PSO for the crossing formation (laplacian) with a group       */
/*               public  heterogenous method.                                  */
/*                                                                             */
/* Author:       Paco Mermoud                                                  */
/*****************************************************************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <webots/emitter.h>
#include <webots/receiver.h>
#include <webots/supervisor.h>
#include <webots/robot.h>

#define NOISY 1

#if NOISY == 1
#define ITS_COEFF 1.0     // Multiplier for number of iterations
#else
#define ITS_COEFF 2.0     // Multiplier for number of iterations
#endif

#define FONT "Arial"
#define NB_INIT_POS 5 // number of fixed pos
#define FLOCK_SIZE 5
#define ROBOTS 1
#define ROB_RAD 0.035
#define WHEEL_RADIUS 0.0205	// Wheel radius (meters)
#define MAX_SPEED  6.28        // maximum speed of the robots
#define NB_SENSORS 8
#define TIME_STEP	64		// [ms] Length of time step

/* PSO definitions */
#define NB_PARTICLE 15                 // Number of particles in swarm
#define MIN_WEIGHT_BRAITEN -200         // Minimum of a particles weight for braiten
#define MAX_WEIGHT_BRAITEN 200         // Maximum of a particles weight for braiten

#define PRIOR_KNOWLEDGE 1
#define NB_NEIGHBOURS 3                 // Number of neighbors on each side
#define LWEIGHT 2.0                     // Weight of attraction to personal best
#define NBWEIGHT 5.0                    // Weight of attraction to neighborhood best
#define DAMPING 0.6                     // damping of the particle velocity
#define VMAX 30.0                       // Maximum velocity of particles by default
#define VMAX_REYNOLD  0.01               // Maximum velocity of particles for  reynold


#define MIN_BRAITEN -200.0               // Lower bound on initialization value for braiten
#define MAX_BRAITEN 200.0                // Upper bound on initialization value for braiten
#define MIN_REYNOLDS 0.0001              // Lower bound on initialization value for reynolds
#define MAX_REYNOLDS 0.2                // Upper bound on initialization value for reynolds
#define ITS_PSO 50                      // Number of iterations to run
#define DATASIZE_LEADER NB_SENSORS+5         // Number of elements in particle (2 Neurons with 8 proximity sensors and 5 params for flocking)
#define DATASIZE_FOLLOWER 1
#define DATASIZE DATASIZE_LEADER+DATASIZE_FOLLOWER
#define FINALRUNS 1
#define N_RUNS 1
#define WEIGHT_DFL 2
#define WEIGHT_FIT_OBSTACLE 1
#define WEIGHT_FIT_FLOCKING 4


/* Initial position of robots */
#define RND_POS 1               // Initialize weight with 'set_init_pos'


/* Types of fitness evaluations */
#define EVOLVE 0          // Find new fitness
#define EVOLVE_AVG 1      // Average new fitness into total
#define SELECT 2          // Find more accurate fitness for best selection


/* Fitness definitions */
#define TARGET_FLOCKING_DISTANCE ROB_RAD*4 // targeted flocking distance (2 robot diameters)

#define PI 3.1415926535897932384626433832795


static WbNodeRef epucks[FLOCK_SIZE];
WbDeviceTag emitter[FLOCK_SIZE];
WbDeviceTag rec[FLOCK_SIZE];
const double *loc[FLOCK_SIZE];
const double *rot[FLOCK_SIZE];

// Relative position of each robots
const double rel_init_pos_robot[FLOCK_SIZE][2]={{-2.8,0},
                                             {-2.8,0.1},
                                             {-2.8,-0.1},
                                             {-2.8,0.2},
                                             {-2.8,-0.2},
                                             };
double new_loc[FLOCK_SIZE][3];
double new_rot[FLOCK_SIZE][4];
int num_leader = 0;  //robot id of the leader

// Print in webots FONT
char label[50];
char label2[20];

// initial set of weight for pso
double prior_knowledge_follower[DATASIZE_FOLLOWER] = {17,29,34,10,8,-60,-64,-84, // Braitenberg right //TODO
                        //-80,-66,-62,8,10,36,28,18, // Braitenberg left
                        0.2, (0.6/10),      // rule1_tresh, rule1_weight
                        0.15, (0.02/10),    // rule2_tresh, rule2_weight
                        (0.01/10)};         // migration_weight

double prior_knowledge_leader[DATASIZE_LEADER] = {17,29,34,10,8,-60,-64,-84, // Braitenberg right  //TODO
                        //-80,-66,-62,8,10,36,28,18, // Braitenberg left
                        0.2, (0.6/10),      // rule1_tresh, rule1_weight
                        0.15, (0.02/10),    // rule2_tresh, rule2_weight
                        (0.01/10)};         // migration_weight


// Prototypes



/* RESET - Get device handles and starting locations */
void reset(void) {
  wb_robot_init();
  // Device variables
  char epuck[] = "epuck0";
  char em[] = "emitter0_pso";
  char receive[] = "receiver0_pso";

  int i; //counter
  for (i=0;i<FLOCK_SIZE;i++) {
    epucks[i] = wb_supervisor_node_get_from_def(epuck);
    loc[i] = wb_supervisor_field_get_sf_vec3f(wb_supervisor_node_get_field(epucks[i],"translation"));
    new_loc[i][0] = loc[i][0]; new_loc[i][1] = loc[i][1]; new_loc[i][2] = loc[i][2];
    rot[i] = wb_supervisor_field_get_sf_rotation(wb_supervisor_node_get_field(epucks[i],"rotation"));
    new_rot[i][0] = rot[i][0]; new_rot[i][1] = rot[i][1]; new_rot[i][2] = rot[i][2]; new_rot[i][3] = rot[i][3];
    emitter[i] = wb_robot_get_device(em);
    if (emitter[i]==0) printf("missing emitter %d\n",i);
    rec[i] = wb_robot_get_device(receive);
    wb_receiver_enable(rec[i],TIME_STEP/2);
    epuck[5]++;
    em[7]++;
    receive[8]++;
  }
  wb_robot_step(TIME_STEP*4);
}

// Copy one particle to another
void copyParticle(double particle1[DATASIZE], double particle2[DATASIZE]) {
  int i;                   // FOR-loop counters

  // Copy one bit at a time
  for (i = 0; i < DATASIZE; i++)
    particle1[i] = particle2[i];
}


// Update the best performance of a single particle
void updateLocalPerf(double particles[NB_PARTICLE][DATASIZE],
double perf[NB_PARTICLE], double lbest[NB_PARTICLE][DATASIZE],
double lbestperf[NB_PARTICLE], double lbestage[NB_PARTICLE]) {

  int i;                   // FOR-loop counters

  // If current performance of particle better than previous best, update previous best
  for (i = 0; i < NB_PARTICLE; i++) {
    if (perf[i] > lbestperf[i]) {
      copyParticle(lbest[i],particles[i]);
      lbestperf[i] = perf[i];
      lbestage[i] = 1.0;
    }
  }
}

// Find the best result found, set best to the particle, and return the performance
double bestResult(double lbest[NB_PARTICLE][DATASIZE], double lbestperf[NB_PARTICLE], double best[DATASIZE]) {
  double perf;         // Current best performance
  int i;               // FOR-loop counters

  // Start with the first particle as best
  copyParticle(best,lbest[0]);
  perf = lbestperf[0];


  // Iterate through the rest of the particles looking for better results
  for (i = 1; i < NB_PARTICLE; i++) {
    // If current performance of particle better than previous best, update previous best
    if (lbestperf[i] > perf) {
      copyParticle(best,lbest[i]);
      perf = lbestperf[i];
    }
  }

  return perf;
}


// Update the best performance of a particle neighborhood
void updateNBPerf(double lbest[NB_PARTICLE][DATASIZE], double lbestperf[NB_PARTICLE],
		      double nbbest[NB_PARTICLE][DATASIZE], double nbbestperf[NB_PARTICLE],
		      int neighbors[NB_PARTICLE][NB_PARTICLE]) {
  int i,j;                   // FOR-loop counters

  // For each particle, check the best performances of its neighborhood (-NB to NB, with wraparound from swarmsize-1 to 0)
  for (i = 0; i < NB_PARTICLE; i++) {

    nbbestperf[i] = lbestperf[i];

    for (j = 0; j < NB_PARTICLE; j++) {

      // Make sure it's a valid particle
      if (!neighbors[i][j]) continue;

      // If current performance of particle better than previous best, update previous best
      if (lbestperf[j] > nbbestperf[i]) {
      	copyParticle(nbbest[i],lbest[j]);
      	nbbestperf[i] = lbestperf[j];
      }
    }
  }
}

// Find the modulus of an integer
int mod(int num, int base) {
  while (num >= base)
    num -= base;
  while (num < 0)      // Check for if number is negative to
    num += base;
  return num;
}

// Generate random number in [0,1]
double rnd(void) {
  return ((double)rand())/((double)RAND_MAX);
}

// Generate random number in [0,1]
double rnd_rey(void) {
  return ((double)rand())/((double)RAND_MAX);
}

// Randomly position specified robot
void init_pos(int rob_id) {
  static double rnd_posz=0;
  if(rob_id==0){
    rnd_posz = (1.8-ROB_RAD)*rnd() - (1.8-ROB_RAD)/2.0;
  }
  if(!RND_POS){
    //printf("Setting random position for %d\n",rob_id);
    new_rot[rob_id][0] = 0.0;
    new_rot[rob_id][1] = 1.0;
    new_rot[rob_id][2] = 0.0;
    new_rot[rob_id][3] = -1.5708;
    new_loc[rob_id][1] = 0;
    switch(rob_id){
      case 0:
        new_loc[rob_id][0] = -2.9;
        new_loc[rob_id][2] = 0;
        break;
      case 1:
        new_loc[rob_id][0] = -2.9;
        new_loc[rob_id][2] = 0.1;
        break;
      case 2:
        new_loc[rob_id][0] = -2.9;
        new_loc[rob_id][2] = -0.1;
        break;
      case 3:
        new_loc[rob_id][0] = -2.9;
        new_loc[rob_id][2] = 0.2;
        break;
      case 4:
        new_loc[rob_id][0] = -2.9;
        new_loc[rob_id][2] = -0.2;
        break;
      default:
        printf("Error in ini_pos in pos_obs_sup, rob_id%d not in range", rob_id);
        break;
     }
   }
   else{
     new_rot[rob_id][0] = 0.0;
     new_rot[rob_id][1] = 1.0;
     new_rot[rob_id][2] = 0.0;
     new_rot[rob_id][3] = -1.5708;
     new_loc[rob_id][1] = 0;
     new_loc[rob_id][0] = rel_init_pos_robot[rob_id][0];
     new_loc[rob_id][1] = 0.001;
     new_loc[rob_id][2] = rnd_posz + rel_init_pos_robot[rob_id][1];
   }

  wb_supervisor_field_set_sf_vec3f(wb_supervisor_node_get_field(epucks[rob_id],"translation"), new_loc[rob_id]);
  wb_supervisor_field_set_sf_rotation(wb_supervisor_node_get_field(epucks[rob_id],"rotation"), new_rot[rob_id]);
}

void fitness(double weights[ROBOTS][DATASIZE], double fit[ROBOTS]) {
  double buffer[255];
  double *rbuffer;
  int i,j,k;
  /* Send data to robots */
  for (i=0;i<FLOCK_SIZE;i++) {
    init_pos(i);
    for (j=0;j<DATASIZE;j++) {
        if(j>=DATASIZE_LEADER && !(num_leader == i)){ // send to follower TODO
            buffer[j] = weights[0][j];
        }
        if(j < DATASIZE_LEADER && (num_leader == i)){ // send to leader
            buffer[j] = weights[0][j];
        }
    }
    if(num_leader == i){
        wb_emitter_send(emitter[i],(void *)buffer,(DATASIZE_LEADER)*sizeof(double));
    }
    else{
        wb_emitter_send(emitter[i],(void *)buffer,(DATASIZE_FOLLOWER)*sizeof(double));
    }
  }
  wb_supervisor_simulation_reset_physics();

  // Fitness flocking
  double Mfo=0;    // formation control metric
  double v=0;  // velocity of the team towards the goal direction
  double dfo=0; //TODO
  double unused=0;
  int counter = 0; // count how many time the fitness is computed
  double pre_ctr_x = 0;
  double pre_ctr_z = 0;
  double ctr_x = 0;
  double ctr_z = 0;



  // Initialise the center of the flock -----
  for (i=0;i<FLOCK_SIZE;i++) {
      loc[i] = wb_supervisor_field_get_sf_vec3f(wb_supervisor_node_get_field(epucks[i],"translation"));
  }

  //calculate position of the flock center
  for (i=0;i<FLOCK_SIZE;i++) {
       pre_ctr_x += loc[i][0];
       pre_ctr_z += loc[i][2];
  }
  pre_ctr_x /= (double) FLOCK_SIZE;
  pre_ctr_z /= (double) FLOCK_SIZE;

  /* Wait for response */
  printf("Superviser begins Simulation\n");
  while (wb_receiver_get_queue_length(rec[0]) == 0){
    counter++;
    wb_robot_step(TIME_STEP);

    for (i=0; i<FLOCK_SIZE ; i++){
      loc[i]= wb_supervisor_field_get_sf_vec3f(wb_supervisor_node_get_field(epucks[i],"translation"));
      rot[i] = wb_supervisor_field_get_sf_rotation(wb_supervisor_node_get_field(epucks[i],"rotation"));
    }

    //calculate position of the flock center
     ctr_x = 0;
     ctr_z = 0;
     for (i=0;i<FLOCK_SIZE;i++) {
         ctr_x += loc[i][0];
         ctr_z += loc[i][2];
     }
     ctr_x /= (double) FLOCK_SIZE;
     ctr_z /= (double) FLOCK_SIZE;

    //calculate v(t)
    v = (double) sqrt(pow(pre_ctr_x-ctr_x,2)+pow(pre_ctr_z-ctr_z,2))/dmax*WEIGHT_V;
    pre_ctr_x = ctr_x; // save the center of the flock
    pre_ctr_z = ctr_z;

    //compute final flocking metric
    Mfo+=dfo*v;
  }

   Mfo/=counter; // normalization
   printf("End simulation superviser.\n");

   /* Get fitness values from robots */
   //printf("fitness avoidance : \n");

   for (i=0;i<FLOCK_SIZE;i++) {
      rbuffer = (double *)wb_receiver_get_data(rec[i]);
      //printf("rob%d :%lf  ", i, rbuffer[0]);
      unused += rbuffer[0];
      wb_receiver_next_packet(rec[i]);
  }


  fit[0] = Mfo; //TODO
  printf("\nfit formation (%0.2lf) = dfo(%0.2lf) + v (%0.2lf)\n",fit[0], dfo, v);
  printf("\n -------------------------------------------------------------------\n");
}

// Find the current performance of the swarm.
// Higher performance is better
void findPerformance(double particles[NB_PARTICLE][DATASIZE], double perf[NB_PARTICLE],
		     double age[NB_PARTICLE], char type) {
  double particles_sim[ROBOTS][DATASIZE]; // Assign 1 particle for each robot of the flock in simulation
  double fit[ROBOTS];
  int i,j,k;                   // FOR-loop counters
  for (i = 0; i < NB_PARTICLE; i+=ROBOTS) {
    for (j=0;j<ROBOTS && i+j<NB_PARTICLE;j++) {
      sprintf(label2,"Particle: %d\n", i+j);
      wb_supervisor_set_label(1,label2,0.01,0.05,0.05,0xffffff,0,FONT);
      for (k=0;k<DATASIZE;k++){
        particles_sim[j][k] = particles[i+j][k];
      }
    }
    // USER MUST IMPLEMENT FITNESS FUNCTION
    if (type == EVOLVE_AVG) {
      fitness(particles_sim,fit);
      for (j=0;j<ROBOTS && i+j<NB_PARTICLE;j++) {
      	perf[i+j] = ((age[i+j]-1.0)*perf[i+j] + fit[j])/age[i+j];
      	age[i+j]++;
      }
    }
    else if (type == EVOLVE) {
      fitness(particles_sim,fit);
      for (j=0;j<ROBOTS && i+j<NB_PARTICLE;j++)
	       perf[i+j] = fit[j];
    }
    else if (type == SELECT) {
      for (j=0;j<ROBOTS && i+j<NB_PARTICLE;j++)
          perf[i+j] = 0.0;
      for (k=0;k<5;k++) {
	fitness(particles_sim,fit);
    	for (j=0;j<ROBOTS && i+j<NB_PARTICLE;j++)
    	  perf[i+j] += fit[j];
      }
      for (j=0;j<ROBOTS && i+j<NB_PARTICLE;j++) {
	       perf[i+j] /= 5.0;
      }
    }
  }

}


/* Particle swarm optimization function */
void limit_weight(double min, double max, double particles[NB_PARTICLE][DATASIZE], int i, int j){
  if (particles[i][j] > max){
    particles[i][j] = max;
  }
  if (particles[i][j] < min){
     particles[i][j] = min;
  }
}


/* Particle swarm optimization function */
void pso(double best_weight[DATASIZE]){
  int i,j,k;                                // FOR-loop counters
  double min, max;                         // Bound for random init of weights
  double particles[NB_PARTICLE][DATASIZE];// Swarm of particles
  double perf[NB_PARTICLE];                 // Current local performance of swarm
  double lbest[NB_PARTICLE][DATASIZE];    // Current best local swarm
  double lbestperf[NB_PARTICLE];            // Current best local performance
  double lbestage[NB_PARTICLE];             // Life length of best local swarm
  double nbbest[NB_PARTICLE][DATASIZE];   // Current best neighborhood
  double nbbestperf[NB_PARTICLE];           // Current best neighborhood performance
  double v[NB_PARTICLE][DATASIZE];        // Velocity of particles
  int neighbors[NB_PARTICLE][NB_PARTICLE];  // Neighbor matrix
  double bestperf;                          // Performance of evolved solution

  // Seed the random generator
  srand(time(NULL));

  // Setup neighborhood (Topological)
  printf("Init neighborhood\n");
  for (i = 0; i < NB_PARTICLE; i++) {
    for (j = 0; j < NB_PARTICLE; j++) {
      if (mod(i-j+NB_NEIGHBOURS, NB_PARTICLE) <= 2*NB_NEIGHBOURS){
        neighbors[i][j] = 1;
        }
      else{
        neighbors[i][j] = 0;
        }
    }
  }

  sprintf(label, "Iteration: 0");
  wb_supervisor_set_label(0,label,0.01,0.01,0.1,0xffffff,0,FONT);

  // Initialize the swarm
  for (i = 0; i < NB_PARTICLE; i++) {
    for (j = 0; j < DATASIZE; j++) {

      // Assign initial value with hand tuned weights
      if(PRIOR_KNOWLEDGE){
          // Assign first weight with prior knowledge
          if(j>=DATASIZE_LEADER){
            particles[i][j]=prior_knowledge_follower[j];
          }
          else{
            particles[i][j]=prior_knowledge_leader[j];
          }
          lbest[i][j] = particles[i][j];           // Best configurations are initially current configurations
          nbbest[i][j] = particles[i][j];

      }

      // Randomly assign initial value in [min,max]
      else{
          // min and max for flocking weight init
            min=MIN_BRAITEN;
            max=MAX_BRAITEN;

          particles[i][j] = (max-min)*rnd()+min;
          lbest[i][j] = particles[i][j];           // Best configurations are initially current configurations
          nbbest[i][j] = particles[i][j];
          v[i][j] = 2.0*VMAX*rnd()-VMAX;         // Random initial velocity
      }
    }
  }
  // Best performances are initially current performances
  findPerformance(particles, perf, NULL, EVOLVE);
  for (i = 0; i < NB_PARTICLE; i++) {
    lbestperf[i] = perf[i];
    lbestage[i] = 1.0;  // One performance so far
    nbbestperf[i] = perf[i];
  }
  // Find best neighborhood performances
  updateNBPerf(lbest,lbestperf,nbbest,nbbestperf,neighbors);

  printf("****** Swarm initialized\n");

  // Run optimization
  for (k = 0; k < ITS_COEFF*ITS_PSO; k++) {
    sprintf(label, "Iteration: %d",k+1);
    wb_supervisor_set_label(0,label,0.01,0.01,0.1,0xffffff,0,FONT);
    // Update preferences and generate new particles
    for (i = 0; i < NB_PARTICLE; i++) {
      for (j = 0; j < DATASIZE; j++) {

          v[i][j] *= DAMPING;
          v[i][j] += LWEIGHT*rnd()*(lbest[i][j] - particles[i][j]) + NBWEIGHT*rnd()*(nbbest[i][j] - particles[i][j]);
          particles[i][j] += v[i][j]; // Move particles
      }
    }



     // RE-EVALUATE PERFORMANCES OF PREVIOUS BESTS
#if NOISY == 1
    findPerformance(lbest,lbestperf,lbestage,EVOLVE_AVG);
#endif


    // Find new performance
    findPerformance(particles,perf,NULL,EVOLVE);

    // Update best local performance
    updateLocalPerf(particles,perf,lbest,lbestperf,lbestage);

    // Update best neighborhood performance
    updateNBPerf(lbest,lbestperf,nbbest,nbbestperf,neighbors);

    double temp[DATASIZE];
    bestperf = bestResult(lbest,lbestperf,temp);
    printf("Best performance of the iteration : %lf\n",bestperf);
   }
   findPerformance(lbest,lbestperf,NULL,SELECT);
   bestperf = bestResult(lbest,lbestperf,best_weight);
   printf("_____Best performance found\n");
   printf("Performance over %d iterations: %lf\n",ITS_PSO,bestperf);
   sprintf(label, "Optimization process over.");
   wb_supervisor_set_label(0,label,0.01,0.01,0.1,0xffffff,0,FONT);
}

int main() {
  reset();
  double best_weight[DATASIZE]; // best solution of pso
  double fit, w[ROBOTS][DATASIZE], f[ROBOTS];
  int i,j,k;  // Counter variables
   // Get result of optimization

   // Do N_RUNS runs and send the best controller found to the robot
    for (j=0;j<N_RUNS;j++) {
       pso(best_weight);

    // Set robot weights to optimization results
        fit = 0.0;
        for (i=0;i<ROBOTS;i++) {
            for (k=0;k<DATASIZE;k++)
              w[i][k] = best_weight[k];
        }
        // Run FINALRUN tests and calculate average
        printf("Running final runs\n");
        for (i=0;i<FINALRUNS;i+=ROBOTS) {
            fitness(w,f);
            for (k=0;k<ROBOTS && i+k<FINALRUNS;k++) {
                //fitvals[i+k] = f[k];
                fit += f[k];
            }
        }
        fit /= FINALRUNS;  // average over the 10 runs

        printf("Average Performance: %.3f\n",fit);
    }

    /* Wait forever */
    while (1){
        fitness(w,f);
    }
    return 0;
}

