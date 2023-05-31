

#include <Arduino.h>
#include "JumperlessDefinesRP2040.h"
#include "MatrixStateRP2040.h"
#include "NetManager.h"
#include "NetsToChipConnections.h"

//don't try to understand this, it's still a mess

int startEndChip[2] = {-1, -1}; 
int bothNodes[2] = {-1, -1};
int endChip;
int chipCandidates[2][4] = {{-1, -1, -1, -1}, {-1, -1, -1, -1}}; // nano and sf nodes have multiple possible chips they could be connected to, so we need to store them all and check them all

int chipsLeastToMostCrowded[12] = {0,1,2,3,4,5,6,7,8,9,10,11}; //this will be sorted from most to least crowded, and will be used to determine which chip to use for each node
int sfChipsLeastToMostCrowded[4] = {8,9,10,11}; //this will be sorted from most to least crowded, and will be used to determine which chip to use for each node
int bbToSfLanes[8][4] = {{0}}; // [Chip A - H][CHIP I-K] 0 = free  1 = used

int numberOfUniqueNets = 0;
int numberOfNets = 0;
int numberOfPaths = 0;
unsigned long timeToSort = 0;

bool debugNTCC = false;

void sortPathsByNet(void) // not actually sorting, just copying the bridges and nets back from netStruct so they're both in the same order
{
    timeToSort = micros();
    printBridgeArray();

    for (int i = 0; i < MAX_BRIDGES; i++)
    {
        if (net[i].number == 0)
        {
            numberOfNets = i;
            break;
        }
    }

    for (int i = 0; i < MAX_BRIDGES; i++)
    {
        if (path[i].node1 == 0 && path[i].node2 == 0)
        {
            numberOfPaths = i;
            break;
        }
    }
    // printPathArray();

    Serial.print("number of nets: ");
    Serial.println(numberOfNets);
    int pathIndex = 0;

    for (int j = 1; j <= numberOfPaths; j++)
    {

        for (int k = 0; k < MAX_BRIDGES; k++)
        {
            if (net[j].bridges[k][0] == 0)
            {

                break;
            }
            else
            {
                path[pathIndex].net = net[j].number;
                path[pathIndex].node1 = net[j].bridges[k][0];
                path[pathIndex].node2 = net[j].bridges[k][1];

                if (path[pathIndex].net == path[pathIndex - 1].net)
                {
                }
                else
                {
                    numberOfUniqueNets++;
                }

                // Serial.print("path[");
                // Serial.print(pathIndex);
                // Serial.print("] net: ");
                // Serial.println(path[pathIndex].net);
                pathIndex++;
            }
        }
    }

    newBridgeLength = pathIndex;
    numberOfPaths = pathIndex;

    Serial.print("number unique of nets: ");
    Serial.println(numberOfUniqueNets);
    Serial.print("pathIndex: ");
    Serial.println(pathIndex);
    Serial.print("numberOfPaths: ");
    Serial.println(numberOfPaths);

    clearChipsOnPathToNegOne(); // clear chips and all trailing paths to -1{if there are bridges that weren't made due to DNI rules, there will be fewer paths now because they were skipped}
    printBridgeArray();
    Serial.println("\n\r");
    timeToSort = micros() - timeToSort;
    Serial.print("time to sort: ");
    Serial.print(timeToSort);
    Serial.println("us\n\r");
}

void bridgesToPaths(void)
{
    sortPathsByNet();
    for (int i = 0; i < numberOfPaths; i++)
    {

        Serial.print("path[");
        Serial.print(i);
        Serial.print("]\n\rnodes [");
        Serial.print(path[i].node1);
        Serial.print("-");
        Serial.print(path[i].node2);
        Serial.println("]\n\r");

        findStartAndEndChips(path[i].node1, path[i].node2, i);

        Serial.print("\n\rstart chip: ");
        Serial.println(chipNumToChar(startEndChip[0]));
        Serial.print("end chip:   ");
        Serial.println(chipNumToChar(startEndChip[1]));
        Serial.println("\n\n\n\r");
    }

    printPathArray();
    sortAllChipsLeastToMostCrowded();
     resolveChipCandidates();
}

