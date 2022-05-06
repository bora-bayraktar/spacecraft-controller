#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <stdbool.h>

#include "queue.c"

#define t 2
#define MAX_SPACECRAFT 3*(simulationTime + 1)

int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 40; // frequency of emergency
float p = 0.2;               // probability of a ground job (launch & assembly)
int n = 0;                 // start logging after n seconds

Queue *launch_queue;
Queue *land_queue;
Queue *assembly_queue;
Queue *emergency_queue;
Queue *padA_queue;
Queue *padB_queue;

int ID;
FILE *job_log;
char file_name[] = "job.log"; 
bool padA_working = FALSE;
bool padB_working = FALSE;
time_t padA_work_time;
time_t padB_work_time;
char padA_work_job;
char padB_work_job;

pthread_mutex_t launch_queue_mutex;
pthread_mutex_t land_queue_mutex;
pthread_mutex_t assembly_queue_mutex;
pthread_mutex_t emergency_queue_mutex;
pthread_mutex_t padA_queue_mutex;
pthread_mutex_t padB_queue_mutex;
pthread_mutex_t padA_work_mutex;
pthread_mutex_t padB_work_mutex;
pthread_mutex_t ID_mutex;
pthread_mutex_t file_mutex;

time_t start_time, end_time; 

void* LandingJob(void *arg); 
void* LaunchJob(void *arg);
void* EmergencyJob(void *arg); 
void* AssemblyJob(void *arg); 
void* ControlTower(void *arg); 
void* PadA(void *arg);
void* PadB(void *arg);
void* Print_Jobs_Terminal(void *arg);
void* KeepLog(Job job);
int FindPadARemainingTime();
int FindPadBRemainingTime();
double probability();

// pthread sleeper function
int pthread_sleep(int seconds) {
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    struct timespec timetoexpire;

    if(pthread_mutex_init(&mutex,NULL)) {
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL)) {
        return -1;
    }
    
    struct timeval tp;
    //When to expire is an absolute time, so get the current time and add it to our delay time
    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;
    
    pthread_mutex_lock (&mutex);
    int res = pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock (&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);
    
    //Upon successful completion, a value of zero shall be returned
    return res;
}

int main(int argc,char **argv) {
    // -p (float) => sets p
    // -t (int) => simulation time in seconds
    // -s (int) => change the random seed
    // -n (int) => change the start log time
    for(int i=1; i<argc; i++){
        if(!strcmp(argv[i], "-p")) {p = atof(argv[++i]);}
        else if(!strcmp(argv[i], "-t")) {simulationTime = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-s"))  {seed = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-n"))  {n = atoi(argv[++i]);}
    }
    
    srand(seed); // feed the seed
    job_log = fopen(file_name,"w");
    fprintf(job_log,"EventID  Status  Request_Time  End_Time  Turnaround_Time  Pad\n");
    fprintf(job_log,"-------------------------------------------------------------\n");
    fclose(job_log);
    
    /* Queue usage example
        Queue *myQ = ConstructQueue(1000);
        Job j;
        j.ID = myID;
        j.type = 2;
        Enqueue(myQ, j);
        Job ret = Dequeue(myQ);
        DestructQueue(myQ);
    */

    launch_queue = ConstructQueue(MAX_SPACECRAFT);
    land_queue = ConstructQueue(MAX_SPACECRAFT);
    assembly_queue = ConstructQueue(MAX_SPACECRAFT);
    emergency_queue = ConstructQueue(MAX_SPACECRAFT);

    padA_queue = ConstructQueue(MAX_SPACECRAFT);
    padB_queue = ConstructQueue(MAX_SPACECRAFT);

    pthread_mutex_init(&launch_queue_mutex, NULL);
    pthread_mutex_init(&land_queue_mutex, NULL);
    pthread_mutex_init(&assembly_queue_mutex, NULL);
    pthread_mutex_init(&emergency_queue_mutex, NULL);
    pthread_mutex_init(&padA_queue_mutex, NULL);
    pthread_mutex_init(&padB_queue_mutex, NULL);
    pthread_mutex_init(&padA_work_mutex, NULL);
    pthread_mutex_init(&padB_work_mutex, NULL);
    pthread_mutex_init(&ID_mutex, NULL);

    pthread_t launch_thread;
    pthread_t land_thread;
    pthread_t assembly_thread;
    pthread_t emergency_thread;
    pthread_t padA_thread;
    pthread_t padB_thread;
    pthread_t control_tower_thread;
    pthread_t print_jobs_terminal_thread;

    ID = 1;

    Job job = { .ID = ID, .request_time = time(NULL), .type = 'D', .pad = 'A' };
    ID++;
    Enqueue(launch_queue, job);

    time(&start_time);
    end_time = start_time + simulationTime + 1;

    pthread_create(&launch_thread, NULL, LaunchJob, NULL);
    pthread_create(&land_thread, NULL, LandingJob, NULL);
    pthread_create(&assembly_thread, NULL, AssemblyJob, NULL);
    pthread_create(&emergency_thread, NULL, EmergencyJob, NULL);
    pthread_create(&padA_thread, NULL, PadA, NULL);
    pthread_create(&padB_thread, NULL, PadB, NULL);
    pthread_create(&control_tower_thread, NULL, ControlTower, NULL);
    pthread_create(&print_jobs_terminal_thread, NULL, Print_Jobs_Terminal, NULL);

    pthread_join(launch_thread, NULL);
    pthread_join(land_thread, NULL);
    pthread_join(assembly_thread, NULL);
    pthread_join(emergency_thread, NULL);
    pthread_join(padA_thread, NULL);
    pthread_join(padB_thread, NULL);
    pthread_join(control_tower_thread, NULL);
    pthread_join(print_jobs_terminal_thread, NULL);

    DestructQueue(launch_queue);
    DestructQueue(land_queue);
    DestructQueue(assembly_queue);
    DestructQueue(emergency_queue);
    DestructQueue(padA_queue);
    DestructQueue(padB_queue);

    pthread_mutex_destroy(&launch_queue_mutex);
    pthread_mutex_destroy(&land_queue_mutex);
    pthread_mutex_destroy(&assembly_queue_mutex);
    pthread_mutex_destroy(&emergency_queue_mutex);
    pthread_mutex_destroy(&padA_queue_mutex);
    pthread_mutex_destroy(&padB_queue_mutex);
    pthread_mutex_destroy(&ID_mutex);

    return 0;
}

