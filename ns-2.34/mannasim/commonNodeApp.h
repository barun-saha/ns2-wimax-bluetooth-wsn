///  
/// Copyright (C) 2003-2005 Federal University of Minas Gerais
/// 
/// This program is free software; you can redistribute it and/or
/// modify it under the terms of the GNU General Public License
/// as published by the Free Software Foundation; either version 2
/// of the License, or (at your option) any later version.
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
/// You should have received a copy of the GNU General Public License
/// along with this program; if not, write to the Free Software
/// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
/// MA 02110-1301, USA.
/// 
/// Represents common-nodes application which performs data dissemination
/// using the disseminateData method, processing using processSensedData 
/// method and other functions using CommonNodeApp methods. 
/// The CommonNodeApp is a specialization of SensorBaseApp class.
/// 
/// authors: Linnyer B. Ruiz
///	         Fabr�cio A. Silva
///			 Thais Regina de M. Braga 
///  		 Kalina Ramos Porto
///
/// code revisor: Carlos Eduardo R. Lopes
///
/// --
/// The Manna Reseach Group
/// e-mail: mannateam@gmail.com
///
/// This project was financially supported by The National Council 
/// for Scientific and Technological Development (CNPq) from the 
/// brazilian government under the process number 55.2111/2002-3.
///
#ifndef __common_node_app__
#define __common_node_app__

#include <common/ns-process.h>
#include <common/agent.h>
#include <common/packet.h>

#include "battery.h"
#include "processing.h"
#include "sensedData.h"
#include "processing.h"
#include "sensorNode.h"
#include "onDemandData.h"
#include "sensorBaseApp.h"
#include "dataGenerator.h"
#include "onDemandParameter.h"

/// Common sensor node application. Sensing Dynamics:
/// 1. Data is generated by the DataGenerator object.
/// 2. The Processing object receives the generated data and 
///    disseminates it acording to the dissemination type to be 
///	   done (programed, continuous, on demand or event driven). 
/// extends SensorBaseApp
class CommonNodeApp : public SensorBaseApp {

	public:
	
		/// Constructor
		CommonNodeApp();

		/// NS-2 Function
		int command(int argc, const char*const* argv);
		
	protected:
		
		/// Process the sensed data. Specialization of 
		/// Process::process_data() function. See NS-2 documentation
		/// for details.
		virtual void process_data(int size, AppData* data_);
		
		/// Disseminates data to the network.
		virtual void disseminateData();

		/// Disseminates the parameter data to the network.
		virtual void disseminateData(SensedData* data_);

		/// Receives sensed data, process it and disseminate (only in 
		/// contiuous sensing).
		virtual void recvSensedData(AppData* data_);
                
        /// Receives sensed data, event that generated the data, process
        /// it, and if the event and data are valid, disseminate data.
        virtual void recvSensedData(AppData* data_, AppData* eventData_);
	
		/// Informs if the node ran off energy or not.
		inline bool isDead();
};

#endif
