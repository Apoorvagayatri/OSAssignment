#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <string.h>
#include <pthread.h>

int found=0;
pthread_mutex_t mutex;
typedef struct TurnChangeResponse {
long mtype; 
int turnNumber; 
int newPassengerRequestCount;
int finished; 
} TurnChangeResponse;

typedef struct TurnChangeRequest{
long mtype; 
int droppedPassengersCount;
int pickedUpPassengersCount;
} TurnChangeRequest;


typedef struct SolverRequest {
 long mtype;
 int elevatorNumber;
 char authStringGuess[21];
} SolverRequest;

typedef struct SolverResponse {
 long mtype;
 int guessIsCorrect; 
} SolverResponse;


typedef struct PassengerRequest{
    int requestId;
    int startFloor;
    int requestedFloor;
} PassengerRequest;


typedef struct MainSharedMemory {
char authStrings[100][21];
char elevatorMovementInstructions[100];
PassengerRequest newPassengerRequests[30];
int elevatorFloors [100];
int droppedPassengers[1000];
int pickedUpPassengers[1000][2];
}MainSharedMemory;

typedef struct Passenger{
    int requestId;
    int requestedFloor;
}Passenger;

typedef struct ElevatorData{
    Passenger passengers[20];
    int count;
    int floor;
    int targetFloor;
    char direction;
}ElevatorData;


typedef struct RequestQueueData{
    int requestId;
    int startFloor;
    int requestedFloor;
    int isPickedUp;
    int isCompleted;
}RequestQueueData;

typedef struct ThreadData{
    char ** stringset;
    int starti;
    int endi;
    int elevatorNumber;
    int solqid;
    MainSharedMemory* mainShmPtr;

}ThreadData;

void* threadfunction(void* arg){

    ThreadData* data= (ThreadData*)arg;

    SolverRequest solrequest;

    solrequest.mtype=2;
    solrequest.elevatorNumber=data->elevatorNumber;

    if(msgsnd(data->solqid,&solrequest,sizeof(solrequest)-sizeof(long),0)==-1){
        printf("Error in msgsnd to the solver while setting the elevator");
        pthread_exit(NULL);
    }



    for(int i=data->starti;i<data->endi;i++){
        pthread_mutex_lock(&mutex);

        if(found){
            pthread_mutex_unlock(&mutex);
            break;
        }

        pthread_mutex_unlock(&mutex);

        solrequest.mtype=3;
        strcpy(solrequest.authStringGuess,data->stringset[i]);


        if(msgsnd(data->solqid,&solrequest,sizeof(solrequest)-sizeof(long),0)==-1){
                        printf("Error in msgsnd of solver for guessing string");
                        pthread_exit(NULL);
        }

        SolverResponse solresponse;

        if(msgrcv(data->solqid,&solresponse,sizeof(solresponse)-sizeof(long),4,0)==-1){
             printf("Error in msgrcv of solver for guessing string");
                        pthread_exit(NULL);
        }

        if(solresponse.guessIsCorrect==1){
            pthread_mutex_lock(&mutex);
            // printf("\n \n Guess is correct\n \n");
            found=1;
            strcpy(data->mainShmPtr->authStrings[data->elevatorNumber],data->stringset[i]);
            pthread_mutex_unlock(&mutex);
            break;
            
        }


    }


pthread_exit(NULL);

}

void stringGuesser(MainSharedMemory* mainShmPtr,char ** stringset, int totalstrings, int * solqid, int M, int elevatorNumber){
    int perSolvernum= totalstrings/M;
    int remainder= totalstrings%M;
    
    found=0;

    pthread_t threads[M];
    ThreadData threaddata[M];

    SolverRequest request;
    request.mtype=2;
    request.elevatorNumber=elevatorNumber;


    for(int i=0;i<M;i++){
        int start=i*perSolvernum;
        int end=start+perSolvernum;

        if(i==M-1){
            end+=remainder;
        }

        threaddata[i].stringset=stringset;
        threaddata[i].starti=start;
        threaddata[i].solqid=solqid[i];
        threaddata[i].endi=end;
        threaddata[i].elevatorNumber=elevatorNumber;
        threaddata[i].mainShmPtr=mainShmPtr;
        


        pthread_create(&threads[i],NULL,threadfunction,&threaddata[i]);

        


    }

    for(int i=0;i<M;i++){
        pthread_join(threads[i],NULL);
    }
    
    for(int m=0;m<totalstrings;m++){
        free(stringset[m]);
        
    }
    free(stringset);
    pthread_mutex_destroy(&mutex);

}




char** getAllPossibleStrings(int len){
    
    int totalstrings=1;

    for(int i=0;i<len;i++){
        totalstrings=totalstrings*6;
    }

    char** stringset;
    stringset=(char**)malloc(totalstrings*sizeof(char*));

    for(int i=0;i<totalstrings;i++){
            stringset[i]=(char*)malloc((len+1)*sizeof(char));
        }
    for(int i=0;i<totalstrings;i++){
        stringset[i][len]='\0';
    }
    

    for(int i=0;i<totalstrings;i++){

        int subset=i;

        for(int m=0;m<len;m++){
            stringset[i][m]='a'+(subset%6);
            subset/=6;
        }
        
    

        


    }

    return stringset;


}