double probability() {
    return (double) rand() / (double) RAND_MAX;
}

// the function that creates plane threads for landing
void* LandingJob(void *arg) {
    while (end_time > time(NULL)) {
        pthread_sleep(1*t);

        if (probability() < 1 - p) {
            pthread_mutex_lock(&ID_mutex);
            Job job = { .ID = ID,  .type = 'L', .request_time = time(NULL) };
            ID++;
            pthread_mutex_unlock(&ID_mutex);
            
            pthread_mutex_lock(&land_queue_mutex);
            Enqueue(land_queue, job);
            pthread_mutex_unlock(&land_queue_mutex);
        }
    }
}

// the function that creates plane threads for departure
void* LaunchJob(void *arg) {
    while (end_time > time(NULL)) {
        pthread_sleep(1*t);

        if (probability() < p / 2) {
            pthread_mutex_lock(&ID_mutex);
            Job job = { .ID = ID,  .type = 'D', .request_time = time(NULL) };
            ID++;
            pthread_mutex_unlock(&ID_mutex);
            
            pthread_mutex_lock(&launch_queue_mutex);
            Enqueue(launch_queue, job);
            pthread_mutex_unlock(&launch_queue_mutex);
        }
    }
}

// the function that creates plane threads for emergency landing
void* AssemblyJob(void *arg){
    while (end_time > time(NULL)) {
        pthread_sleep(1*t);

        if (probability() < p / 2) {
            pthread_mutex_lock(&ID_mutex);
            Job job = { .ID = ID,  .type = 'A', .request_time = time(NULL) };
            ID++;
            pthread_mutex_unlock(&ID_mutex);
            
            pthread_mutex_lock(&assembly_queue_mutex);
            Enqueue(assembly_queue, job);
            pthread_mutex_unlock(&assembly_queue_mutex);
        }
    }
}

// the function that creates plane threads for emergency landing
void* EmergencyJob(void *arg) {
    int counter = 0;
    while (end_time > time(NULL)) {
        pthread_sleep(1*t);
        counter++;
        if(counter == 40) {
            pthread_mutex_lock(&ID_mutex);
            Job job1 = { .ID = ID,  .type = 'E', .request_time = time(NULL) };
            ID++;
            Job job2 = { .ID = ID,  .type = 'E', .request_time = time(NULL) };
            ID++;
            pthread_mutex_unlock(&ID_mutex);
            
            pthread_mutex_lock(&emergency_queue_mutex);
            Enqueue(emergency_queue, job1);
            Enqueue(emergency_queue, job2);
            pthread_mutex_unlock(&emergency_queue_mutex);
            counter = 0;
        }

    }
}