void findStartAndEndChips(int node1, int node2, int pathIdx)
{
    bothNodes[0] = node1;
    bothNodes[1] = node2;
    startEndChip[0] = -1;
    startEndChip[1] = -1;

    Serial.print("finding chips for nodes: ");
    Serial.print(definesToChar(node1));
    Serial.print("-");
    Serial.println(definesToChar(node2));

    for (int twice = 0; twice < 2; twice++) // first run gets node1 and start chip, second is node2 and end
    {
        Serial.print("\n\rnode: ");
        Serial.println(twice + 1);
        Serial.println(" ");
        int candidatesFound = 0;

        switch (bothNodes[twice]) // not checking availability, just finding the chip
        {

        case 1:
        case 30:
        case 32:
        case 61:
        {
            startEndChip[twice] = CHIP_L;
            Serial.print("sf chip: ");
            Serial.println(chipNumToChar(startEndChip[twice]));
            path[pathIdx].chip[twice] = startEndChip[twice];

            break;
        }

        case 2 ... 29: // on the breadboard
        case 33 ... 60:
        {
            for (int i = 0; i < 8; i++)
            {
                for (int j = 1; j < 8; j++) // start at 1 so we dont confuse CHIP_L for a node
                {
                    if (ch[i].yMap[j] == bothNodes[twice])
                    {
                        startEndChip[twice] = i;
                        Serial.print("bb chip: ");
                        Serial.println(chipNumToChar(startEndChip[twice]));
                        path[pathIdx].chip[twice] = startEndChip[twice];
                        break;
                    }
                }
            }
            break;
        }
        case NANO_D0 ... NANO_A7: // on the nano
        {
            int nanoIndex = defToNano(bothNodes[twice]);

            if (nano.numConns[nanoIndex] == 1)
            {
                startEndChip[twice] = nano.mapIJ[nanoIndex];
                Serial.print("nano chip: ");
                Serial.println(chipNumToChar(startEndChip[twice]));
                path[pathIdx].chip[twice] = startEndChip[twice];
            }
            else
            {
                chipCandidates[twice][0] = nano.mapIJ[nanoIndex];
                path[pathIdx].candidates[twice][0] = chipCandidates[twice][0];
                candidatesFound++;
                chipCandidates[twice][1] = nano.mapKL[nanoIndex];
                path[pathIdx].candidates[twice][1] = chipCandidates[twice][1];
                candidatesFound++;
                Serial.print("nano candidate chips: ");
                Serial.print(chipNumToChar(chipCandidates[twice][0]));
                Serial.print(" ");
                Serial.println(chipNumToChar(chipCandidates[twice][1]));
            }
            break;
        }
        case GND ... CURRENT_SENSE_MINUS:
        {

            Serial.print("special function candidate chips: ");
            for (int i = 8; i < 12; i++)
            {
                for (int j = 0; j < 16; j++)
                {
                    if (ch[i].xMap[j] == bothNodes[twice])
                    {
                        chipCandidates[twice][candidatesFound] = i;
                        path[pathIdx].candidates[twice][candidatesFound] = chipCandidates[twice][candidatesFound];
                        candidatesFound++;
                        Serial.print(chipNumToChar(i));
                        Serial.print(" ");
                    }
                }
            }
            Serial.println(" ");
            break;
        }
        }
        //Serial.print("\n\r");
    }

    if (startEndChip[0] == -1 || startEndChip[1] == -1)
    {
    }
    else
    {
       
    }

    
}

void resolveChipCandidates(void)
{
    int nodesToResolve[2] = {0,0}; // {node1,node2} 0 = already found, 1 = needs resolving

   

    for (int pathIndex = 0; pathIndex < numberOfPaths; pathIndex++)
    {
        nodesToResolve[0] = 0;
        nodesToResolve[1] = 0;
        
        if (path[pathIndex].chip[0] == -1)
        {
            nodesToResolve[0] = 1;
        }
        else
        {
            nodesToResolve[0] = 0;
        }

        if (path[pathIndex].chip[1] == -1)
        {
            nodesToResolve[1] = 1;
        }
        else
        {
            nodesToResolve[1] = 0;
        }
    



        for (int nodeOneOrTwo = 0; nodeOneOrTwo < 2; nodeOneOrTwo++)
        {
            if(nodesToResolve[nodeOneOrTwo] == 1)
            {
   

            path[pathIndex].chip[nodeOneOrTwo] = moreAvailableChip(path[pathIndex].candidates[nodeOneOrTwo][0], path[pathIndex].candidates[nodeOneOrTwo][1]);

            Serial.print("path[");
            Serial.print(pathIndex);
            Serial.print("] chip from ");
            Serial.print(chipNumToChar(path[pathIndex].chip[(1+nodeOneOrTwo)%2]));
            Serial.print(" to chip ");
            Serial.print(chipNumToChar(path[pathIndex].chip[nodeOneOrTwo]));
            Serial.print(" chosen\n\n\r");
            }



        }




    }


}

