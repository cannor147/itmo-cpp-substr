/**
* Programmer: Minhas Kamal (BSSE0509,IIT,DU)
* Date: 21-02-2014
**/

#include <iostream>
#include <string>
#include <fstream>

using namespace std;


//failure function
void FailureFunction(string String, int* F){
    int StringLength=String.length();
    int value=0;
    F[0]=value;

    int i;
    for(i=1; i<StringLength; i++){
        while(value>0 && String[value]!=String[i]){
            value=F[value-1];
        }

        if(String[value]==String[i]){
            value=value+1;
        }

        F[i]=value;
    }


    return;
}

//searching algorithm
int KMPAlgorithm(string wholeFileString, string subFileString){
    int position=-1;

    int wholeFileStringLength=wholeFileString.length(),
        subFileStringLength=subFileString.length();

    int F[subFileStringLength];
    FailureFunction(subFileString, F);

    /*///test
    int k=0;
    for(; k<subFileStringLength; k++){
        printf("%d ", F[k]);
    }printf("\n");
    /**/

    int i, j=0;
    for(i=0; i<wholeFileStringLength; i++){
        while(j>0 && subFileString[j]!=wholeFileString[i]){
            j=F[j-1];
        }

        if(subFileString[j]==wholeFileString[i]){
            j++;
        }

        if(j==subFileStringLength){
            position=i-subFileStringLength+1;
            break;
        }
    }


    return position;
}