void* PadA(void *arg) {
    while (end_time > time(NULL)) {
        pthread_mutex_lock(&padA_queue_mutex);
        if (isEmpty(padA_queue)) {
            pthread_mutex_unlock(&padA_queue_mutex);
            pthread_sleep(t);
        } else {
            Job job = padA_queue->head->data;

            pthread_mutex_unlock(&padA_queue_mutex);
            if (job.type == 'D') {
                pthread_mutex_lock(&padA_work_mutex);
                padA_working = TRUE;
                padA_work_time = time(NULL);
                padA_work_job = 'D';
                pthread_mutex_unlock(&padA_work_mutex);
                pthread_sleep(2*t);
            } else if (job.type == 'L') {
                pthread_mutex_lock(&padA_work_mutex);
                padA_working = TRUE;
                padA_work_time = time(NULL);
                padA_work_job = 'L';
                pthread_mutex_unlock(&padA_work_mutex);
                pthread_sleep(1*t);
            } else if (job.type == 'E') {
                pthread_mutex_lock(&padA_work_mutex);
                padA_working = TRUE;
                padA_work_time = time(NULL);
                padA_work_job = 'E';
                pthread_mutex_unlock(&padA_work_mutex);
                pthread_sleep(1*t);
            }
            job.end_time = time(NULL);

            pthread_mutex_lock(&padA_queue_mutex);
            Dequeue(padA_queue);
            pthread_mutex_unlock(&padA_queue_mutex);

            pthread_mutex_lock(&padA_work_mutex);
            padA_working = FALSE;
            pthread_mutex_unlock(&padA_work_mutex);

            KeepLog(job);
        }
    }
}

void* PadB(void *arg) {
    while (end_time > time(NULL)) {
        pthread_mutex_lock(&padB_queue_mutex);
        if (isEmpty(padB_queue)) {
            pthread_mutex_unlock(&padB_queue_mutex);
            pthread_sleep(t);
        } else {
            Job job = padB_queue->head->data;

            pthread_mutex_unlock(&padB_queue_mutex);
            if (job.type == 'A') {
                pthread_mutex_lock(&padB_work_mutex);
                padB_working = TRUE;
                padB_work_time = time(NULL);
                padB_work_job = 'A';
                pthread_mutex_unlock(&padB_work_mutex);
                pthread_sleep(6*t);
            } else if (job.type == 'L') {
                pthread_mutex_lock(&padB_work_mutex);
                padB_working = TRUE;
                padB_work_time = time(NULL);
                padB_work_job = 'L';
                pthread_mutex_unlock(&padB_work_mutex);
                pthread_sleep(1*t);
            } else if (job.type == 'E') {
                pthread_mutex_lock(&padB_work_mutex);
                padB_working = TRUE;
                padB_work_time = time(NULL);
                padB_work_job = 'E';
                pthread_mutex_unlock(&padB_work_mutex);
                pthread_sleep(1*t);
            }

            job.end_time = time(NULL);
            
            pthread_mutex_lock(&padB_queue_mutex);
            Dequeue(padB_queue);
            pthread_mutex_unlock(&padB_queue_mutex);

            pthread_mutex_lock(&padB_work_mutex);
            padB_working = FALSE;
            pthread_mutex_unlock(&padB_work_mutex);

            KeepLog(job);
        }
    }
}