void bbToSfConnections (void)
{

    for (int i = 0 ; i < numberOfPaths; i++)
    {
        if (path[i].chip[0] > 7 && path[i].chip[1] <= 7 && path[i].chip[1] >= 0)
        {


                bbToSfLanes[path[i].chip[1]][path[i].chip[0] - 8] ++; //why is this happening every loop
                Serial.print (i);
                Serial.print (" ");
                Serial.print (chipNumToChar((path[i].chip[1])));
                Serial.print ("-");
                Serial.println (chipNumToChar((path[i].chip[0])));
                

        } else if (path[i].chip[1] > 7 && path[i].chip[0] <= 7 && path[i].chip[0] >= 0)

                bbToSfLanes[path[i].chip[0]][path[i].chip[1] - 8] ++;
                Serial.print (i);
                Serial.print (" ");
                Serial.print (chipNumToChar((path[i].chip[0])));
                Serial.print ("-");
                Serial.println (chipNumToChar((path[i].chip[1])));




        }

    

for (int i = 0; i < 8; i++)
{
    Serial.print(chipNumToChar(i));
    Serial.print(": ");
    for (int j = 0; j < 4; j++)
    {
        Serial.print(chipNumToChar(j + 8));
        Serial.print(bbToSfLanes[i][j]);
        Serial.print("  ");
    }
    Serial.println("\n\r");
}


}

int moreAvailableChip (int chip1 , int chip2)
{
    int chipChosen = -1;
    sortAllChipsLeastToMostCrowded();

    for (int i = 0; i < 12; i++)
    {
        if (chipsLeastToMostCrowded[i] == chip1 || chipsLeastToMostCrowded[i] == chip2)
        {
            chipChosen = chipsLeastToMostCrowded[i];
            break;
        }
    }
return chipChosen;

}


void sortSFchipsLeastToMostCrowded(void)
{
    int numberOfConnectionsPerSFchip[4] = {0,0,0,0}; 


    for (int i = 0; i < numberOfPaths; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (path[i].chip[j] > 7)
            {
                numberOfConnectionsPerSFchip[path[i].chip[j] - 8]++;
            }
        }
        



    }
for (int i = 0; i < 4; i++)
{
Serial.print("sf connections: ");
Serial.print (chipNumToChar(i + 8));
Serial.print(numberOfConnectionsPerSFchip[i]);
Serial.print("\n\r");


}



}

void sortAllChipsLeastToMostCrowded(void)
{
    debugNTCC = false;
    int numberOfConnectionsPerChip[12] = {0,0,0,0,0,0,0,0,0,0,0,0}; //this will be used to determine which chip is most crowded
 

        for (int i = 0; i < 12; i++)
    {
        chipsLeastToMostCrowded[i] = i;

    }

    Serial.println("\n\r");
    
    for (int i = 0; i < numberOfPaths; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (path[i].chip[j] != -1)
            {
                numberOfConnectionsPerChip[path[i].chip[j]]++;
            }
        }
        
    }
if (debugNTCC)
{
    for (int i = 0; i < 12; i++)
    {
        Serial.print(chipNumToChar(i));
        Serial.print(": ");
        Serial.println(numberOfConnectionsPerChip[i]);
    }

    Serial.println("\n\r");
}
    int temp = 0;

    for (int i = 0; i < 12; i++)
    {
        for (int j = 0; j < 11; j++)
        {
            if (numberOfConnectionsPerChip[j] > numberOfConnectionsPerChip[j + 1])
            {
                temp = numberOfConnectionsPerChip[j];
                //chipsLeastToMostCrowded[j] = chipsLeastToMostCrowded[j + 1];
                numberOfConnectionsPerChip[j] = numberOfConnectionsPerChip[j + 1];
                numberOfConnectionsPerChip[j + 1] = temp;

                temp = chipsLeastToMostCrowded[j];
                chipsLeastToMostCrowded[j] = chipsLeastToMostCrowded[j + 1];
                chipsLeastToMostCrowded[j + 1] = temp;
            }
        }
    }

    for (int i = 0; i < 12; i++)
    {

        if (chipsLeastToMostCrowded[i] > 7)
        {
            sfChipsLeastToMostCrowded[i - 8] = chipsLeastToMostCrowded[i];
        }
        if (debugNTCC)
{
        Serial.print(chipNumToChar(chipsLeastToMostCrowded[i]));
        Serial.print(": ");
        Serial.println(numberOfConnectionsPerChip[i]);
}
    }
