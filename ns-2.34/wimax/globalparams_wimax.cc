/** RPI
    This is a singleton class , which has been created to access global functions and variables for wimax.  
    There is a global beta table and BLER table. 

    RPI**/ 

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <math.h>
#include "globalparams_wimax.h"

using namespace std;


// Added by Barun : 22-Sep-2011
const string WIMAX_REF_PATH("/var/vlabs/ant/_wimax_ref_/");


namespace
{
  GlobalParams_Wimax* instance = 0;        //Address of the singleton
}


GlobalParams_Wimax::GlobalParams_Wimax()      
{
  Num_SINR_Decimals = 1;
  int FileOK;
  FileOK=ReadTableFromFile();
  LoadBetaFile();
				
}	

GlobalParams_Wimax::~GlobalParams_Wimax()       
{

  delete instance;
  instance = 0;	

  //release memory
  free(Table_Index);
  free(Table_SINR);
  free(Table_BLER);

}

//The "official" access point

GlobalParams_Wimax* GlobalParams_Wimax::Instance()
{
  //"Lazy" initialization. Singleton not created until it's needed

  if (!instance)
    {
      instance = new GlobalParams_Wimax;
    }

  return instance;
}


int GlobalParams_Wimax::CountLines( std::ifstream& In )
{
  return static_cast<int>( count( std::istreambuf_iterator<char>( In),
				       std::istreambuf_iterator<char>(), '\n' ) );
}

double GlobalParams_Wimax::GetTablePrecision(double FullDoubleNumber)
{
  int TruncatedInt;
	
  double TruncatedDouble;

  FullDoubleNumber = FullDoubleNumber * pow(10, Num_SINR_Decimals);
  TruncatedInt= (int) FullDoubleNumber;
  TruncatedDouble = (double) TruncatedInt;
	
  TruncatedDouble = TruncatedDouble / (pow(10, Num_SINR_Decimals));

  return TruncatedDouble;
	
}

double GlobalParams_Wimax::TableLookup(int Index, double SINR)
{
  //in the table provided, SINR is given in increments of 0.1 (ie. 1.1, 1.2, 1.3, etc.)
  //we lookup(Index, truncate(SINR)) and the value above it in the table (Index, truncate(SINR + 0.1))
  //we linearly interpolate the BLER to the actual SINR provided

  double BLER_low=double(NULL);
  double BLER_high=double(NULL); //when BLER_low is found, BLER_high is taken to be the next BLER in the table
  double SINR_low=GetTablePrecision(SINR);
  double SINR_high=SINR_low + pow(10, -Num_SINR_Decimals); //SINR_high is one "table precision unit" above SINR_low

  cout << "SINR_low = " << SINR_low << endl;
  cout << "SINR_high = " << SINR_high << endl;

  cout << "SINR: " << SINR << endl;
  cout << "Low: " << Table_Low_SINR_Bounds[Index] << endl;
  cout << "High: " << Table_High_SINR_Bounds[Index] << endl;

  if (SINR < Table_Low_SINR_Bounds[Index])
    return 1; // we are outside the range (lower limit) of the table, so the packet is definilty in error

  if (SINR > Table_High_SINR_Bounds[Index])
    return 0; //we are outside the rang of the table (upper limit), so the packet is definitly received correctly

  //Check if we have an exact match.  If so, simply return the value from the SINR lookup.
  for (int i=0; i < NumLines - 1 ; i++)
    {
      if ( (Table_Index[i] == Index) && (Table_SINR[i] == SINR) )
	{
	  cout << "Exact match (in TableLookup() ): " << Table_BLER[i] << endl;
	  return Table_BLER[i];
	  break;
	}
    }

	
  //if we get to here, there has not been an exact match
  //cout << endl << "No exact match... getting interpolated match..." << endl;

  //get bounding BLERs
  for (int i=0; i < NumLines - 1 ; i++)
    {
      //cout << Table_Index[i] << " " << Index << endl;
      //cout << Table_SINR[i] << " " << SINR_low << endl;
      if ( (Table_Index[i] == Index) && ( Table_SINR[i] == SINR_low) )
	{
	  //cout << "BLER_low found to be " << Table_BLER[i] << endl;
	  BLER_low = Table_BLER[i];
	  BLER_high = Table_BLER[i+1];
	  cout << "BLER_low found to be " << Table_BLER[i] <<" high = "<<Table_BLER[i+1]<< endl;
	}
    }


  double Percent_between_SINR;
  //check if there is a partial match
  if (BLER_low != double(NULL))
    {
      //interpolate BLER values
      Percent_between_SINR = (SINR - SINR_low) / (SINR_high - SINR_low); //percent between SINR

      double TableLookupBLER = (BLER_low + (Percent_between_SINR * (BLER_high - BLER_low) ) );
      cout << "BLER after interpolation found to be " << TableLookupBLER << endl;

      return TableLookupBLER;
    }

  //if all else fails
  cout << "No Match Found!" << endl;
  return (double) NULL;


}