// the function that controls the air traffic
void* ControlTower(void *arg)  {
    while (end_time > time(NULL)) {

        pthread_mutex_lock(&emergency_queue_mutex);
        while (!isEmpty(emergency_queue)) {
            pthread_mutex_lock(&padA_queue_mutex);
            pthread_mutex_lock(&padB_queue_mutex);
            pthread_mutex_lock(&padA_work_mutex);
            pthread_mutex_lock(&padB_work_mutex);
            if(!padA_working) {
                Job job = Dequeue(emergency_queue);
                job.pad = 'A';
                EnqueueFirst(padA_queue, job);
            } else if(!padB_working) {
                Job job = Dequeue(emergency_queue);
                job.pad = 'A';
                EnqueueFirst(padA_queue, job);
            } else {
                int padA_remaining_time = FindPadARemainingTime();
                int padB_remaining_time = FindPadBRemainingTime();
                if(padA_remaining_time >= padB_remaining_time) {
                    Job job = Dequeue(emergency_queue);
                    job.pad = 'B';
                    EnqueueSecond(padB_queue, job);
                } else {
                    Job job = Dequeue(emergency_queue);
                    job.pad = 'A';
                    EnqueueSecond(padA_queue, job);
                }
            }
            pthread_mutex_unlock(&padA_queue_mutex);
            pthread_mutex_unlock(&padB_queue_mutex);
            pthread_mutex_unlock(&padA_work_mutex);
            pthread_mutex_unlock(&padB_work_mutex);
        }
        pthread_mutex_unlock(&emergency_queue_mutex);



        pthread_mutex_lock(&land_queue_mutex);
        if (!isEmpty(land_queue) && (launch_queue->size < 3) && (assembly_queue->size < 3)) {
            pthread_mutex_lock(&padA_queue_mutex);
            pthread_mutex_lock(&padB_queue_mutex);

            NODE *currentA = padA_queue->head;
            int padA_sum = 0;
            while (currentA != NULL) {
                if (currentA->data.type == 'D') {
                    padA_sum += 2*t;
                } else {
                    padA_sum += 1*t;
                }

                currentA = currentA->prev;
            }

            NODE *currentB = padB_queue->head;
            int padB_sum = 0;
            while (currentB != NULL) {
                if (currentB->data.type == 'A') {
                    padB_sum += 6*t;
                } else {
                    padB_sum += 1*t;
                }

                currentB = currentB->prev;
            }

            if (padA_sum >= padB_sum) {
                Job job = Dequeue(land_queue);
                job.pad = 'B';
                Enqueue(padB_queue, job);
            } else {
                Job job = Dequeue(land_queue);
                job.pad = 'A';
                Enqueue(padA_queue, job);
            }

            pthread_mutex_unlock(&padA_queue_mutex);
            pthread_mutex_unlock(&padB_queue_mutex);
        }
        pthread_mutex_unlock(&land_queue_mutex);
        
        pthread_mutex_lock(&launch_queue_mutex);
        pthread_mutex_lock(&padA_queue_mutex);
        if (isEmpty(padA_queue) && !isEmpty(launch_queue) || launch_queue->size >= 3) {
            Job job = Dequeue(launch_queue);
            job.pad = 'A';
            Enqueue(padA_queue, job);
        }
        pthread_mutex_unlock(&padA_queue_mutex);
        pthread_mutex_unlock(&launch_queue_mutex);

        pthread_mutex_lock(&assembly_queue_mutex);
        pthread_mutex_lock(&padB_queue_mutex);
        if (isEmpty(padB_queue) && !isEmpty(assembly_queue) || assembly_queue->size >= 3) {
            Job job = Dequeue(assembly_queue);
            job.pad = 'B';
            Enqueue(padB_queue, job);
        }
        pthread_mutex_unlock(&padB_queue_mutex);
        pthread_mutex_unlock(&assembly_queue_mutex);

    }
}