if (debugNTCC)
{
    for (int i = 0; i < 4; i++)
    {
        Serial.print("\n\n\r");
        Serial.print(chipNumToChar(sfChipsLeastToMostCrowded[i]));
        Serial.print(": ");
    
        Serial.print("\n\n\r");
    }
}
debugNTCC = true;
bbToSfConnections();

}


void printPathArray(void) // this also prints candidates and x y
{
    // Serial.print("\n\n\r");
    // Serial.print("newBridgeIndex = ");
    // Serial.println(newBridgeIndex);
    Serial.print("\n\r");
    int tabs = 0;
    int lineCount = 0;
    for (int i = 0; i < newBridgeLength; i++)
    {
        tabs += Serial.print(i);
        Serial.print("  ");
        if (i < 10)
        {
            tabs += Serial.print(" ");
        }
        if (i < 100)
        {
            tabs += Serial.print(" ");
        }
        tabs += Serial.print("[");
        tabs += printNodeOrName(path[i].node1);
        tabs += Serial.print("-");
        tabs += printNodeOrName(path[i].node2);
        tabs += Serial.print("]\tNet ");
        tabs += printNodeOrName(path[i].net);
        tabs += Serial.println(" ");
        tabs += Serial.print("\n\rnode1 chip:  ");
        tabs += printChipNumToChar(path[i].chip[0]);
        tabs += Serial.print("\n\rnode2 chip:  ");
        tabs += printChipNumToChar(path[i].chip[1]);
        tabs += Serial.print("\n\n\rnode1 candidates: ");
        for (int j = 0; j < 3; j++)
        {
            printChipNumToChar(path[i].candidates[0][j]);
            tabs += Serial.print(" ");
        }
        tabs += Serial.print("\n\rnode2 candidates: ");
        for (int j = 0; j < 3; j++)
        {
            printChipNumToChar(path[i].candidates[1][j]);
            tabs += Serial.print(" ");
        }

        tabs += Serial.println("\n\n\r");

        // Serial.print(tabs);
        // for (int i = 0; i < 24 - (tabs); i++)
        //{
        //     Serial.print(" ");
        // }
        tabs = 0;
    }
}

int defToNano(int nanoIndex)
{
    return nanoIndex - NANO_D0;
}

char chipNumToChar(int chipNumber)
{
    return chipNumber + 'A';
}

int printChipNumToChar(int chipNumber)
{
    if (chipNumber == -1)
    {
        return Serial.print(" ");
    }
    else
    {

        return Serial.print((char)(chipNumber + 'A'));
    }
}