int GlobalParams_Wimax::ReadTableFromFile ()
{

  cout << "Enter ReadTableFromFile()" << endl;

  int NumIndex = 32;
  //open file to get numLines
  ifstream LineFile;

  // Changed by Barun : 22-Sep-2011
    // To work with ANT VLabs    
    string file_path("BLER_LookupTable.txt");
    file_path = WIMAX_REF_PATH + file_path;    
  //LineFile.open("BLER_LookupTable.txt");
    LineFile.open( file_path.c_str() );
    // End changes

  if (LineFile.fail()) {
    cerr << "*** Error opening file BLER_LookupTable.txt\n";
    exit (0);
  } 
  // Added by Barun
  else {
    cout << "Reading file " << file_path << endl;
  }
  // End changes

  NumLines = CountLines(LineFile);
  //cout << "num lines = " << NumLines << endl;

  LineFile.close(); //close the file

  //OPEN TABLE FILE
  ifstream TableFile;
  // Changed by Barun : 22-Sep-2011
  //TableFile.open("BLER_LookupTable.txt");
  TableFile.open( file_path.c_str() );
  // End changes

    // Added by Barun
  if (TableFile.fail()) {
    cerr << "*** Error opening file BLER_LookupTable.txt\n";
    exit (0);
  }   
  else {
    cout << "Reading file " << file_path << endl;
  }
  // End changes
  
  Table_Index = (int *) malloc(NumLines * sizeof(int) );
  //check if malloc worked
  if(Table_Index == NULL)
    {
      fprintf(stderr,"*** Error: Table_Index is NULL\n");
      free(Table_Index);
      exit(1);
    }

  Table_SINR = (double *) malloc(NumLines * sizeof(double) );
  //check if malloc worked
  if(Table_SINR == NULL)
    {
      fprintf(stderr,"*** Error: Table_SINR is NULL\n");
      free(Table_SINR);
      exit(1);
    }

  Table_BLER = (double *) malloc(NumLines * sizeof(double) );
  //check if malloc worked
  if(Table_BLER == NULL)
    {
      fprintf(stderr,"*** Error: Table_BLER is NULL\n");
      free(Table_BLER);
      exit(1);
    }

  Table_Low_SINR_Bounds = (double *) malloc((NumIndex + 1) * sizeof(double) ); //so array indexes match table indexes
  //check if malloc worked
  if(Table_Low_SINR_Bounds == NULL)
    {
      fprintf(stderr,"*** Error: Table_Low_SINR_Bounds is NULL\n");
      free(Table_Low_SINR_Bounds);
      exit(1);
    }

  Table_High_SINR_Bounds = (double *) malloc((NumIndex + 1)* sizeof(double) ); //so array indexes match table indexes
  //check if malloc worked
  if(Table_High_SINR_Bounds == NULL)
    {
      fprintf(stderr,"*** Error: Table_High_SINR_Bounds is NULL\n");
      free(Table_High_SINR_Bounds);
      exit(1);
    }

  //cout << "Before input loop" << endl;
	
  int CurrentLine=0;
  while (!TableFile.eof())
    {
      TableFile >> Table_Index[CurrentLine];
      //cout << "Index: " << Table_Index[CurrentLine] << endl;
      TableFile >> Table_SINR[CurrentLine];
      //cout << "SINR: " << Table_SINR[CurrentLine] << endl;
      TableFile >> Table_BLER[CurrentLine];
      //cout << "BLER: " << Table_BLER[CurrentLine] << endl;

      CurrentLine++;
    }

  TableFile.close(); // close the file

  //cout << "after read file" << endl;

  Table_Low_SINR_Bounds[0]=double(NULL); //not used
  Table_High_SINR_Bounds[0]=double(NULL); //not used

  Table_Low_SINR_Bounds[1]=Table_SINR[0]; //first line in file
  //	cout << "LOW 1 = " << Table_Low_SINR_Bounds[1] << endl;

  Table_High_SINR_Bounds[NumIndex]=Table_SINR[NumLines - 1]; //last line in file
  //	cout << "HIGH 32 = " << Table_High_SINR_Bounds[NumIndex] << endl;

  int BreaksFound = 0;
  //cout << "Before break finding loop..." << endl;

  for (int counter=1; counter <= NumLines; counter++)
    {
      int TempIndex1, TempIndex2;
      TempIndex1=Table_Index[counter];
      TempIndex2=Table_Index[counter + 1];
      if (TempIndex1 != TempIndex2)
	{
	  Table_High_SINR_Bounds[BreaksFound + 1] = Table_SINR[counter];
	  Table_Low_SINR_Bounds[BreaksFound + 2] = Table_SINR[counter + 1];

	  BreaksFound++;
	  //			cout << "Breaks Found = " << BreaksFound << endl;

	  //			cout << "HIGH " << (BreaksFound) << " = " << Table_High_SINR_Bounds[BreaksFound] << endl;
	  //			cout << "LOW " << (BreaksFound + 1) << " = " << Table_Low_SINR_Bounds[BreaksFound + 1] << endl;
	  //			cout << endl;
	}

      if (BreaksFound == (NumIndex - 1) )
	break;
    }

  //if we get to here, all is well
  return 1;
}


int GlobalParams_Wimax::LoadBetaFile()
{

  ifstream BetaFile;
  // Changed by Barun : 22-Sep-2011
  string file_path("BetaTable.txt");
  file_path = WIMAX_REF_PATH + file_path;
  //BetaFile.open("BetaTable.txt"); 
  BetaFile.open(file_path.c_str()); 
  // End changes

  if (BetaFile.fail()) {
    cerr << "*** Error opening file BetaTable.txt\n";
    exit (0);
  }
  // Added by Barun
  else {
    cout << "Reading file " << file_path << endl;
  }
  // End changes 

  /*Model Index
    PedB=1
    VehA=2
  */
	
  int Model, Index;
  double TempBeta;

  while (!BetaFile.eof())
    {
      BetaFile >> Model;
      //cout << "Model " << Model << endl;
      BetaFile >> Index;
      //cout << "Index " << Index << endl;
      BetaFile >> TempBeta;
      //cout << "Beta " << TempBeta << endl;

      Beta[Model][Index] = TempBeta;
    }

  //cout << endl << Beta[1][16] << endl;

  return 0;
}