void* Print_Jobs_Terminal(void *arg)  {
    while(time(NULL) < (start_time + n));
    int printTime = n;
    while(time(NULL) < end_time) {
        int landingJobsIDArray[100];
        int totalLands = 0;
        int launchingJobsIDArray[100];
        int totalLaunchs = 0;
        int assemblyJobsIDArray[100];
        int totalAssemblies = 0;
        int emergencyJobsIDArray[100];
        int totalEmergencies = 0;
        int padAJobsIDArray[100];
        char padAJobsTypeArray[100];
        int totalPadA = 0;
        int padBJobsIDArray[100];
        char padBJobsTypeArray[100];
        int totalPadB = 0;
        pthread_mutex_lock(&padA_queue_mutex);
        NODE *currentA = padA_queue->head;
        while (currentA != NULL) {
            padAJobsIDArray[totalPadA] = currentA->data.ID;
            padAJobsTypeArray[totalPadA] = currentA->data.type;
            totalPadA++;
            if (currentA->data.type == 'D') {
                launchingJobsIDArray[totalLaunchs] = currentA->data.ID;
                totalLaunchs++;
            } else if (currentA->data.type == 'L') {
                landingJobsIDArray[totalLands] = currentA->data.ID;
                totalLands++;
            } else if (currentA->data.type == 'E') {
                emergencyJobsIDArray[totalEmergencies] = currentA->data.ID;
                totalEmergencies++;
            }
            currentA = currentA->prev;
        }
        pthread_mutex_unlock(&padA_queue_mutex);
        
        pthread_mutex_lock(&padB_queue_mutex);
        NODE *currentB = padB_queue->head;
        while (currentB != NULL) {
            padBJobsIDArray[totalPadB] = currentB->data.ID;
            padBJobsTypeArray[totalPadB] = currentB->data.type;
            totalPadB++;
            if (currentB->data.type == 'A') {
                assemblyJobsIDArray[totalAssemblies] = currentB->data.ID;
                totalAssemblies++;
            } else if (currentB->data.type == 'L') {
                landingJobsIDArray[totalLands] = currentB->data.ID;
                totalLands++;
            } else if (currentB->data.type == 'E') {
                emergencyJobsIDArray[totalEmergencies] = currentB->data.ID;
                totalEmergencies++;
            }
            currentB = currentB->prev;
        }
        pthread_mutex_unlock(&padB_queue_mutex);

        pthread_mutex_lock(&land_queue_mutex);
        NODE *currentLand = land_queue->head;
        while (currentLand != NULL) {
            landingJobsIDArray[totalLands] = currentLand->data.ID;
            totalLands++;
            currentLand = currentLand->prev;
        }
        pthread_mutex_unlock(&land_queue_mutex);

        pthread_mutex_lock(&launch_queue_mutex);
        NODE *currentLaunch = launch_queue->head;
        while (currentLaunch != NULL) {
            launchingJobsIDArray[totalLaunchs] = currentLaunch->data.ID;
            totalLaunchs++;
            currentLaunch = currentLaunch->prev;
        }
        pthread_mutex_unlock(&launch_queue_mutex);

        pthread_mutex_lock(&assembly_queue_mutex);
        NODE *currentAssembly = assembly_queue->head;
        while (currentAssembly != NULL) {
            assemblyJobsIDArray[totalAssemblies] = currentAssembly->data.ID;
            totalAssemblies++;
            currentAssembly = currentAssembly->prev;
        }
        pthread_mutex_unlock(&assembly_queue_mutex);

        pthread_mutex_lock(&emergency_queue_mutex);
        NODE *currentEmergency = emergency_queue->head;
        while (currentEmergency != NULL) {
            emergencyJobsIDArray[totalEmergencies] = currentEmergency->data.ID;
            totalEmergencies++;
            currentEmergency = currentEmergency->prev;
        }
        pthread_mutex_unlock(&emergency_queue_mutex);

        printf("At %d sec landing    : ",printTime);
        for(int i = 0; i < totalLands; i++) {
            printf("%d ",landingJobsIDArray[i]);
        }
        printf("\n");

        printf("At %d sec launch     : ",printTime);
        for(int i = 0; i < totalLaunchs; i++) {
            printf("%d ",launchingJobsIDArray[i]);
        }
        printf("\n");

        printf("At %d sec assembly   : ",printTime);
        for(int i = 0; i < totalAssemblies; i++) {
            printf("%d ",assemblyJobsIDArray[i]);
        }
        printf("\n");

        printf("At %d sec emergency  : ",printTime);
        for(int i = 0; i < totalEmergencies; i++) {
            printf("%d ",emergencyJobsIDArray[i]);
        }
        printf("\n");

        printf("At %d sec padA       : ",printTime);
        for(int i = 0; i < totalPadA; i++) {
            printf("%d(%c) ",padAJobsIDArray[i],padAJobsTypeArray[i]);
        }
        printf("\n");

        printf("At %d sec padB       : ",printTime);
        for(int i = 0; i < totalPadB; i++) {
            printf("%d(%c) ",padBJobsIDArray[i],padBJobsTypeArray[i]);
        }
        printf("\n\n");
        printTime++;
        pthread_sleep(1);
    }
}

void* KeepLog(Job job) {
    pthread_mutex_lock(&file_mutex);
    job_log = fopen(file_name,"a");
    fprintf(job_log,"%-9d%-8c%-14ld%-10ld%-17d%c\n",job.ID,job.type,job.request_time-start_time,job.end_time-start_time,job.end_time-job.request_time,job.pad);
    fclose(job_log);
    pthread_mutex_unlock(&file_mutex);
}

int FindPadARemainingTime() {
    int emergency_time = 0;
    if(padA_queue->head->prev != NULL && padA_queue->head->prev->data.type == 'E') {
        emergency_time = 1*t;
    }
    if(padA_work_job == 'L') {
        return padA_work_time+1*t+emergency_time-time(NULL);
    } else if (padA_work_job == 'D') {
        return padA_work_time+2*t+emergency_time-time(NULL);
    } else {
        return padA_work_time+1*t+emergency_time-time(NULL);
    }
}

int FindPadBRemainingTime() {
    int emergency_time = 0;
    if(padB_queue->head->prev != NULL && padB_queue->head->prev->data.type == 'E') {
        emergency_time = 1*t;
    }
    if(padB_work_job == 'L') {
        return padB_work_time+1*t+emergency_time-time(NULL);
    } else if (padB_work_job == 'A') {
        return padB_work_time+6*t+emergency_time-time(NULL);
    } else {
        return padB_work_time+1*t+emergency_time-time(NULL);
    }
}