void clearChipsOnPathToNegOne(void)
{
    for (int i = 0; i < MAX_BRIDGES; i++)
    {
        if (i > numberOfPaths)
        {
            path[i].node1 = -1; // i know i can just do {-1,-1,-1} but
            path[i].node2 = -1;
            path[i].net = -1;
        }

        for (int c = 0; c < 3; c++) 
        {
            path[i].chip[c] = -1;
            path[i].x[c] = -1;
            path[i].y[c] = -1;

            path[i].candidates[c][0] = -1;
            path[i].candidates[c][1] = -1;
            path[i].candidates[c][2] = -1; //CEEEEEEEE!!!!!! i had this set to 3 and it was clearing everything, but no im not using rust
        }
    }
}
/*
So the nets are all made, now we need to figure out which chip connections need to be made to make that phycically happen

start with the special function nets, they're the highest priority

maybe its simpler to make an array of every possible connection


start at net 1 and go up

find start and end chip

bb chips
sf chips
nano chips


things that store x and y valuse for paths
chipStatus.xStatus[]
chipStatus.yStatus[]
nanoStatus.xStatusIJKL[]


struct nanoStatus {  //there's only one of these so ill declare and initalize together unlike above

//all these arrays should line up (both by index and visually) so one index will give you all this data

//                         |        |        |        |        |        |        |        |        |        |        |         |         |          |         |           |          |        |        |        |        |        |        |        |        |
const char *pinNames[24]=  {   " D0",   " D1",   " D2",   " D3",   " D4",   " D5",   " D6",   " D7",   " D8",   " D9",    "D10",    "D11",     "D12",    "D13",      "RST",     "REF",   " A0",   " A1",   " A2",   " A3",   " A4",   " A5",   " A6",   " A7"};// String with readable name //padded to 3 chars (space comes before chars)
//                         |        |        |        |        |        |        |        |        |        |        |         |         |          |         |           |          |        |        |        |        |        |        |        |        |
const int8_t pinMap[24] =  { NANO_D0, NANO_D1, NANO_D2, NANO_D3, NANO_D4, NANO_D5, NANO_D6, NANO_D7, NANO_D8, NANO_D9, NANO_D10, NANO_D11,  NANO_D12, NANO_D13, NANO_RESET, NANO_AREF, NANO_A0, NANO_A1, NANO_A2, NANO_A3, NANO_A4, NANO_A5, NANO_A6, NANO_A7};//Array index to internal arbitrary #defined number
//                         |        |        |        |        |        |        |        |        |        |        |         |         |          |         |           |          |        |        |        |        |        |        |        |        |
const int8_t numConns[24]= {  1     , 1      , 2      , 2      , 2      , 2      , 2      , 2      , 2      , 2      , 2       , 2       ,  2       , 2       , 1         , 1        , 2      , 2      , 2      , 2      , 2      , 2      , 1      , 1      };//Whether this pin has 1 or 2 connections to special function chips    (OR maybe have it be a map like i = 2  j = 3  k = 4  l = 5 if there's 2 it's the product of them ij = 6  ik = 8  il = 10 jk = 12 jl = 15 kl = 20 we're trading minuscule amounts of CPU for RAM)
//                         |        |        |        |        |        |        |        |        |        |        |         |         |          |         |           |          |        |        |        |        |        |        |        |        |
const int8_t  mapIJ[24] =  { CHIP_J , CHIP_I , CHIP_J , CHIP_I , CHIP_J , CHIP_I , CHIP_J , CHIP_I , CHIP_J , CHIP_I , CHIP_J  , CHIP_I  ,  CHIP_J  , CHIP_I  , CHIP_I    ,  CHIP_J  , CHIP_I , CHIP_J , CHIP_I , CHIP_J , CHIP_I , CHIP_J , CHIP_I , CHIP_J };//Since there's no overlapping connections between Chip I and J, this holds which of those 2 chips has a connection at that index, if numConns is 1, you only need to check this one
const int8_t  mapKL[24] =  { -1     , -1     , CHIP_K , CHIP_K , CHIP_K , CHIP_K , CHIP_K , CHIP_K , CHIP_K , CHIP_K , CHIP_K  , CHIP_K  ,  CHIP_K  , -1      , -1        , -1       , CHIP_K , CHIP_K , CHIP_K , CHIP_K , CHIP_L , CHIP_L , -1     , -1     };//Since there's no overlapping connections between Chip K and L, this holds which of those 2 chips has a connection at that index, -1 for no connection
//                         |        |        |        |        |        |        |        |        |        |        |         |         |          |         |           |          |        |        |        |        |        |        |        |        |
const int8_t xMapI[24]  =  { -1     , 1      , -1     , 3      , -1     , 5      , -1     , 7      , -1     , 9      , -1      , 8       ,  -1      , 10      , 11        , -1       , 0      , -1     , 2      , -1     , 4      , -1     , 6      , -1     };//holds which X pin is connected to the index on Chip I, -1 for none
   int8_t xStatusI[24]  =  { -1     , 0      , -1     , 0      , -1     , 0      , -1     , 0      , -1     , 0      , -1      , 0       ,  -1      , 0       , 0         , -1       , 0      , -1     , 0      , -1     , 0      , -1     , 0      , -1     };//-1 for not connected to that chip, 0 for available, >0 means it's connected and the netNumber is stored here
//                         |        |        |        |        |        |        |        |        |        |        |         |         |          |         |           |          |        |        |        |        |        |        |        |        |
const int8_t xMapJ[24]  =  { 0      , -1     , 2      , -1     , 4      , -1     , 6      , -1     , 8      , -1     , 9       , -1      ,  10      , -1      , -1        , 11       , -1     , 1      , -1     , 3      , -1     , 5      , -1     , 7      };//holds which X pin is connected to the index on Chip J, -1 for none
   int8_t xStatusJ[24]  =  { 0      , -1     , 0      , -1     , 0      , -1     , 0      , -1     , 0      , -1     , 0       , -1      , 0        , 0       , -1        , 0        , -1     , 0      , -1     , 0      , -1     , 0      , -1     , 0      };//-1 for not connected to that chip, 0 for available, >0 means it's connected and the netNumber is stored here
//                         |        |        |        |        |        |        |        |        |        |        |         |         |          |         |           |          |        |        |        |        |        |        |        |        |
const int8_t xMapK[24]  =  { -1     , -1     , 4      , 5      , 6      , 7      , 8      , 9      , 10     , 11     , 12      , 13      ,  14      , -1      , -1        , -1       , 0      , 1      , 2      , 3      , -1     , -1     , -1     , -1     };//holds which X pin is connected to the index on Chip K, -1 for none
   int8_t xStatusK[24]  =  { -1     , -1     , 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0       , 0       ,  0       , -1      , -1        , -1       , 0      , 0      , 0      , 0      , -1     , -1     , -1     , -1     };//-1 for not connected to that chip, 0 for available, >0 means it's connected and the netNumber is stored here
//                         |        |        |        |        |        |        |        |        |        |        |         |         |          |         |           |          |        |        |        |        |        |        |        |        |
const int8_t xMapL[24]  =  { -1     , -1     , -1     , -1     , -1     , -1     , -1     , -1     , -1     , -1     , -1      , -1      ,  -1      , -1      , -1        , -1       , -1     , -1     , -1     , -1     , 12     , 13     , -1     , -1     };//holds which X pin is connected to the index on Chip L, -1 for none
   int8_t xStatusL[24]  =  { -1     , -1     , -1     , -1     , -1     , -1     , -1     , -1     , -1     , -1     , -1      , -1      ,  -1      , -1      , -1        , -1       , -1     , -1     , -1     , -1     , 0      , 0      , -1     , -1     };//-1 for not connected to that chip, 0 for available, >0 means it's connected and the netNumber is stored here

// mapIJKL[]     will tell you whethher there's a connection from that nano pin to the corresponding special function chip
// xMapIJKL[]    will tell you the X pin that it's connected to on that sf chip
// xStatusIJKL[] says whether that x pin is being used (this should be the same as mt[8-10].xMap[] if theyre all stacked on top of each other)
//              I haven't decided whether to make this just a flag, or store that signal's destination
const int8_t reversePinMap[110] = {NANO_D0, NANO_D1, NANO_D2, NANO_D3, NANO_D4, NANO_D5, NANO_D6, NANO_D7, NANO_D8, NANO_D9, NANO_D10, NANO_D11, NANO_D12, NANO_D13, NANO_RESET, NANO_AREF, NANO_A0, NANO_A1, NANO_A2, NANO_A3, NANO_A4, NANO_A5, NANO_A6, NANO_A7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,GND,101,102,SUPPLY_3V3,104,SUPPLY_5V,DAC0_5V,DAC1_8V,CURRENT_SENSE_PLUS,CURRENT_SENSE_MINUS};

};

struct netStruct net[MAX_NETS] = { //these are the special function nets that will always be made
//netNumber,       ,netName          ,memberNodes[]         ,memberBridges[][2]     ,specialFunction        ,intsctNet[] ,doNotIntersectNodes[]                 ,priority
    {     127      ,"Empty Net"      ,{EMPTY_NET}           ,{{}}                   ,EMPTY_NET              ,{}          ,{EMPTY_NET,EMPTY_NET,EMPTY_NET,EMPTY_NET,EMPTY_NET,EMPTY_NET,EMPTY_NET} , 0},
    {     1        ,"GND\t"          ,{GND}                 ,{{}}                   ,GND                    ,{}          ,{SUPPLY_3V3,SUPPLY_5V,DAC0_5V,DAC1_8V}    , 1},
    {     2        ,"+5V\t"          ,{SUPPLY_5V}           ,{{}}                   ,SUPPLY_5V              ,{}          ,{GND,SUPPLY_3V3,DAC0_5V,DAC1_8V}          , 1},
    {     3        ,"+3.3V\t"        ,{SUPPLY_3V3}          ,{{}}                   ,SUPPLY_3V3             ,{}          ,{GND,SUPPLY_5V,DAC0_5V,DAC1_8V}           , 1},
    {     4        ,"DAC 0\t"        ,{DAC0_5V}             ,{{}}                   ,DAC0_5V                ,{}          ,{GND,SUPPLY_5V,SUPPLY_3V3,DAC1_8V}        , 1},
    {     5        ,"DAC 1\t"        ,{DAC1_8V}             ,{{}}                   ,DAC1_8V                ,{}          ,{GND,SUPPLY_5V,SUPPLY_3V3,DAC0_5V}        , 1},
    {     6        ,"I Sense +"      ,{CURRENT_SENSE_PLUS}  ,{{}}                   ,CURRENT_SENSE_PLUS     ,{}          ,{CURRENT_SENSE_MINUS}                     , 2},
    {     7        ,"I Sense -"      ,{CURRENT_SENSE_MINUS} ,{{}}                   ,CURRENT_SENSE_MINUS    ,{}          ,{CURRENT_SENSE_PLUS}                      , 2},
};



Index   Name            Number          Nodes                   Bridges                         Do Not Intersects
0       Empty Net       127             EMPTY_NET               {0-0}                           EMPTY_NET
1       GND             1               GND,1,2,D0,3,4          {1-GND,1-2,D0-1,2-3,3-4}        3V3,5V,DAC_0,DAC_1
2       +5V             2               5V,11,12,10,9           {11-5V,11-12,10-11,9-10}        GND,3V3,DAC_0,DAC_1
3       +3.3V           3               3V3,D10,D11,D12         {D10-3V3,D10-D11,D11-D12}       GND,5V,DAC_0,DAC_1
4       DAC 0           4               DAC_0                   {0-0}                           GND,5V,3V3,DAC_1
5       DAC 1           5               DAC_1                   {0-0}                           GND,5V,3V3,DAC_0
6       I Sense +       6               I_POS,6,5,A1,AREF       {6-I_POS,5-6,A1-5,AREF-A1}      I_NEG
7       I Sense -       7               I_NEG                   {0-0}                           I_POS

Index   Name            Number          Nodes                   Bridges                         Do Not Intersects
8       Net 8           8               7,8                     {7-8}                           0
9       Net 9           9               D13,D1,A7               {D13-D1,D13-A7}                 0




struct chipStatus{

int chipNumber;
char chipChar;
int8_t xStatus[16]; //store the bb row or nano conn this is eventually connected to so they can be stacked if conns are redundant
int8_t yStatus[8];  //store the row/nano it's connected to
const int8_t xMap[16];
const int8_t yMap[8];

};



struct chipStatus ch[12] = {
  {0,'A',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {CHIP_I, CHIP_J, CHIP_B, CHIP_B, CHIP_C, CHIP_C, CHIP_D, CHIP_D, CHIP_E, CHIP_K, CHIP_F, CHIP_F, CHIP_G, CHIP_G, CHIP_H, CHIP_H},//X MAP constant
  {CHIP_L,  t2,t3, t4, t5, t6, t7, t8}},  // Y MAP constant

  {1,'B',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {CHIP_A, CHIP_A, CHIP_I, CHIP_J, CHIP_C, CHIP_C, CHIP_D, CHIP_D, CHIP_E, CHIP_E, CHIP_F, CHIP_K, CHIP_G, CHIP_G, CHIP_H, CHIP_H},
  {CHIP_L,  t9,t10,t11,t12,t13,t14,t15}},

  {2,'C',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {CHIP_A, CHIP_A, CHIP_B, CHIP_B, CHIP_I, CHIP_J, CHIP_D, CHIP_D, CHIP_E, CHIP_E, CHIP_F, CHIP_F, CHIP_G, CHIP_K, CHIP_H, CHIP_H},
  {CHIP_L, t16,t17,t18,t19,t20,t21,t22}},

  {3,'D',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {CHIP_A, CHIP_A, CHIP_B, CHIP_B, CHIP_C, CHIP_C, CHIP_I, CHIP_J, CHIP_E, CHIP_E, CHIP_F, CHIP_F, CHIP_G, CHIP_G, CHIP_H, CHIP_K},
  {CHIP_L, t23,t24,t25,t26,t27,t28,t29}},

  {4,'E',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {CHIP_A, CHIP_K, CHIP_B, CHIP_B, CHIP_C, CHIP_C, CHIP_D, CHIP_D, CHIP_I, CHIP_J, CHIP_F, CHIP_F, CHIP_G, CHIP_G, CHIP_H, CHIP_H},
  {CHIP_L,   b2, b3, b4, b5, b6, b7, b8}},

  {5,'F',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {CHIP_A, CHIP_A, CHIP_B, CHIP_K, CHIP_C, CHIP_C, CHIP_D, CHIP_D, CHIP_E, CHIP_E, CHIP_I, CHIP_J, CHIP_G, CHIP_G, CHIP_H, CHIP_H},
  {CHIP_L,  b9, b10,b11,b12,b13,b14,b15}},

  {6,'G',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {CHIP_A, CHIP_A, CHIP_B, CHIP_B, CHIP_C, CHIP_K, CHIP_D, CHIP_D, CHIP_E, CHIP_E, CHIP_F, CHIP_F, CHIP_I, CHIP_J, CHIP_H, CHIP_H},
  {CHIP_L,  b16,b17,b18,b19,b20,b21,b22}},

  {7,'H',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {CHIP_A, CHIP_A, CHIP_B, CHIP_B, CHIP_C, CHIP_C, CHIP_D, CHIP_K, CHIP_E, CHIP_E, CHIP_F, CHIP_F, CHIP_G, CHIP_G, CHIP_I, CHIP_J},
  {CHIP_L,  b23,b24,b25,b26,b27,b28,b29}},

  {8,'I',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {NANO_A0, NANO_D1, NANO_A2, NANO_D3, NANO_A4, NANO_D5, NANO_A6, NANO_D7, NANO_D11, NANO_D9, NANO_D13, NANO_RESET, DAC0_5V, ADC0_5V, SUPPLY_3V3, GND},
  {CHIP_A,CHIP_B,CHIP_C,CHIP_D,CHIP_E,CHIP_F,CHIP_G,CHIP_H}},

  {9,'J',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {NANO_D0, NANO_A1, NANO_D2, NANO_A3, NANO_D4, NANO_A5, NANO_D6, NANO_A7, NANO_D8, NANO_D10, NANO_D12, NANO_AREF, DAC1_8V, ADC1_5V, SUPPLY_5V, GND},
  {CHIP_A,CHIP_B,CHIP_C,CHIP_D,CHIP_E,CHIP_F,CHIP_G,CHIP_H}},

  {10,'K',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {NANO_A0, NANO_A1, NANO_A2, NANO_A3, NANO_A4, NANO_A5, NANO_A6, NANO_A7, NANO_D2, NANO_D3, NANO_D4, NANO_D5, NANO_D6, NANO_D7, NANO_D8, NANO_D9},
  {CHIP_A,CHIP_B,CHIP_C,CHIP_D,CHIP_E,CHIP_F,CHIP_G,CHIP_H}},

  {11,'L',
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // x status
  {-1,-1,-1,-1,-1,-1,-1,-1}, //y status
  {CURRENT_SENSE_MINUS, CURRENT_SENSE_PLUS, ADC0_5V, ADC1_5V, ADC2_5V, ADC3_8V, DAC1_8V, DAC0_5V, t1, t30, b1, b30, NANO_A4, NANO_A5, SUPPLY_5V, GND},
  {CHIP_A,CHIP_B,CHIP_C,CHIP_D,CHIP_E,CHIP_F,CHIP_G,CHIP_H}}
  };

enum nanoPinsToIndex       {     NANO_PIN_D0 ,     NANO_PIN_D1 ,     NANO_PIN_D2 ,     NANO_PIN_D3 ,     NANO_PIN_D4 ,     NANO_PIN_D5 ,     NANO_PIN_D6 ,     NANO_PIN_D7 ,     NANO_PIN_D8 ,     NANO_PIN_D9 ,     NANO_PIN_D10 ,     NANO_PIN_D11 ,      NANO_PIN_D12 ,     NANO_PIN_D13 ,       NANO_PIN_RST ,      NANO_PIN_REF ,     NANO_PIN_A0 ,     NANO_PIN_A1 ,     NANO_PIN_A2 ,     NANO_PIN_A3 ,     NANO_PIN_A4 ,     NANO_PIN_A5 ,     NANO_PIN_A6 ,     NANO_PIN_A7 };

extern struct nanoStatus nano;


struct pathStruct{

  int node1; //these are the rows or nano header pins to connect
  int node2;
  int net;

  int chip[3];
  int x[3];
  int y[3];
  int candidates[3][3];

};

*/