int main(){
    FILE *file=fopen("input.txt","r");

    if(file==NULL){
        printf("Error in fopen.\n");
        return 1;

    }

    int N,K,M,T;
    int keyshm,keymsgq;


    fscanf(file,"%d",&N); 
    fscanf(file,"%d",&K); 
    fscanf(file,"%d",&M); 
    fscanf(file,"%d",&T); 
    fscanf(file,"%d",&keyshm);
    fscanf(file,"%d",&keymsgq);
    
    int solkey[M];


    for(int i=0;i<M;i++){
        fscanf(file,"%d",&solkey[i]);
    }
    
     printf("here T is %d.\n ",T);
    // printf("here M is %d",M);




    MainSharedMemory* mainShmPtr;
    int shmId= shmget(keyshm,sizeof(MainSharedMemory),0666);
    if(shmId==-1){
        printf("Error in shmget");
        return 1;
    }

    mainShmPtr = shmat(shmId, NULL, 0);

    int msgqid=msgget(keymsgq,0666);

    if(msgqid==-1){
        printf("Error in msgget from helper");
        return 1;
    }


    int solqid[M];


    for(int i=0;i<M;i++){
        solqid[i]=msgget(solkey[i],0666);
    }

    ElevatorData* elevators=malloc(N*sizeof(ElevatorData));
    char elevatorMovementInstructions[N];
    
    for(int i=0;i<N;i++){
        elevatorMovementInstructions[i]='s';
    }

    for(int i=0;i<N;i++){
        //initiaize
        elevators[i].count=0;
        elevators[i].floor=0;
        elevators[i].targetFloor=0;
    }
    int droppedPassengers[1000];
    int pickedUpPassengers[1000][2];


    for(int i=0;i<1000;i++){
        pickedUpPassengers[i][0]=0;
        pickedUpPassengers[i][1]=0;
    }

    for(int i=0;i<1000;i++){
        droppedPassengers[i]=0;
        
    }

    RequestQueueData* requests=malloc(1000 * sizeof(RequestQueueData));
    int requestCount=0;

    int turn=0;
    int droppedinturn[N];
    for(int i=0;i<N;i++){
            droppedinturn[i]=0;
        }
    while(1){
        printf("request count is %d\n", requestCount);
        int initialCounts[N];
        printf("Intial Counts:");
        for(int i=0;i<N;i++){
            printf("%d ",elevators[i].count);
            initialCounts[i]=elevators[i].count;
        }
        // printf("\n");
   
        
        

    TurnChangeResponse response;
    if(msgrcv(msgqid,&response,sizeof(response)-sizeof(long),2,0)==-1){
        printf("Error in msgrcv helper");
        return 1;
    }

    turn=response.turnNumber;
    int newPassengerRequestCount=response.newPassengerRequestCount;
    printf("This is turn %d\n",turn);
    if(response.finished==1){
        break;
    }

    int droppedPassengerCount=0;
    int pickedUpPassengerCount=0;
    for(int i=0;i<N;i++){
            droppedinturn[i]=0;
        }
    for(int j=0;j<N;j++){
            elevators[j].floor=mainShmPtr->elevatorFloors[j];
    }
    
    
    

    // for(int i=0;i<N;i++){
    //     printf("Elevator %d on floor %d\n",i,elevators[i].floor);
    // }

    // printf("No of requests for this turn %d\n",newPassengerRequestCount);

    if(turn<=T){
        for(int k=0;k<newPassengerRequestCount;k++){
            requests[requestCount].requestId=mainShmPtr->newPassengerRequests[k].requestId;
            requests[requestCount].startFloor=mainShmPtr->newPassengerRequests[k].startFloor;
            requests[requestCount].requestedFloor=mainShmPtr->newPassengerRequests[k].requestedFloor;
            requests[requestCount].isPickedUp=0;
            requests[requestCount].isCompleted=0;
            
            // printf("request added %d %d %d\n",requests[requestCount].requestId,requests[requestCount].startFloor,requests[requestCount].requestedFloor);
            requestCount++;

        }
    }


    if(turn>0){
        for(int i=0;i<N;i++){
            for(int j=0;j<elevators[i].count;j++){
                if(elevators[i].floor==elevators[i].passengers[j].requestedFloor){
                    droppedPassengers[droppedPassengerCount]=elevators[i].passengers[j].requestId;
                    droppedPassengerCount++;
                    // printf("dropped : by %d passenger %d on floor %d\n ",i,elevators[i].passengers[j].requestId,elevators[i].floor);
                    for(int k=0;k<requestCount;k++){
                        if(requests[k].requestId==elevators[i].passengers[j].requestId){
                            requests[k].isCompleted=1;
                            break;
                        }
                    }

                    for(int k=j;k<elevators[i].count-1;k++){
                        elevators[i].passengers[k]=elevators[i].passengers[k+1];
                    }
                    elevators[i].count--;
                    droppedinturn[i]++;
                    j--;

                
                }
            }
        }

        for(int i=0;i<requestCount;i++){
            if(!requests[i].isPickedUp && !requests[i].isCompleted){
                for(int j=0;j<N;j++){
                    if(elevators[j].floor==requests[i].startFloor && elevators[j].count<5){
                        // printf("%d elevator %d count is before\n",j,elevators[j].count);
                    
                        pickedUpPassengers[pickedUpPassengerCount][0]=requests[i].requestId;
                        pickedUpPassengers[pickedUpPassengerCount][1]=j;
                        pickedUpPassengerCount++;


                        elevators[j].passengers[elevators[j].count].requestId=requests[i].requestId;
                        elevators[j].passengers[elevators[j].count].requestedFloor=requests[i].requestedFloor;
                        elevators[j].count++;

                        requests[i].isPickedUp=1;
                        
                        // printf("picked up: passenger %d at %d by elevator %d\n",requests[i].requestId,requests[i].startFloor,j);
                        // printf("%d elevator %d count is after\n",j,elevators[j].count);
                        break;

                    }
                }
            }
        }


        for(int i=0;i<N;i++){
            char move='s';
            int target=-1;
            if (elevators[i].count>0)
            {//sort for min floor elevator
                
                target=elevators[i].passengers[0].requestedFloor;
            }
            else{
                int mindist=10000;
                for(int j=0;j<requestCount;j++){
                    if(!requests[j].isPickedUp && !requests[j].isCompleted){
                        for(int k=0;k<N;k++){
                            if(elevators[k].targetFloor<=requests[j].startFloor){
                                break;
                            }
                        }
                        int dist;
                        if(elevators[i].floor-requests[j].startFloor>0){
                            dist=elevators[i].floor-requests[j].startFloor;
                        }
                        else{
                            dist=requests[j].startFloor-elevators[i].floor;
                        }

                        if(dist<mindist){
                            mindist=dist;
                            target=requests[j].startFloor;
                        }
                    }
                }
            }

            if(target!=-1 && target!=elevators[i].floor){
                if(target>elevators[i].floor){
                    move='u';       
                }
                else if(target<elevators[i].floor){
                    move='d';
                }
                else{
                    move='s';
                }
                
            }

                

            

            elevatorMovementInstructions[i]=move;


        }
        
         


    }
    
    
        
    
    for(int i=0;i<N;i++){
        mainShmPtr->elevatorMovementInstructions[i]=elevatorMovementInstructions[i];
    }
    // for(int i=0;i<pickedUpPassengerCount;i++){
    //     printf("This is pickedUpPassengers");
    //     printf("%d",pickedUpPassengers[i][0]);
    //     printf("%d",pickedUpPassengers[i][1]);
    //     printf("end of it");
    // }
    
    
    
    for(int i=0;i<pickedUpPassengerCount;i++){
        mainShmPtr->pickedUpPassengers[i][0]=pickedUpPassengers[i][0];
        mainShmPtr->pickedUpPassengers[i][1]=pickedUpPassengers[i][1];
    }
     for(int i=0;i<droppedPassengerCount;i++){
        mainShmPtr->droppedPassengers[i]=droppedPassengers[i];
    }
    
    
    for(int i=0;i<N;i++){
        if(initialCounts[i]>0 && elevatorMovementInstructions[i]!='s'){
                    printf("Auth String required\n");
                    int totalstrings=1;

                    for(int k=0;k<initialCounts[i];k++){
                    totalstrings=totalstrings*6;
                    }
                    
                    // printf("Elevator Count %d\n",elevators[i].count);

                    char** stringset;
                    stringset=getAllPossibleStrings(initialCounts[i]);

                    stringGuesser(mainShmPtr,stringset,totalstrings,solqid,M,i);


                    
                      
                }
                
            }
        // for(int i=0;i<pickedUpPassengerCount;i++){
        // printf("THIS I AM PRINITING FROM SHARED MEMORY (picked up)\n");
        // printf("%d\n",mainShmPtr->pickedUpPassengers[i][0]);
        // printf("%d\n",mainShmPtr->pickedUpPassengers[i][1]);
        //  }
        //  for(int i=0;i<droppedPassengerCount;i++){
        //  printf("THIS I AM PRINITING FROM SHARED MEMORY (Dropped)\n");

        // printf("%d\n",mainShmPtr->droppedPassengers[i]);
        //  }
       
    

        TurnChangeRequest changerequest;

        changerequest.mtype=1;
        changerequest.droppedPassengersCount=droppedPassengerCount;
        changerequest.pickedUpPassengersCount=pickedUpPassengerCount;


        if(msgsnd(msgqid,&changerequest,sizeof(changerequest)-sizeof(long),0)==-1){
            printf("Error in msgsnd");
            break;
        }
    }


    
    return 0;
}
