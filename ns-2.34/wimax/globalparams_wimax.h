/** RPI
    This is a singleton class , which has been created to access global functions and variables for wimax.  
    There is a global beta table and BLER table. 


    RPI**/  

#include <stdio.h>
#include <fstream>
#include <iostream>
 
class GlobalParams_Wimax
   
{  
   
 private:
	   
  GlobalParams_Wimax(const GlobalParams_Wimax&);  //Prevents making a copy         
   
 protected:
   
  GlobalParams_Wimax();       //Only a SysParms member can call this , Prevents a user from creating singleton objects

  virtual ~GlobalParams_Wimax();  //Prevents just anyone from deleting the singleton
  
 public:
  
  static GlobalParams_Wimax* Instance();        //The "official" access point.

  //SINR table reading variables
  double * Table_SINR;
	
  double * Table_BLER;

  double * Table_Low_SINR_Bounds;

  double * Table_High_SINR_Bounds;
  
  int * Table_Index;         
  
  int NumLines;
			
  int Num_SINR_Decimals;

  // beta tabel reading variables
  double Beta[3][33];
      
  // SINR table reading functions 
  int CountLines( std::ifstream& In ); 

  int ReadTableFromFile (); 

  double GetTablePrecision(double FullDoubleNumber);

  double TableLookup(int Index, double SINR);

  // beta table reading functions
  int LoadBetaFile(); 
		
};
