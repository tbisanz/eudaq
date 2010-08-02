// -*- mode: c++; mode: auto-fill; mode: flyspell-prog; -*-
// Author Philipp Roloff, DESY <mailto:philipp.roloff@desy.de>
// Version: $Id: EUTelMille.cc,v 1.48 2009-08-01 10:49:46 bulgheroni Exp $
/*
 *   This source code is part of the Eutelescope package of Marlin.
 *   You are free to use this source files for your own development as
 *   long as it stays in a public research context. You are not
 *   allowed to use it for commercial purpose. You must put this
 *   header with author names in all development based on this file.
 *
 */
// built only if GEAR and MARLINUTIL are used
#if defined(USE_GEAR) && defined(USE_MARLINUTIL)

// eutelescope includes ".h"
#include "EUTelMille.h"
#include "EUTelRunHeaderImpl.h"
#include "EUTelEventImpl.h"
#include "EUTELESCOPE.h"
#include "EUTelVirtualCluster.h"
#include "EUTelFFClusterImpl.h"
#include "EUTelDFFClusterImpl.h"
#include "EUTelBrickedClusterImpl.h"
#include "EUTelSparseClusterImpl.h"
#include "EUTelSparseCluster2Impl.h"
#include "EUTelExceptions.h"
#include "EUTelPStream.h"
#include "EUTelAlignmentConstant.h"

// marlin includes ".h"
#include "marlin/Processor.h"
#include "marlin/Global.h"
#include "marlin/Exceptions.h"
#include "marlin/AIDAProcessor.h"

// marlin util includes
#include "mille/Mille.h"

// gear includes <.h>
#include <gear/GearMgr.h>
#include <gear/SiPlanesParameters.h>

// aida includes <.h>
#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)
#include <marlin/AIDAProcessor.h>
#include <AIDA/IHistogramFactory.h>
#include <AIDA/IHistogram1D.h>
#include <AIDA/ITree.h>
#endif

// lcio includes <.h>
#include <IO/LCWriter.h>
#include <UTIL/LCTime.h>
#include <EVENT/LCCollection.h>
#include <EVENT/LCEvent.h>
#include <IMPL/LCCollectionVec.h>
#include <IMPL/TrackerHitImpl.h>
#include <IMPL/TrackImpl.h>
#include <IMPL/LCFlagImpl.h>
#include <Exceptions.h>

// ROOT includes
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
#include <TRandom.h>
#include <TMinuit.h>
#include <TSystem.h>
#include <TMath.h>
#endif

// system includes <>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <memory>
#include <cmath>
#include <iostream>
#include <fstream>

using namespace std;
using namespace lcio;
using namespace marlin;
using namespace eutelescope;

#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
//evil global variables ...
//std::vector<EUTelMille::hit> hitsarray;
EUTelMille::hit *hitsarray;
unsigned int number_of_datapoints;
void fcn_wrapper(int &npar, double *gin, double &f, double *par, int iflag)
{
  EUTelMille::trackfitter fobj(hitsarray,number_of_datapoints);
  double p[4];
  p[0] = 0.0;
  p[1] = 0.0;
  p[2] = 0.0;
  p[3] = 0.0;
  f = fobj.fit(par,p);
}
#endif




// definition of static members mainly used to name histograms
#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)
std::string EUTelMille::_numberTracksLocalname   = "NumberTracks";
std::string EUTelMille::_chi2XLocalname          = "Chi2X";
std::string EUTelMille::_chi2YLocalname          = "Chi2Y";
std::string EUTelMille::_residualXLocalname      = "ResidualX";
std::string EUTelMille::_residualYLocalname      = "ResidualY";
std::string EUTelMille::_residualZLocalname      = "ResidualZ";

#endif




EUTelMille::EUTelMille () : Processor("EUTelMille") {

  //some default values
  FloatVec MinimalResidualsX;
  FloatVec MinimalResidualsY;
  FloatVec MaximalResidualsX;
  FloatVec MaximalResidualsY;

  FloatVec PedeUserStartValuesX;
  FloatVec PedeUserStartValuesY;

  FloatVec PedeUserStartValuesGamma;

  FloatVec SensorZPositions;

  FloatVec SensorXShifts;
  FloatVec SensorYShifts;

  FloatVec SensorGamma;

  FloatVec SensorAlpha;

  FloatVec SensorBeta;

  //maybe one has to chose a larger value than 6?
  for(int i =0; i<6;i++)
    {
      MinimalResidualsX.push_back(0.0);
      MinimalResidualsY.push_back(0.0);
      MaximalResidualsX.push_back(0.0);
      MaximalResidualsY.push_back(0.0);

      PedeUserStartValuesX.push_back(0.0);
      PedeUserStartValuesY.push_back(0.0);

      PedeUserStartValuesGamma.push_back(0.0);

      float zpos = 20000.0 +  20000.0 * (float)i;
      SensorZPositions.push_back(zpos);

      SensorXShifts.push_back(0.0);
      SensorYShifts.push_back(0.0);

      SensorGamma.push_back(0.0);
      SensorAlpha.push_back(0.0);
      SensorBeta.push_back(0.0);
    }



  // modify processor description
  _description =
    "EUTelMille uses the MILLE program to write data files for MILLEPEDE II.";

  // choose input mode
  registerOptionalParameter("InputMode","Selects the source of input hits."
                            "\n0 - hits read from hitfile with simple trackfinding. "
                            "\n1 - hits read from output of tracking processor. "
                            "\n2 - Test mode. Simple internal simulation and simple trackfinding. "
                            "\n3 - Mixture of a track collection from the telescope and hit collections for the DUT (only one DUT layer can be used unfortunately)",
                            _inputMode, static_cast <int> (0));

  // input collections
  std::vector<std::string > HitCollectionNameVecExample;
  HitCollectionNameVecExample.push_back("corrhits");

  registerInputCollections(LCIO::TRACKERHIT,"HitCollectionName",
                           "Hit collections name",
                           _hitCollectionName,HitCollectionNameVecExample);

  registerInputCollection(LCIO::TRACK,"TrackCollectionName",
                          "Track collection name",
                          _trackCollectionName,std::string("fittracks"));

  // parameters

  registerOptionalParameter("DistanceMax","Maximal allowed distance between hits entering the fit per 10 cm space between the planes.",
                            _distanceMax, static_cast <float> (2000.0));

  registerOptionalParameter("DistanceMaxVec","Maximal allowed distance between hits entering the fit per 10 cm space between the planes. One value for each neighbor planes. DistanceMax will be used for each pair if this vector is empty.",
                            _distanceMaxVec, std::vector<float> ());

  

  registerOptionalParameter("ExcludePlanes","Exclude planes from fit according to their sensor ids.",_excludePlanes_sensorIDs ,std::vector<int>());

  registerOptionalParameter("FixedPlanes","Fix sensor planes in the fit according to their sensor ids.",_FixedPlanes_sensorIDs ,std::vector<int>());



  registerOptionalParameter("MaxTrackCandidates","Maximal number of track candidates.",_maxTrackCandidates, static_cast <int> (2000));

  registerOptionalParameter("BinaryFilename","Name of the Millepede binary file.",_binaryFilename, string ("mille.bin"));

  registerOptionalParameter("TelescopeResolution","Resolution of the telescope for Millepede (sigma_x=sigma_y.",_telescopeResolution, static_cast <float> (3.0));

  registerOptionalParameter("OnlySingleHitEvents","Use only events with one hit in every plane.",_onlySingleHitEvents, static_cast <int> (0));

  registerOptionalParameter("OnlySingleTrackEvents","Use only events with one track candidate.",_onlySingleTrackEvents, static_cast <int> (0));

  registerOptionalParameter("AlignMode","Number of alignment constants used. Available mode are: "
                            "\n1 - shifts in the X and Y directions and a rotation around the Z axis,"
                            "\n2 - only shifts in the X and Y directions"
                            "\n3 - (EXPERIMENTAL) shifts in the X,Y and Z directions and rotations around all three axis",
                            _alignMode, static_cast <int> (1));

  registerOptionalParameter("UseResidualCuts","Use cuts on the residuals to reduce the combinatorial background. 0 for off (default), 1 for on",_useResidualCuts,
                            static_cast <int> (0));

  registerOptionalParameter("AlignmentConstantLCIOFile","This is the name of the LCIO file name with the output alignment"
                            "constants (add .slcio)",_alignmentConstantLCIOFile, static_cast< string > ( "alignment.slcio" ) );

  registerOptionalParameter("AlignmentConstantCollectionName", "This is the name of the alignment collection to be saved into the slcio file",
                            _alignmentConstantCollectionName, static_cast< string > ( "alignment" ));

  registerOptionalParameter("ResidualsXMin","Minimal values of the hit residuals in the X direction for a track. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_residualsXMin,MinimalResidualsX);

  registerOptionalParameter("ResidualsYMin","Minimal values of the hit residuals in the Y direction for a track. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_residualsYMin,MinimalResidualsY);

  registerOptionalParameter("ResidualsXMax","Maximal values of the hit residuals in the X direction for a track. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_residualsXMax,MaximalResidualsX);

  registerOptionalParameter("ResidualsYMax","Maximal values of the hit residuals in the Y direction for a track. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_residualsYMax,MaximalResidualsY);



  registerOptionalParameter("ResolutionX","X resolution parameter for each plane. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_resolutionX,  std::vector<float> (static_cast <int> (6), 3.5));

  registerOptionalParameter("ResolutionY","Y resolution parameter for each plane. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_resolutionY,std::vector<float> (static_cast <int> (6), 3.5));

  registerOptionalParameter("ResolutionZ","Z resolution parameter for each plane. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_resolutionZ,std::vector<float> (static_cast <int> (6), 3.5));


 

  registerOptionalParameter("FixParameter","Fixes the given alignment parameters in the fit if alignMode==3 is used. For each sensor an integer must be specified (If no value is given, then all parameters will be free). bit 0 = x shift, bit 1 = y shift, bit 2 = z shift, bit 3 = alpha, bit 4 = beta, bit 5 = gamma. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_FixParameter, std::vector<int> (static_cast <int> (6), 24));

  registerOptionalParameter("GeneratePedeSteerfile","Generate a steering file for the pede program.",_generatePedeSteerfile, static_cast <int> (0));

  registerOptionalParameter("PedeSteerfileName","Name of the steering file for the pede program.",_pedeSteerfileName, string("steer_mille.txt"));

  registerOptionalParameter("RunPede","Execute the pede program using the generated steering file.",_runPede, static_cast <int> (0));

  registerOptionalParameter("UsePedeUserStartValues","Give start values for pede by hand (0 - automatic calculation of start values, 1 - start values defined by user).", _usePedeUserStartValues, static_cast <int> (0));

  registerOptionalParameter("PedeUserStartValuesX","Start values for the alignment for shifts in the X direction.",_pedeUserStartValuesX,PedeUserStartValuesX);

  registerOptionalParameter("PedeUserStartValuesY","Start values for the alignment for shifts in the Y direction.",_pedeUserStartValuesY,PedeUserStartValuesY);

  registerOptionalParameter("PedeUserStartValuesZ","Start values for the alignment for shifts in the Z direction.",_pedeUserStartValuesZ,std::vector<float> (static_cast <int> (6), 0.0));

  registerOptionalParameter("PedeUserStartValuesAlpha","Start values for the alignment for the angle alpha.",_pedeUserStartValuesAlpha,std::vector<float> (static_cast <int> (6), 0.0));
  
  registerOptionalParameter("PedeUserStartValuesBeta","Start values for the alignment for the angle beta.",_pedeUserStartValuesBeta,std::vector<float> (static_cast <int> (6), 0.0));
  
  registerOptionalParameter("PedeUserStartValuesGamma","Start values for the alignment for the angle gamma.",_pedeUserStartValuesGamma,PedeUserStartValuesGamma);

  registerOptionalParameter("TestModeSensorResolution","Resolution assumed for the sensors in test mode.",_testModeSensorResolution, static_cast <float> (3.0));

  registerOptionalParameter("TestModeXTrackSlope","Width of the track slope distribution in the x direction",_testModeXTrackSlope, static_cast <float> (0.0005));

  registerOptionalParameter("TestModeYTrackSlope","Width of the track slope distribution in the y direction",_testModeYTrackSlope, static_cast <float> (0.0005));

  registerOptionalParameter("TestModeSensorZPositions","Z positions of the sensors in test mode.",_testModeSensorZPositions,SensorZPositions);

  registerOptionalParameter("TestModeSensorXShifts","X shifts of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorXShifts,SensorXShifts);

  registerOptionalParameter("TestModeSensorYShifts","Y shifts of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorYShifts,SensorYShifts);


  registerOptionalParameter("TestModeSensorGamma","Rotation around the z axis of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorGamma,SensorGamma);


  registerOptionalParameter("TestModeSensorAlpha","Rotation around the x axis of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorAlpha,SensorAlpha);


  registerOptionalParameter("TestModeSensorBeta","Rotation around the y axis of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorBeta,SensorBeta);

}

void EUTelMille::init() {
  // check if Marlin was built with GEAR support or not
#ifndef USE_GEAR

  streamlog_out ( ERROR2 ) << "Marlin was not built with GEAR support." << endl;
  streamlog_out ( ERROR2 ) << "You need to install GEAR and recompile Marlin with -DUSE_GEAR before continue." << endl;

  exit(-1);

#else

  // check if the GEAR manager pointer is not null!
  if ( Global::GEAR == 0x0 ) {
    streamlog_out ( ERROR2) << "The GearMgr is not available, for an unknown reason." << endl;
    exit(-1);
  }


#if !defined(USE_ROOT) && !defined(MARLIN_USE_ROOT)
  if(_alignMode == 3)
    {
      streamlog_out ( ERROR2) << "alignMode == 3 was chosen but Eutelescope was not build with ROOT support!" << endl;
      exit(-1);   
    }  
#endif

  _siPlanesParameters  = const_cast<gear::SiPlanesParameters* > (&(Global::GEAR->getSiPlanesParameters()));
  _siPlanesLayerLayout = const_cast<gear::SiPlanesLayerLayout*> ( &(_siPlanesParameters->getSiPlanesLayerLayout() ));

  _histogramSwitch = true;

  //lets guess the number of planes
  if(_inputMode == 0 || _inputMode == 2) 
    {

      // the number of planes is got from the GEAR description and is
      // the sum of the telescope reference planes and the DUT (if
      // any)
      _nPlanes = _siPlanesParameters->getSiPlanesNumber();
      if ( _siPlanesParameters->getSiPlanesType() == _siPlanesParameters->TelescopeWithDUT ) {
        ++_nPlanes;
      }

    }
  else if(_inputMode == 1)
    {
      _nPlanes = _siPlanesParameters->getSiPlanesNumber();
    }
  else if(_inputMode == 3)
    {

      // the number of planes is got from the GEAR description and is
      // the sum of the telescope reference planes and the DUT (if
      // any)
      _nPlanes = _siPlanesParameters->getSiPlanesNumber();
      if ( _siPlanesParameters->getSiPlanesType() == _siPlanesParameters->TelescopeWithDUT ) {
        ++_nPlanes;
      }
    }
  else
    {
      cout << "unknown input mode " << _inputMode << endl;
      exit(-1);
    }
  
  // an associative map for getting also the sensorID ordered
  map< double, int > sensorIDMap;
  //lets create an array with the z positions of each layer
  for ( int iPlane = 0 ; iPlane < _siPlanesLayerLayout->getNLayers(); iPlane++ ) {
    _siPlaneZPosition.push_back(_siPlanesLayerLayout->getLayerPositionZ(iPlane));
    sensorIDMap.insert( make_pair( _siPlanesLayerLayout->getLayerPositionZ(iPlane), _siPlanesLayerLayout->getID(iPlane) ) );
  }

  if  ( _siPlanesParameters->getSiPlanesType() == _siPlanesParameters->TelescopeWithDUT ) {
    _siPlaneZPosition.push_back(_siPlanesLayerLayout->getDUTPositionZ());
    sensorIDMap.insert( make_pair( _siPlanesLayerLayout->getDUTPositionZ(),  _siPlanesLayerLayout->getDUTID() ) ) ;
  }

  //lets sort the array with increasing z
  sort(_siPlaneZPosition.begin(), _siPlaneZPosition.end());

  
  //the user is giving sensor ids for the planes to be excluded. this
  //sensor ids have to be converted to a local index according to the
  //planes positions along the z axis.
  for (size_t i = 0; i < _FixedPlanes_sensorIDs.size(); i++)
    {
      map< double, int >::iterator iter = sensorIDMap.begin();
      int counter = 0;
      while ( iter != sensorIDMap.end() ) {
        if( iter->second == _FixedPlanes_sensorIDs[i])
          {
            _FixedPlanes.push_back(counter);
            break;
          }
        ++iter;
        ++counter;
      }
    }
  for (size_t i = 0; i < _excludePlanes_sensorIDs.size(); i++)
    {
      map< double, int >::iterator iter = sensorIDMap.begin();
      int counter = 0;
      while ( iter != sensorIDMap.end() ) {
        if( iter->second == _excludePlanes_sensorIDs[i])
          {
//            printf("excludePlanes_sensorID %2d of %2d (%2d) \n", i, _excludePlanes_sensorIDs.size(), counter );
            _excludePlanes.push_back(counter);
            break;
          }
        ++iter;
        ++counter;
      }
    }
  
  // strip from the map the sensor id already sorted.
  map< double, int >::iterator iter = sensorIDMap.begin();
  int counter = 0;
  while ( iter != sensorIDMap.end() ) {
    bool excluded = false;
    for (size_t i = 0; i < _excludePlanes.size(); i++)
      {
        if(_excludePlanes[i] == counter)
          {
//            printf("excludePlanes %2d of %2d (%2d) \n", i, _excludePlanes_sensorIDs.size(), counter );
            excluded = true;
            break;
          }
      }
    if(!excluded)
      _orderedSensorID_wo_excluded.push_back( iter->second );
    _orderedSensorID.push_back( iter->second );
    
    ++iter;
    ++counter;
  }
  //


  //consistency
  if((int)_siPlaneZPosition.size() != _nPlanes)
    {
      streamlog_out ( ERROR2 ) << "the number of detected planes is " << _nPlanes << " but only " << _siPlaneZPosition.size() << " layer z positions were found!"  << endl;
      exit(-1);
    }

#endif



  // this method is called only once even when the rewind is active
  // usually a good idea to
  printParameters ();

  // set to zero the run and event counters
  _iRun = 0;
  _iEvt = 0;

  // Initialize number of excluded planes
  _nExcludePlanes = _excludePlanes.size();

  streamlog_out ( MESSAGE2 ) << "Number of planes excluded from the alignment fit: " << _nExcludePlanes << endl;

  // Initialise Mille statistics
  _nMilleDataPoints = 0;
  _nMilleTracks = 0;

  _waferResidX = new double[_nPlanes];
  _waferResidY = new double[_nPlanes];
  _waferResidZ = new double[_nPlanes];
  
  
  _xFitPos = new double[_nPlanes];
  _yFitPos = new double[_nPlanes];

  _telescopeResolX = new double[_nPlanes];
  _telescopeResolY = new double[_nPlanes];
  _telescopeResolZ = new double[_nPlanes];

  //check the consistency of the resolution parameters
  if(_alignMode == 3)
    {
      if(
         _resolutionX.size() != static_cast<unsigned int>(_nPlanes ) ||
         _resolutionY.size() != static_cast<unsigned int>(_nPlanes ) ||
         _resolutionZ.size() != static_cast<unsigned int>(_nPlanes )
         )
        {
//  ADDITIONAL PRINTOUTS   
//         for(int i = 0; i < _resolutionX.size(); i++)
//          { 
//              printf("_resolutionX.at(%2d) = %8.3f; _resolutionY.at(%2d) = %8.3f;  _resolutionZ.at(%2d) = %8.3f;  \n", 
//                      i,_resolutionX.at(i), i,_resolutionY.at(i), i,_resolutionZ.at(i) );
//          }
          streamlog_out ( WARNING2 ) << "Consistency check of the resolution parameters failed. The array size is different than the number of found planes! The resolution parameters are set to default values now (see variable TelescopeResolution). This introduces a bias if the real values for X,Y and Z are rather different." << endl;
          _resolutionX.clear();
          _resolutionY.clear();
          _resolutionZ.clear();
          for(int i = 0; i < _nPlanes; i++)
            {
              _resolutionX.push_back(_telescopeResolution);
              _resolutionY.push_back(_telescopeResolution);
              _resolutionZ.push_back(_telescopeResolution);
            }
        }

      if(_FixParameter.size() != static_cast<unsigned int>(_nPlanes ) && _FixParameter.size() > 0)
        {
//  ADDITIONAL PRINTOUTS   
//       for(int i = 0; i < _FixParameter.size(); i++) printf(" _FixParameter(%2d) = %3d \n", i, _FixParameter.at(i) );
         
         streamlog_out ( WARNING2 ) << "Consistency check of the fixed parameters array failed. The array size is different than the number of found planes! The array is now set to default values, which means that all parameters are free in the fit." << endl;
          _FixParameter.clear();
          for(int i = 0; i < _nPlanes; i++)
            {
              _FixParameter.push_back(0);
            }
        }
      if(_FixParameter.size() == 0)
        {
          streamlog_out ( WARNING2 ) << "The fixed parameters array was found to be empty. It will be filled with default values. All parameters are free in the fit now." << endl;
          _FixParameter.clear();
          for(int i = 0; i < _nPlanes; i++)
            {
              _FixParameter.push_back(0);
            }
        }
    }

#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
  if(_alignMode == 3)
    {
      number_of_datapoints = _nPlanes -_nExcludePlanes;
      hitsarray = new hit[number_of_datapoints];
    }
#endif

  // booking histograms
  bookHistos();

  streamlog_out ( MESSAGE2 ) << "Initialising Mille..." << endl;
  _mille = new Mille(_binaryFilename.c_str());
  streamlog_out ( MESSAGE2 ) << "The filename for the binary file is: " << _binaryFilename.c_str() << endl;

  for(int i = 0; i < _maxTrackCandidates; i++)
    {
      _xPos.push_back(std::vector<double>(_nPlanes,0.0));
      _yPos.push_back(std::vector<double>(_nPlanes,0.0));
      _zPos.push_back(std::vector<double>(_nPlanes,0.0));
    }

  if(_distanceMaxVec.size() > 0)
    {
      if(_distanceMaxVec.size() !=  static_cast<unsigned int>(_nPlanes )-1 )
        {
          streamlog_out ( WARNING2 ) << "Consistency check of the DistanceMaxVec array failed. Its size is different compared to the number of planes! Will now use _distanceMax for each pair of planes." << endl;
          _distanceMaxVec.clear();
          for(int i = 0; i < _nPlanes-1; i++)
            {
              _distanceMaxVec.push_back(_distanceMax);
            }
        }
    }
  else
    {
      _distanceMaxVec.clear();
      for(int i = 0; i < _nPlanes-1; i++)
        {
          _distanceMaxVec.push_back(_distanceMax);
        }
    }
  streamlog_out ( MESSAGE2 ) << "end of init" << endl;
}

void EUTelMille::processRunHeader (LCRunHeader * rdr) {

  auto_ptr<EUTelRunHeaderImpl> header ( new EUTelRunHeaderImpl (rdr) );
  header->addProcessor( type() ) ;


  // this is the right place also to check the geometry ID. This is a
  // unique number identifying each different geometry used at the
  // beam test. The same number should be saved in the run header and
  // in the xml file. If the numbers are different, instead of barely
  // quitting ask the user what to do.

  if ( header->getGeoID() != _siPlanesParameters->getSiPlanesID() ) {
    streamlog_out ( ERROR2 ) << "Error during the geometry consistency check: " << endl;
    streamlog_out ( ERROR2 ) << "The run header says the GeoID is " << header->getGeoID() << endl;
    streamlog_out ( ERROR2 ) << "The GEAR description says is     " << _siPlanesParameters->getSiPlanesNumber() << endl;

#ifdef EUTEL_INTERACTIVE
    string answer;
    while (true) {
      streamlog_out ( ERROR2 ) << "Type Q to quit now or C to continue using the actual GEAR description anyway [Q/C]" << endl;
      cin >> answer;
      // put the answer in lower case before making the comparison.
      transform( answer.begin(), answer.end(), answer.begin(), ::tolower );
      if ( answer == "q" ) {
        exit(-1);
      } else if ( answer == "c" ) {
        break;
      }
    }
#endif

  }

  // increment the run counter
  ++_iRun;
}



void EUTelMille::findtracks(
                            std::vector<std::vector<int> > &indexarray,
                            std::vector<int> vec,
                            std::vector<std::vector<EUTelMille::HitsInPlane> > &_hitsArray,
                            int i,
                            int y
                            )
{
 if(i>0)
    vec.push_back(y);

//  ADDITIONAL PRINTOUTS   
// printf("come to the  plane id=%3d y=%3d  : : _hitsArray[i].size()=%5d \n", i,y, _hitsArray[i].size() );
 for(size_t j =0; j < _hitsArray[i].size(); j++)
    {
      //if we are not in the last plane, call this method again
      if(i<(int)(_hitsArray.size())-1)
        {
          vec.push_back((int)j); //index of the cluster in the last plane
         
          //track candidate requirements
          bool taketrack = true;
          const int e = vec.size()-2;
          if(e >= 0)
            {
              double distance = sqrt(
                                     pow( _hitsArray[e][vec[e]].measuredX - _hitsArray[e+1][vec[e+1]].measuredX ,2) +
                                     pow( _hitsArray[e][vec[e]].measuredY - _hitsArray[e+1][vec[e+1]].measuredY ,2)
                                     );
              double distance_z = _hitsArray[e+1][vec[e+1]].measuredZ - _hitsArray[e][vec[e]].measuredZ;
              
              
              const double dM = _distanceMaxVec[e];
              
              double distancemax = dM * ( distance_z / 100000.0);
              
              if( distance >= distancemax )
                taketrack = false;
              
              if(_onlySingleHitEvents == 1 && (_hitsArray[e].size() != 1 || _hitsArray[e+1].size() != 1))
                taketrack = false;
            }
          vec.pop_back(); 
//  ADDITIONAL PRINTOUTS   
//          printf("come to the next plane id=%3d clu=%3d of %3d \n", i, j, _hitsArray[i].size());
          if(taketrack)
            findtracks(indexarray,vec, _hitsArray, i+1,(int)j);
        }
      else
        {
          //we are in the last plane
          vec.push_back((int)j); //index of the cluster in the last plane

          //track candidate requirements
          bool taketrack = true;
          for(size_t e =0; e < vec.size()-1; e++)
            {
              double distance = sqrt(
                                     pow( _hitsArray[e][vec[e]].measuredX - _hitsArray[e+1][vec[e+1]].measuredX ,2) +
                                     pow( _hitsArray[e][vec[e]].measuredY - _hitsArray[e+1][vec[e+1]].measuredY ,2)
                                     );
              double distance_z = _hitsArray[e+1][vec[e+1]].measuredZ - _hitsArray[e][vec[e]].measuredZ;
             
              const double dM = _distanceMaxVec[e];
             
              double distancemax = dM * ( distance_z / 100000.0);

              if( distance >= distancemax )
                taketrack = false;

              if(_onlySingleHitEvents == 1 && (_hitsArray[e].size() != 1 || _hitsArray[e+1].size() != 1))
                taketrack = false;

            }
          if((int)indexarray.size() >= _maxTrackCandidates)
            taketrack = false;
//  ADDITIONAL PRINTOUTS   
//          printf("come to the last plane id=%3d clu=%3d of %3d ; vec.size=%5d\n", i, j, _hitsArray[i].size(), vec.size());
 
          if(taketrack)
            {
              indexarray.push_back(vec);
            }
          vec.pop_back(); //last element must be removed because the
                          //vector is still used
        }
    }
}


void EUTelMille::FitTrack(int nPlanesFitter, double xPosFitter[], double yPosFitter[], double zPosFitter[], double xResFitter[], double yResFitter[], double chi2Fit[2], double residXFit[], double residYFit[], double angleFit[2]) {

  int sizearray;

  if (_nExcludePlanes > 0) {
    sizearray = nPlanesFitter - _nExcludePlanes;
  } else {
    sizearray = nPlanesFitter;
  }

  double * xPosFit = new double[sizearray];
  double * yPosFit = new double[sizearray];
  double * zPosFit = new double[sizearray];
  double * xResFit = new double[sizearray];
  double * yResFit = new double[sizearray];

  int nPlanesFit = 0;

  for (int help = 0; help < nPlanesFitter; help++) {

    int excluded = 0;

    // check if actual plane is excluded
    if (_nExcludePlanes > 0) {
      for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
        if (help == _excludePlanes[helphelp]) {
          excluded = 1;
        }
//        printf("735: excludePlanes %2d of %2d (%2d) \n", helphelp, _nExcludePlanes, excluded );
      }
    }

    if (excluded == 1) {
      // do noting
    } else {
      xPosFit[nPlanesFit] = xPosFitter[help];
      yPosFit[nPlanesFit] = yPosFitter[help];
      zPosFit[nPlanesFit] = zPosFitter[help];
      xResFit[nPlanesFit] = xResFitter[help];
      yResFit[nPlanesFit] = yResFitter[help];
      nPlanesFit++;
    }
  }

  // ++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // ++++++++++++ See Blobel Page 226 !!! +++++++++++++++++
  // ++++++++++++++++++++++++++++++++++++++++++++++++++++++

  int counter;

  float S1[2]   = {0,0};
  float Sx[2]   = {0,0};
  float Xbar[2] = {0,0};

  float * Zbar_X = new float[nPlanesFit];
  float * Zbar_Y = new float[nPlanesFit];
  for (counter = 0; counter < nPlanesFit; counter++){
    Zbar_X[counter] = 0.;
    Zbar_Y[counter] = 0.;
  }

  float Sy[2]     = {0,0};
  float Ybar[2]   = {0,0};
  float Sxybar[2] = {0,0};
  float Sxxbar[2] = {0,0};
  float A2[2]     = {0,0};

  // define S1
  for( counter = 0; counter < nPlanesFit; counter++ ){
    S1[0] = S1[0] + 1/pow(xResFit[counter],2);
    S1[1] = S1[1] + 1/pow(yResFit[counter],2);
  }

  // define Sx
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Sx[0] = Sx[0] + zPosFit[counter]/pow(xResFit[counter],2);
    Sx[1] = Sx[1] + zPosFit[counter]/pow(yResFit[counter],2);
  }

  // define Xbar
  Xbar[0]=Sx[0]/S1[0];
  Xbar[1]=Sx[1]/S1[1];

  // coordinate transformation !! -> bar
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Zbar_X[counter] = zPosFit[counter]-Xbar[0];
    Zbar_Y[counter] = zPosFit[counter]-Xbar[1];
  }

  // define Sy
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Sy[0] = Sy[0] + xPosFit[counter]/pow(xResFit[counter],2);
    Sy[1] = Sy[1] + yPosFit[counter]/pow(yResFit[counter],2);
  }

  // define Ybar
  Ybar[0]=Sy[0]/S1[0];
  Ybar[1]=Sy[1]/S1[1];

  // define Sxybar
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Sxybar[0] = Sxybar[0] + Zbar_X[counter] * xPosFit[counter]/pow(xResFit[counter],2);
    Sxybar[1] = Sxybar[1] + Zbar_Y[counter] * yPosFit[counter]/pow(yResFit[counter],2);
  }

  // define Sxxbar
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Sxxbar[0] = Sxxbar[0] + Zbar_X[counter] * Zbar_X[counter]/pow(xResFit[counter],2);
    Sxxbar[1] = Sxxbar[1] + Zbar_Y[counter] * Zbar_Y[counter]/pow(yResFit[counter],2);
  }

  // define A2
  A2[0]=Sxybar[0]/Sxxbar[0];
  A2[1]=Sxybar[1]/Sxxbar[1];

  // Calculate chi sqaured
  // Chi^2 for X and Y coordinate for hits in all planes
  for( counter = 0; counter < nPlanesFit; counter++ ){
    chi2Fit[0] += pow(-zPosFit[counter]*A2[0]
                      +xPosFit[counter]-Ybar[0]+Xbar[0]*A2[0],2)/pow(xResFit[counter],2);
    chi2Fit[1] += pow(-zPosFit[counter]*A2[1]
                      +yPosFit[counter]-Ybar[1]+Xbar[1]*A2[1],2)/pow(yResFit[counter],2);
  }

  for( counter = 0; counter < nPlanesFitter; counter++ ) {
    residXFit[counter] = (Ybar[0]-Xbar[0]*A2[0]+zPosFitter[counter]*A2[0])-xPosFitter[counter];
    residYFit[counter] = (Ybar[1]-Xbar[1]*A2[1]+zPosFitter[counter]*A2[1])-yPosFitter[counter];

    // residXFit[counter] = xPosFitter[counter] - (Ybar[0]-Xbar[0]*A2[0]+zPosFitter[counter]*A2[0]);
    // residYFit[counter] = yPosFitter[counter] - (Ybar[1]-Xbar[1]*A2[1]+zPosFitter[counter]*A2[1]);

  }

  // define angle
  angleFit[0] = atan(A2[0]);
  angleFit[1] = atan(A2[1]);

  // clean up
  delete [] zPosFit;
  delete [] yPosFit;
  delete [] xPosFit;
  delete [] yResFit;
  delete [] xResFit;

  delete [] Zbar_X;
  delete [] Zbar_Y;

}

void EUTelMille::processEvent (LCEvent * event) {


  if (_iEvt % 10 == 0) {
    streamlog_out( MESSAGE2 ) << "Processing event "
                              << setw(6) << setiosflags(ios::right) << event->getEventNumber() << " in run "
                              << setw(6) << setiosflags(ios::right) << setfill('0')  << event->getRunNumber() << setfill(' ')
                              << " (Total = " << setw(10) << _iEvt << ")" << resetiosflags(ios::left) << endl;
    streamlog_out( MESSAGE2 ) << "Currently having " << _nMilleDataPoints << " data points in "
                              << _nMilleTracks << " tracks " << endl;
  }


  // fill resolution arrays
  for (int help = 0; help < _nPlanes; help++) {
    _telescopeResolX[help] = _telescopeResolution;
    _telescopeResolY[help] = _telescopeResolution;
  }

  EUTelEventImpl * evt = static_cast<EUTelEventImpl*> (event) ;

  if ( evt->getEventType() == kEORE ) {
    streamlog_out ( DEBUG2 ) << "EORE found: nothing else to do." << endl;
    return;
  }

  std::vector<std::vector<EUTelMille::HitsInPlane> > _hitsArray(_nPlanes - _nExcludePlanes, std::vector<EUTelMille::HitsInPlane>());
  std::vector<int> indexconverter (_nPlanes,-1);
 
  

//  printf("887: excludePlanes %2d  \n", _nExcludePlanes );
//  if ( _nExcludePlanes > 0 )
  {
 
    
      int icounter = 0;
      for(int i = 0; i < _nPlanes; i++)
      {
          int excluded = 0; //0 - not excluded, 1 - excluded
          if ( _nExcludePlanes > 0 )
          {
              for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
                  if (i == _excludePlanes[helphelp] ) {
                      excluded = 1;
                      break;//leave the for loop
                  }
              }
          }
          if(excluded == 1)
              indexconverter[i] = -1;
          else
          {
              indexconverter[i] = icounter;
              icounter++;
          }
 
//        printf("907: excludePlanes %2d of %2d (%2d) \n", i, _nPlanes, indexconverter[i] );
      }
  }

//  std::vector<std::vector<EUTelMille::HitsInPlane> > _hitsArray(_nPlanes, std::vector<EUTelMille::HitsInPlane>());

//  printf(" inputMode = %5d  hitCollectionName.size = %5d, indexconverter.size= %5d \n", _inputMode, _hitCollectionName.size(), indexconverter.size() );
//     for(size_t i =0;i < indexconverter.size();i++)
//      {
//          std::cout << " " << indexconverter[i] ;
//      }
//     std::cout << std::endl;
  
  if (_inputMode != 1 && _inputMode != 3)
    for(size_t i =0;i < _hitCollectionName.size();i++)
      {

        LCCollection* collection;
        try {
          collection = event->getCollection(_hitCollectionName[i]);
        } catch (DataNotAvailableException& e) {
          streamlog_out ( WARNING2 ) << "No input collection " << _hitCollectionName[i] << " found for event " << event->getEventNumber()
                                     << " in run " << event->getRunNumber() << endl;
          throw SkipEventException(this);
        }
        int layerIndex = -1;
        HitsInPlane hitsInPlane;

        // check if running in input mode 0 or 2
        if (_inputMode == 0) {

          // loop over all hits in collection
          for ( int iHit = 0; iHit < collection->getNumberOfElements(); iHit++ ) {

            TrackerHitImpl * hit = static_cast<TrackerHitImpl*> ( collection->getElementAt(iHit) );

            LCObjectVec clusterVector = hit->getRawHits();

            EUTelVirtualCluster * cluster;

            if ( hit->getType() == kEUTelBrickedClusterImpl ) {

               // fixed cluster implementation. Remember it
               //  can come from
               //  both RAW and ZS data
   
                cluster = new EUTelBrickedClusterImpl(static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                
            } else if ( hit->getType() == kEUTelDFFClusterImpl ) {
              
              // fixed cluster implementation. Remember it can come from
              // both RAW and ZS data
              cluster = new EUTelDFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
            } else if ( hit->getType() == kEUTelFFClusterImpl ) {
              
              // fixed cluster implementation. Remember it can come from
              // both RAW and ZS data
              cluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
            } else if ( hit->getType() == kEUTelSparseClusterImpl ) {

              // ok the cluster is of sparse type, but we also need to know
              // the kind of pixel description used. This information is
              // stored in the corresponding original data collection.

              LCCollectionVec * sparseClusterCollectionVec = dynamic_cast < LCCollectionVec * > (evt->getCollection("original_zsdata"));
              TrackerDataImpl * oneCluster = dynamic_cast<TrackerDataImpl*> (sparseClusterCollectionVec->getElementAt( 0 ));
              CellIDDecoder<TrackerDataImpl > anotherDecoder(sparseClusterCollectionVec);
              SparsePixelType pixelType = static_cast<SparsePixelType> ( static_cast<int> ( anotherDecoder( oneCluster )["sparsePixelType"] ));

              // now we know the pixel type. So we can properly create a new
              // instance of the sparse cluster
              if ( pixelType == kEUTelSimpleSparsePixel ) {

                cluster = new EUTelSparseClusterImpl< EUTelSimpleSparsePixel >
                  ( static_cast<TrackerDataImpl *> ( clusterVector[ 0 ]  ) );

              } else {
                streamlog_out ( ERROR4 ) << "Unknown pixel type.  Sorry for quitting." << endl;
                throw UnknownDataTypeException("Pixel type unknown");
              }

            } else {
              throw UnknownDataTypeException("Unknown cluster type");
            }

            double minDistance =  numeric_limits< double >::max() ;
            double * hitPosition = const_cast<double * > (hit->getPosition());

            for ( int i = 0 ; i < (int)_siPlaneZPosition.size(); i++ )
              {
                double distance = std::abs( hitPosition[2] - _siPlaneZPosition[i] );
                if ( distance < minDistance )
                  {
                    minDistance = distance;
                    layerIndex = i;
                  }
              }
            if ( minDistance > 5 /* mm */ ) {
              // advice the user that the guessing wasn't successful
              streamlog_out( WARNING3 ) << "A hit was found " << minDistance << " mm far from the nearest plane\n"
                "Please check the consistency of the data with the GEAR file" << endl;
            }


            // Getting positions of the hits.
            // ------------------------------
            hitsInPlane.measuredX = 1000 * hit->getPosition()[0];
            hitsInPlane.measuredY = 1000 * hit->getPosition()[1];
            hitsInPlane.measuredZ = 1000 * hit->getPosition()[2];

//            printf("hit %5d of %5d , at %-8.1f %-8.1f %-8.1f, %5d %5d \n", iHit , collection->getNumberOfElements(), hitsInPlane.measuredX, hitsInPlane.measuredY, hitsInPlane.measuredZ, indexconverter[layerIndex], layerIndex );
            // adding a requirement to use only fat clusters for alignement
//            if(cluster->getTotalCharge()>1){
              if(indexconverter[layerIndex] != -1)
                    _hitsArray[indexconverter[layerIndex]].push_back(hitsInPlane);
//            }

            delete cluster; // <--- destroying the cluster

//COMMENT 190510-2227            if(indexconverter[layerIndex] != -1) 
//COMMENT            _hitsArray[indexconverter[layerIndex]].push_back(hitsInPlane);
            
//            _hitsArray[layerIndex].push_back(hitsInPlane);

          } // end loop over all hits in collection

        } else if (_inputMode == 2) {

#if defined( USE_ROOT ) || defined(MARLIN_USE_ROOT)

          const float resolX = _testModeSensorResolution;
          const float resolY = _testModeSensorResolution;

          const float xhitpos = gRandom->Uniform(-3500.0,3500.0);
          const float yhitpos = gRandom->Uniform(-3500.0,3500.0);

          const float xslope = gRandom->Gaus(0.0,_testModeXTrackSlope);
          const float yslope = gRandom->Gaus(0.0,_testModeYTrackSlope);

          // loop over all planes
          for (int help = 0; help < _nPlanes; help++) {

            // The x and y positions are given by the sums of the measured
            // hit positions, the detector resolution, the shifts of the
            // planes and the effect due to the track slopes.
            hitsInPlane.measuredX = xhitpos + gRandom->Gaus(0.0,resolX) + _testModeSensorXShifts[help] + _testModeSensorZPositions[help] * tan(xslope) - _testModeSensorGamma[help] * yhitpos - _testModeSensorBeta[help] * _testModeSensorZPositions[0];
            hitsInPlane.measuredY = yhitpos + gRandom->Gaus(0.0,resolY) + _testModeSensorYShifts[help] + _testModeSensorZPositions[help] * tan(yslope) + _testModeSensorGamma[help] * xhitpos - _testModeSensorAlpha[help] * _testModeSensorZPositions[help];
            hitsInPlane.measuredZ = _testModeSensorZPositions[help];
            if(indexconverter[help] != -1) 
              _hitsArray[indexconverter[help]].push_back(hitsInPlane);

//  ADDITIONAL PRINTOUTS   
//            printf("plane:%3d hit:%3d %8.3f %8.3f %8.3f \n", help, indexconverter[help], hitsInPlane.measuredX,hitsInPlane.measuredY,hitsInPlane.measuredZ);
       _hitsArray[help].push_back(hitsInPlane);
            _telescopeResolX[help] = resolX;
            _telescopeResolY[help] = resolY;
          } // end loop over all planes

#else // USE_ROOT

          throw MissingLibraryException( this, "ROOT" );

#endif


        } // end if check running in input mode 0 or 2

      }

  std::vector<int> fitplane(_nPlanes, 0);

  for (int help = 0; help < _nPlanes; help++) {
    fitplane[help] = 1;
  }

  int _nTracks = 0;

  int _nGoodTracks = 0;

  // check if running in input mode 0 or 2 => perform simple track finding
  if (_inputMode == 0 || _inputMode == 2) {

    // Find track candidates using the distance cuts
    // ---------------------------------------------
    //
    // This is done separately for different numbers of planes.

    std::vector<std::vector<int> > indexarray;

//  ADDITIONAL PRINTOUTS   
//    printf("=========\n");
    findtracks(indexarray, std::vector<int>(), _hitsArray, 0, 0);
//    printf("indexarray size i = %5d %5d \n", indexarray.size(), indexconverter.size());
 
    for(size_t i = 0; i < indexarray.size(); i++)
      {
        for(size_t j = 0; j <  indexconverter.size(); j++)
          {

//  ADDITIONAL PRINTOUTS   
//             printf("indexarray size i = %5d, j = %5d (%2d)\n", i, j, indexconverter[j]);
              
             if(indexconverter[j] == -1)
             {
                //if the plane j is excluded set a fake hit. This hit
                //will be ignored by the fitter.
                _xPos[i][j] = 0.0;
                _yPos[i][j] = 0.0;
                _zPos[i][j] = 0.0;
             }
             else
             {
                //if the plane j is not excluded. note that j must be
                //converted using indexconverter into the index number
                //of _hitsArray, because _hitsArray does not contain
                //excluded planes.
                _xPos[i][j] = _hitsArray[indexconverter[j]][indexarray[i][indexconverter[j]]].measuredX;
                _yPos[i][j] = _hitsArray[indexconverter[j]][indexarray[i][indexconverter[j]]].measuredY;
                _zPos[i][j] = _hitsArray[indexconverter[j]][indexarray[i][indexconverter[j]]].measuredZ;
             }
//            _xPos[i][j] = _hitsArray[j][indexarray[i][j]].measuredX;
//            _yPos[i][j] = _hitsArray[j][indexarray[i][j]].measuredY;
//            _zPos[i][j] = _hitsArray[j][indexarray[i][j]].measuredZ;
          }
      }

    _nTracks = (int) indexarray.size();
//  ADDITIONAL PRINTOUTS   
//   printf("indexarray size i = %5d  _nTracks = %5d \n", indexarray.size(), _nTracks );
 
    // end check if running in input mode 0 or 2 => perform simple track finding
  } else if (_inputMode == 1) {
    LCCollection* collection;
    try {
      collection = event->getCollection(_trackCollectionName);
    } catch (DataNotAvailableException& e) {
      streamlog_out ( WARNING2 ) << "No input track collection " << _trackCollectionName  << " found for event " << event->getEventNumber()
                                 << " in run " << event->getRunNumber() << endl;
      throw SkipEventException(this);
    }
    const int nTracksHere = collection->getNumberOfElements();

    streamlog_out ( MILLEMESSAGE ) << "Number of tracks available in track collection: " << nTracksHere << endl;

    // loop over all tracks
    for (int nTracksEvent = 0; nTracksEvent < nTracksHere && nTracksEvent < _maxTrackCandidates; nTracksEvent++) {

      Track *TrackHere = dynamic_cast<Track*> (collection->getElementAt(nTracksEvent));

      // hit list assigned to track

      std::vector<EVENT::TrackerHit*> TrackHitsHere = TrackHere->getTrackerHits();

      // check for a hit in every plane

      if (_nPlanes == int(TrackHitsHere.size() / 2)) {

        // assume hits are ordered in z! start counting from 0
        int nPlaneHere = 0;

        // loop over all hits and fill arrays
        for (int nHits = 0; nHits < int(TrackHitsHere.size()); nHits++) {

          TrackerHit *HitHere = TrackHitsHere.at(nHits);

          // hit positions
          const double *PositionsHere = HitHere->getPosition();

          // assume fitted hits have type 32
          if ( HitHere->getType() == 32 ) {

            // fill hits to arrays
            _xPos[nTracksEvent][nPlaneHere] = PositionsHere[0] * 1000;
            _yPos[nTracksEvent][nPlaneHere] = PositionsHere[1] * 1000;
            _zPos[nTracksEvent][nPlaneHere] = PositionsHere[2] * 1000;

            nPlaneHere++;

          } // end assume fitted hits have type 32

        } // end loop over all hits and fill arrays

        _nTracks++;

      } else {

        streamlog_out ( MILLEMESSAGE ) << "Dropping track " << nTracksEvent << " because there is not a hit in every plane assigned to it." << endl;

      }

    } // end loop over all tracks

  } else if (_inputMode == 3) {
    LCCollection* collection;
    try {
      collection = event->getCollection(_trackCollectionName);
    } catch (DataNotAvailableException& e) {
      streamlog_out ( WARNING2 ) << "No input track collection " << _trackCollectionName  << " found for event " << event->getEventNumber()
                                 << " in run " << event->getRunNumber() << endl;
      throw SkipEventException(this);
    }
    const int nTracksHere = collection->getNumberOfElements();
    
    // loop over all tracks
    for (int nTracksEvent = 0; nTracksEvent < nTracksHere && nTracksEvent < _maxTrackCandidates; nTracksEvent++) {

      Track *TrackHere = dynamic_cast<Track*> (collection->getElementAt(nTracksEvent));

      // hit list assigned to track
      std::vector<EVENT::TrackerHit*> TrackHitsHere = TrackHere->getTrackerHits();

      int number_of_planes = int((TrackHitsHere.size() - _excludePlanes.size() )/ 2);
//    int number_of_planes = int(TrackHitsHere.size() / 2);
      if ( _siPlanesParameters->getSiPlanesType() == _siPlanesParameters->TelescopeWithDUT ) {
        ++number_of_planes;
      }
      // check for a hit in every telescope plane. this needs probably
      // some further investigations. perhaps it fails if some planes
      // were excluded in the track fitter. but it should work
      if ((_nPlanes  - static_cast<int>(_excludePlanes.size()))== number_of_planes)
        {
          for(size_t i =0;i < _hitCollectionName.size();i++)
 //     // check for a hit in every telescope plane
 //     if (_siPlanesParameters->getSiPlanesNumber() == number_of_planes)
          {
              LCCollection* collection;
              try {
                collection = event->getCollection(_hitCollectionName[i]);
              } catch (DataNotAvailableException& e) {
                streamlog_out ( WARNING2 ) << "No input collection " << _hitCollectionName[i] << " found for event " << event->getEventNumber()
                                           << " in run " << event->getRunNumber() << endl;
                throw SkipEventException(this);
              }
              for ( int iHit = 0; iHit < collection->getNumberOfElements(); iHit++ )
                {
                  TrackerHitImpl *hit = static_cast<TrackerHitImpl*> ( collection->getElementAt(iHit) );

                  LCObjectVec clusterVector = hit->getRawHits();
                  EUTelVirtualCluster *cluster;

                  if ( hit->getType() == kEUTelBrickedClusterImpl ) {

                    // fixed cluster implementation. Remember it can come from
                    // both RAW and ZS data
                    cluster = new EUTelBrickedClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                  }
                  else if ( hit->getType() == kEUTelDFFClusterImpl ) {

                    // fixed cluster implementation. Remember it can come from
                    // both RAW and ZS data
                    cluster = new EUTelDFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                  }
                  else if ( hit->getType() == kEUTelFFClusterImpl ) {

                    // fixed cluster implementation. Remember it can come from
                    // both RAW and ZS data
                    cluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                  }
                  else if ( hit->getType() == kEUTelSparseClusterImpl ) {
                    //copy and paste from the inputmode 0
                    // code. needs to be tested ...

                    // ok the cluster is of sparse type, but we also need to know
                    // the kind of pixel description used. This information is
                    // stored in the corresponding original data collection.

                    LCCollectionVec * sparseClusterCollectionVec = dynamic_cast < LCCollectionVec * > (evt->getCollection("original_zsdata"));
                    TrackerDataImpl * oneCluster = dynamic_cast<TrackerDataImpl*> (sparseClusterCollectionVec->getElementAt( 0 ));
                    CellIDDecoder<TrackerDataImpl > anotherDecoder(sparseClusterCollectionVec);
                    SparsePixelType pixelType = static_cast<SparsePixelType> ( static_cast<int> ( anotherDecoder( oneCluster )["sparsePixelType"] ));

                    // now we know the pixel type. So we can properly create a new
                    // instance of the sparse cluster
                    if ( pixelType == kEUTelSimpleSparsePixel ) {

                      cluster = new EUTelSparseClusterImpl< EUTelSimpleSparsePixel >
                        ( static_cast<TrackerDataImpl *> ( clusterVector[ 0 ]  ) );

                    } else {
                      streamlog_out ( ERROR4 ) << "Unknown pixel type.  Sorry for quitting." << endl;
                      throw UnknownDataTypeException("Pixel type unknown");
                    }

                  } else {
                    throw UnknownDataTypeException("Unknown cluster type");
                  }

                  std::vector<EUTelMille::HitsInPlane> hitsplane;

                  hitsplane.push_back(
                          EUTelMille::HitsInPlane(
                              1000 * hit->getPosition()[0],
                              1000 * hit->getPosition()[1],
                              1000 * hit->getPosition()[2]
                              )
                          );

                  double measuredz = hit->getPosition()[2];

                  delete cluster; // <--- destroying the cluster
                  for (int nHits = 0; nHits < int(TrackHitsHere.size()); nHits++)  // end loop over all hits and fill arrays
                    {
                      TrackerHit *HitHere = TrackHitsHere.at(nHits);

                      // hit positions
                      const double *PositionsHere = HitHere->getPosition();

                      //assume that fitted hits have type 32.
                      //the tracker hit will be excluded if the
                      //distance to the hit from the hit collection
                      //is larger than 5 mm. this requirement should reject
                      //reconstructed hits in the DUT in order to
                      //avoid double counting.

//  ADDITIONAL PRINTOUTS   
//                      printf("measZ = %8.3f posHere[2] = %8.3f  type = %5d \n", measuredz, PositionsHere[2], HitHere->getType() );
                      if( std::abs( measuredz - PositionsHere[2] ) > 5.0 /* mm */)
                        {
                          if ( HitHere->getType()  == 32 )
                            {
                              hitsplane.push_back(
                                      EUTelMille::HitsInPlane(
                                          PositionsHere[0] * 1000,
                                          PositionsHere[1] * 1000,
                                          PositionsHere[2] * 1000
                                          )
                                      );
                            } // end assume fitted hits have type 32
                        }
                    }
                  //sort the array such that the hits are ordered
                  //in z assuming that z is constant over all
                  //events for each plane
                  std::sort(hitsplane.begin(), hitsplane.end());
                          
          
                  //now the array is filled into the track
                  //candidates array
                  for(int i = 0; i < _nPlanes; i++)
                    {
                      _xPos[_nTracks][i] = hitsplane[i].measuredX;
                      _yPos[_nTracks][i] = hitsplane[i].measuredY;
                      _zPos[_nTracks][i] = hitsplane[i].measuredZ;
                    }
                  _nTracks++; //and we found an additional track candidate.
                }
            }
          //end of the loop
        } else {

        streamlog_out ( MILLEMESSAGE ) << "Dropping track " << nTracksEvent << " because there is not a hit in every plane assigned to it." << endl;
      }

    } // end loop over all tracks

  }

  if (_nTracks == _maxTrackCandidates) {
    streamlog_out ( WARNING2 ) << "Maximum number of track candidates reached. Maybe further tracks were skipped" << endl;
  }

  streamlog_out ( MILLEMESSAGE ) << "Number of hits in the individual planes: ";
  for(size_t i = 0; i < _hitsArray.size(); i++)
    streamlog_out ( MILLEMESSAGE ) << _hitsArray[i].size() << " ";
  streamlog_out ( MILLEMESSAGE ) << endl;

  streamlog_out ( MILLEMESSAGE ) << "Number of track candidates found: " << _iEvt << ": " << _nTracks << endl;

  // Perform fit for all found track candidates
  // ------------------------------------------

  // only one track or no single track event
  if (_nTracks == 1 || _onlySingleTrackEvents == 0) {

#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
    std::vector<double> lambda;
    double par_c0 = 0.0;
    double par_c1 = 0.0;
    lambda.reserve(_nPlanes-_nExcludePlanes);
    bool validminuittrack = false;
#endif

    double Chiquare[2] = {0,0};
    double angle[2] = {0,0};

    // loop over all track candidates
    for (int track = 0; track < _nTracks; track++) {

      _xPosHere = new double[_nPlanes];
      _yPosHere = new double[_nPlanes];
      _zPosHere = new double[_nPlanes];

      for (int help = 0; help < _nPlanes; help++) {
        _xPosHere[help] = _xPos[track][help];
        _yPosHere[help] = _yPos[track][help];
        _zPosHere[help] = _zPos[track][help];
      }

      Chiquare[0] = 0.0;
      Chiquare[1] = 0.0;

      streamlog_out ( MILLEMESSAGE ) << "Adding track using the following coordinates: ";

      // loop over all planes
      for (int help = 0; help < _nPlanes; help++) {

        int excluded = 0;

        // check if actual plane is excluded
        if (_nExcludePlanes > 0) {
          for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
            if (help == _excludePlanes[helphelp]) {
              excluded = 1;
            }
          }
        }

        if (excluded == 0) {
          streamlog_out ( MILLEMESSAGE ) << _xPosHere[help] << " " << _yPosHere[help] << " " << _zPosHere[help] << "   ";
        }

      } // end loop over all planes

      streamlog_out ( MILLEMESSAGE ) << endl;
      
      if(_alignMode == 3)
        {
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
          //use minuit to find tracks
          //hitsarray.clear();
          //hitsarray.reserve(_nPlanes-_nExcludePlanes);
          double mean_x = 0.0;
          double mean_y = 0.0;
          double mean_z = 0.0;
          int index_hitsarray=0;
          for (int help = 0; help < _nPlanes; help++) {
            bool excluded = false;
            // check if actual plane is excluded
            if (_nExcludePlanes > 0) {
              for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
                if (help == _excludePlanes[helphelp]) {
                  excluded = true;
                }
              }
            }
            if(!excluded)
              {
                const double x = _xPos[track][help];
                const double y = _yPos[track][help];
                const double z = _zPos[track][help];

                mean_z += z;
                mean_x += x;
                mean_y += y;

              //   hitsarray.push_back(hit(
//                                         x, y, z,
//                                         _resolutionX[help],_resolutionY[help], _resolutionZ[help],
//                                         help
//                                         ));

                hitsarray[index_hitsarray] = (hit(
                                                  x, y, z,
                                                  _resolutionX[help],_resolutionY[help], _resolutionZ[help],
                                                  help
                                                  ));
                
                index_hitsarray++;
              }
          }
          mean_z = mean_z / (double)(number_of_datapoints);
          mean_x = mean_x / (double)(number_of_datapoints);
          mean_y = mean_y / (double)(number_of_datapoints);

          static bool firstminuitcall = true;
          
          if(firstminuitcall)
          {
              gSystem->Load("libMinuit");//is this really needed?
              firstminuitcall = false;
          }          
          TMinuit *gMinuit = new TMinuit(4);  //initialize TMinuit with a maximum of 4 params
          
          //  set print level (-1 = quiet, 0 = normal, 1 = verbose)
          gMinuit->SetPrintLevel(-1);
  
          gMinuit->SetFCN(fcn_wrapper);
          
          double arglist[10];
          int ierflg = 0;

   
          // minimization strategy (1 = standard, 2 = slower)
          arglist[0] = 2;
          gMinuit->mnexcm("SET STR",arglist,2,ierflg);
          
          // set error definition (1 = for chi square)
          arglist[0] = 1;
          gMinuit->mnexcm("SET ERR",arglist,1,ierflg);
          
          //analytic track fit to guess the starting parameters
          double sxx = 0.0;
          double syy = 0.0;
          double szz = 0.0;
            
          double szx = 0.0;
          double szy = 0.0;
            
          for(size_t i = 0; i< number_of_datapoints; i++)
            {
              const double x = hitsarray[i].x;
              const double y = hitsarray[i].y;
              const double z = hitsarray[i].z;
                
              sxx += pow(x-mean_x,2);
              syy += pow(y-mean_y,2);
              szz += pow(z-mean_z,2);
                
              szx += (x-mean_x)*(z-mean_z);
              szy += (y-mean_y)*(z-mean_z);
            }
          double linfit_x_a1 = szx/szz; //slope
          double linfit_y_a1 = szy/szz; //slope
            
          double linfit_x_a0 = mean_x - linfit_x_a1 * mean_z; //offset
          double linfit_y_a0 = mean_y - linfit_y_a1 * mean_z; //offset
            
          double del= -1.0*atan(linfit_y_a1);//guess of delta
          double ps = atan(linfit_x_a1/sqrt(1.0+linfit_y_a1*linfit_y_a1));//guess
                                                                          //of psi
            
          //  Set starting values and step sizes for parameters
          Double_t vstart[4] = {linfit_x_a0, linfit_y_a0, del, ps};
          //duble vstart[4] = {0.0, 0.0, 0.0, 0.0};
          double step[4] = {0.01, 0.01, 0.01, 0.01};
            
          gMinuit->mnparm(0, "b0", vstart[0], step[0], 0,0,ierflg);
          gMinuit->mnparm(1, "b1", vstart[1], step[1], 0,0,ierflg);
          gMinuit->mnparm(2, "delta", vstart[2], step[2], -1.0*TMath::Pi(), 1.0*TMath::Pi(),ierflg);
          gMinuit->mnparm(3, "psi", vstart[3], step[3], -1.0*TMath::Pi(), 1.0*TMath::Pi(),ierflg);
            
          //   gMinuit->FixParameter(2);
          //             gMinuit->FixParameter(3);
            

          //  Now ready for minimization step
          arglist[0] = 2000;
          arglist[1] = 0.01;
          gMinuit->mnexcm("MIGRAD", arglist ,1,ierflg);
            
          //gMinuit->Release(2);
          //gMinuit->Release(3);
            
          ////  Now ready for minimization step
          //arglist[0] = 8000;
          //arglist[1] = 1.0;
          //gMinuit->mnexcm("MIGRAD", arglist ,1,ierflg);
            
          bool ok = true;
            
          if(ierflg != 0)
            {
              ok = false;
            
            }
                    
      //     //    calculate errors using MINOS. do we need this?
//           arglist[0] = 2000;
//           arglist[1] = 0.1;
//           gMinuit->mnexcm("MINOS",arglist,1,ierflg);
          
          //   get results from migrad
          double b0 = 0.0;
          double b1 = 0.0;
          double delta = 0.0;
          double psi = 0.0;
          double b0_error = 0.0;
          double b1_error = 0.0;
          double delta_error = 0.0;
          double psi_error = 0.0;
          
          gMinuit->GetParameter(0,b0,b0_error);
          gMinuit->GetParameter(1,b1,b1_error);
          gMinuit->GetParameter(2,delta,delta_error);
          gMinuit->GetParameter(3,psi,psi_error);
          
          double c0 = 1.0;
          double c1 = 1.0;
          double c2 = 1.0;
          if(ok)
            {
              c0 = TMath::Sin(psi);
              c1 = -1.0*TMath::Cos(psi) * TMath::Sin(delta);
              c2 = TMath::Cos(delta) * TMath::Cos(psi);
              par_c0 = c0;
              par_c1 = c1;
              validminuittrack = true;
              
              for (int help =0; help < _nPlanes; help++)
                {
                  const double x = _xPos[track][help];
                  const double y = _yPos[track][help];
                  const double z = _zPos[track][help];
                  
                  //calculate the lambda parameter
                  const double la = -1.0*b0*c0-b1*c1+c0*x+c1*y+sqrt(1-c0*c0-c1*c1)*z;
                  lambda.push_back(la);
                  
                  //determine the residuals
                  _waferResidX[help] = b0 + la*c0 - x;
                  _waferResidY[help] = b1 + la*c1 - y;
                  _waferResidZ[help] = la*sqrt(1.0 - c0*c0 - c1*c1) - z;

                }
            }
          delete gMinuit;
#endif
        }
      else
        {
          // Calculate residuals
          FitTrack(int(_nPlanes),
                   _xPosHere,
                   _yPosHere,
                   _zPosHere,
                   _telescopeResolX,
                   _telescopeResolY,
                   Chiquare,
                   _waferResidX,
                   _waferResidY,
                   angle);
        }
      streamlog_out ( MILLEMESSAGE ) << "Residuals X: ";

      for (int help = 0; help < _nPlanes; help++) {
        streamlog_out ( MILLEMESSAGE ) << _waferResidX[help] << " ";
      }

      streamlog_out ( MILLEMESSAGE ) << endl;

      streamlog_out ( MILLEMESSAGE ) << "Residuals Y: ";

      for (int help = 0; help < _nPlanes; help++) {
        streamlog_out ( MILLEMESSAGE ) << _waferResidY[help] << " ";
      }

      streamlog_out ( MILLEMESSAGE ) << endl;

      int residualsXOkay = 1;
      int residualsYOkay = 1;

      // check if residal cuts are used
      if (_useResidualCuts != 0) {

        // loop over all sensors
        for (int help = 0; help < _nPlanes; help++) {
          int excluded = 0; //0 not excluded, 1 excluded
          if (_nExcludePlanes > 0) {
            for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
              if (help == _excludePlanes[helphelp]) {
                excluded = 1;
              }
            }
          }
          if(excluded == 0)
            {
              if (_waferResidX[help] < _residualsXMin[help] || _waferResidX[help] > _residualsXMax[help]) {
                residualsXOkay = 0;
              }
              if (_waferResidY[help] < _residualsYMin[help] || _waferResidY[help] > _residualsYMax[help]) {
                residualsYOkay = 0;
              }
            }
// 
//          if (_waferResidX[help] < _residualsXMin[help] || _waferResidX[help] > _residualsXMax[help]) {
//            residualsXOkay = 0;
//          }
//          if (_waferResidY[help] < _residualsYMin[help] || _waferResidY[help] > _residualsYMax[help]) {
//            residualsYOkay = 0;
//          }

        } // end loop over all sensors

      } // end check if residual cuts are used

      if (_useResidualCuts != 0 && (residualsXOkay == 0 || residualsYOkay == 0)) {
        streamlog_out ( MILLEMESSAGE ) << "Track did not pass the residual cuts." << endl;
      }

      // apply track cuts (at the moment only residuals)
      if (_useResidualCuts == 0 || (residualsXOkay == 1 && residualsYOkay == 1)) {

        // Add track to Millepede
        // ---------------------------

        // Easy case: consider only shifts
        if (_alignMode == 2) {

          const int nLC = 4; // number of local parameters
          const int nGL = (_nPlanes - _nExcludePlanes) * 2; // number of global parameters

          float sigma = _telescopeResolution;

          float *derLC = new float[nLC]; // array of derivatives for local parameters
          float *derGL = new float[nGL]; // array of derivatives for global parameters

          int *label = new int[nGL]; // array of labels

          float residual;

          // create labels
          for (int help = 0; help < nGL; help++) {
            label[help] = help + 1;
          }

          for (int help = 0; help < nGL; help++) {
            derGL[help] = 0;
          }

          for (int help = 0; help < nLC; help++) {
            derLC[help] = 0;
          }

          int nExcluded = 0;

          // loop over all planes
          for (int help = 0; help < _nPlanes; help++) {

            int excluded = 0;

            // check if actual plane is excluded
            if (_nExcludePlanes > 0) {
              for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
                if (help == _excludePlanes[helphelp]) {
                  excluded = 1;
                  nExcluded++;
                }
              }
            }

            // if plane is not excluded
            if (excluded == 0) {

              int helphelp = help - nExcluded; // index of plane after
                                               // excluded planes have
                                               // been removed

              derGL[((helphelp * 2) + 0)] = -1;
              derLC[0] = 1;
              derLC[2] = _zPosHere[help];
              residual = _waferResidX[help];

              _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

              derGL[((helphelp * 2) + 0)] = 0;
              derLC[0] = 0;
              derLC[2] = 0;

              derGL[((helphelp * 2) + 1)] = -1;
              derLC[1] = 1;
              derLC[3] = _zPosHere[help];
              residual = _waferResidY[help];

              _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

              derGL[((helphelp * 2) + 1)] = 0;
              derLC[1] = 0;
              derLC[3] = 0;

              _nMilleDataPoints++;

            } // end if plane is not excluded

          } // end loop over all planes

          // clean up

          delete [] derLC;
          delete [] derGL;
          delete [] label;

          // Slightly more complicated: add rotation around the z axis
        } else if (_alignMode == 1) {

          const int nLC = 4; // number of local parameters
          const int nGL = _nPlanes * 3; // number of global parameters

          float sigma = _telescopeResolution;

          float *derLC = new float[nLC]; // array of derivatives for local parameters
          float *derGL = new float[nGL]; // array of derivatives for global parameters

          int *label = new int[nGL]; // array of labels

          float residual;

          // create labels
          for (int help = 0; help < nGL; help++) {
            label[help] = help + 1;
          }

          for (int help = 0; help < nGL; help++) {
            derGL[help] = 0;
          }

          for (int help = 0; help < nLC; help++) {
            derLC[help] = 0;
          }

          int nExcluded = 0;

          // loop over all planes
          for (int help = 0; help < _nPlanes; help++) {

            int excluded = 0;

            // check if actual plane is excluded
            if (_nExcludePlanes > 0) {
              for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
                if (help == _excludePlanes[helphelp]) {
                  excluded = 1;
                  nExcluded++;
                }
              }
            }

            // if plane is not excluded
            if (excluded == 0) {

              int helphelp = help - nExcluded; // index of plane after
                                               // excluded planes have
                                               // been removed

              derGL[((helphelp * 3) + 0)] = -1;
              derGL[((helphelp * 3) + 2)] = _yPosHere[help];
              derLC[0] = 1;
              derLC[2] = _zPosHere[help];
              residual = _waferResidX[help];

              _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

              derGL[((helphelp * 3) + 0)] = 0;
              derGL[((helphelp * 3) + 2)] = 0;
              derLC[0] = 0;
              derLC[2] = 0;

              derGL[((helphelp * 3) + 1)] = -1;
              derGL[((helphelp * 3) + 2)] = -1 * _xPosHere[help];
              derLC[1] = 1;
              derLC[3] = _zPosHere[help];
              residual = _waferResidY[help];

              _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

              derGL[((helphelp * 3) + 1)] = 0;
              derGL[((helphelp * 3) + 2)] = 0;
              derLC[1] = 0;
              derLC[3] = 0;

              _nMilleDataPoints++;

            } // end if plane is not excluded

          } // end loop over all planes

          // clean up

          delete [] derLC;
          delete [] derGL;
          delete [] label;

        } else if (_alignMode == 3) {
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
          if(validminuittrack)
            {
              const int nLC = 4; // number of local parameters
              const int nGL = _nPlanes * 6; // number of global parameters

              float sigma = _telescopeResolution;

              float *derLC = new float[nLC]; // array of derivatives for local parameters
              float *derGL = new float[nGL]; // array of derivatives for global parameters

              int *label = new int[nGL]; // array of labels

              float residual;

              // create labels
              for (int help = 0; help < nGL; help++) {
                label[help] = help + 1;
              }

              for (int help = 0; help < nGL; help++) {
                derGL[help] = 0;
              }

              for (int help = 0; help < nLC; help++) {
                derLC[help] = 0;
              }

              int nExcluded = 0;

              // loop over all planes
             
              for (int help = 0; help < _nPlanes; help++) {

                int excluded = 0;

                // check if actual plane is excluded
                if (_nExcludePlanes > 0) {
                  for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
                    if (help == _excludePlanes[helphelp]) {
                      excluded = 1;
                      nExcluded++;
                    }
                  }
                }

                // if plane is not excluded
//                 if (excluded == 0) {
//                   cout << "--" << endl;
//                   int helphelp = help - nExcluded; // index of plane after
//                   // excluded planes have
//                   // been removed

//                   //local parameters: b0, b1, c0, c1

//                   const double la = lambda[help];
              
//                   derGL[((helphelp * 6) + 0)] = 1.0;
//                   derGL[((helphelp * 6) + 1)] = 0.0;
//                   derGL[((helphelp * 6) + 2)] = 0.0;
//                   derGL[((helphelp * 6) + 3)] = 0.0;
//                   derGL[((helphelp * 6) + 4)] = -1.0*_zPosHere[help];
//                   derGL[((helphelp * 6) + 5)] = _yPosHere[help];

//                   derLC[0] = 1.0;
//                   derLC[1] = 0.0;
//                   derLC[2] = la;
//                   derLC[3] = 0.0;
            
//                   residual = _waferResidX[help];
//                   sigma = _resolutionX[help];
//                   for(int i =0; i<4;i++)
//                     cout << "a " << derLC[i] << endl;
                 
//                   _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

             
//                   derGL[((helphelp * 6) + 0)] = 0.0;
//                   derGL[((helphelp * 6) + 1)] = 1.0;
//                   derGL[((helphelp * 6) + 2)] = 0.0;
//                   derGL[((helphelp * 6) + 3)] = _zPosHere[help];
//                   derGL[((helphelp * 6) + 4)] = _zPosHere[help];
//                   derGL[((helphelp * 6) + 5)] = -1.0*_xPosHere[help];

//                   derLC[0] = 0.0;
//                   derLC[1] = 1.0;
//                   derLC[2] = 0.0;
//                   derLC[3] = 1.0;
            
//                   residual = _waferResidY[help];
//                   sigma = _resolutionY[help];
//                   for(int i =0; i<4;i++)
//                     cout << "b " << derLC[i] << endl;
                  
//                   _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);
              
              
//                   derGL[((helphelp * 6) + 0)] = 0.0;
//                   derGL[((helphelp * 6) + 1)] = 0.0;
//                   derGL[((helphelp * 6) + 2)] = 1.0;
//                   derGL[((helphelp * 6) + 3)] = -1.0*_yPosHere[help];
//                   derGL[((helphelp * 6) + 4)] = _xPosHere[help];
//                   derGL[((helphelp * 6) + 5)] = 0.0;

//                   derLC[0] = 0.0;
//                   derLC[1] = 0.0;
//                   derLC[2] = -1.0*la*par_c0/sqrt(1.0-par_c0*par_c0-par_c1*par_c1);
//                   derLC[3] = -1.0*la*par_c1/sqrt(1.0-par_c0*par_c0-par_c1*par_c1);
            
//                   residual = _waferResidZ[help];
//                   sigma = _resolutionZ[help];
//                   for(int i =0; i<4;i++)
//                     cout << "c " << derLC[i] << endl;
                 
//                   _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);


//                   _nMilleDataPoints++;

//                 } // end if plane is not excluded

                if (excluded == 0) {
                  //     cout << "--" << endl;
                  int helphelp = help - nExcluded; // index of plane after
                  // excluded planes have
                  // been removed

                  //local parameters: b0, b1, c0, c1

                  const double la = lambda[help];
              
                  derGL[((helphelp * 6) + 0)] = -1.0;
                  derGL[((helphelp * 6) + 1)] = 0.0;
                  derGL[((helphelp * 6) + 2)] = 0.0;
                  derGL[((helphelp * 6) + 3)] = 0.0;
                  derGL[((helphelp * 6) + 4)] = -1.0*_zPosHere[help];
                  derGL[((helphelp * 6) + 5)] = _yPosHere[help];

                  derLC[0] = 1.0;
                  derLC[1] = 0.0;
                  derLC[2] = _zPosHere[help];
                  derLC[3] = 0.0;
            
                  residual = _waferResidX[help];
                  sigma = _resolutionX[help];
               //    for(int i =0; i<4;i++)
//                     cout << "a " << derLC[i] << endl;
                 
                  _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

             
                  derGL[((helphelp * 6) + 0)] = 0.0;
                  derGL[((helphelp * 6) + 1)] = -1.0;
                  derGL[((helphelp * 6) + 2)] = 0.0;
                  derGL[((helphelp * 6) + 3)] = _zPosHere[help];
                  derGL[((helphelp * 6) + 4)] = _zPosHere[help];
                  derGL[((helphelp * 6) + 5)] = -1.0*_xPosHere[help];

                  derLC[0] = 0.0;
                  derLC[1] = 1.0;
                  derLC[2] = 0.0;
                  derLC[3] = _zPosHere[help];
            
                  residual = _waferResidY[help];
                  sigma = _resolutionY[help];
               //    for(int i =0; i<4;i++)
//                     cout << "b " << derLC[i] << endl;
                  
                  _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);
              
              
                  derGL[((helphelp * 6) + 0)] = 0.0;
                  derGL[((helphelp * 6) + 1)] = 0.0;
                  derGL[((helphelp * 6) + 2)] = -1.0;
                  derGL[((helphelp * 6) + 3)] = -1.0*_yPosHere[help];
                  derGL[((helphelp * 6) + 4)] = _xPosHere[help];
                  derGL[((helphelp * 6) + 5)] = 0.0;

                  derLC[0] = 0.0;
                  derLC[1] = 0.0;
                  derLC[2] = _xPosHere[help];
                  derLC[3] = _yPosHere[help];
            
                  residual = _waferResidZ[help];
                  sigma = _resolutionZ[help];
               //    for(int i =0; i<4;i++)
//                     cout << "c " << derLC[i] << endl;
                 
                  _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);


                  _nMilleDataPoints++;

                } // end if plane is not excluded



              } // end loop over all planes

              // clean up

              delete [] derLC;
              delete [] derGL;
              delete [] label;
            }
#endif
        } else {

          streamlog_out ( ERROR2 ) << _alignMode << " is not a valid mode. Please choose 1,2 or 3." << endl;

        }

        _nGoodTracks++;

        // end local fit
        _mille->end();

        _nMilleTracks++;

        // Fill histograms for individual tracks
        // -------------------------------------

#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)

        string tempHistoName;

        if ( _histogramSwitch ) {
          if ( AIDA::IHistogram1D* chi2x_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[_chi2XLocalname]) )
            chi2x_histo->fill(Chiquare[0]);
          else {
            streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _chi2XLocalname << endl;
            streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
            _histogramSwitch = false;
          }
        }

        if ( _histogramSwitch ) {
          if ( AIDA::IHistogram1D* chi2y_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[_chi2YLocalname]) )
            chi2y_histo->fill(Chiquare[1]);
          else {
            streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _chi2YLocalname << endl;
            streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
            _histogramSwitch = false;
          }
        }

        // loop over all detector planes
        for( int iDetector = 0; iDetector < _nPlanes; iDetector++ ) {

          int sensorID = _orderedSensorID.at( iDetector );

          if ( _histogramSwitch ) {
            tempHistoName = _residualXLocalname + "_d" + to_string( sensorID );
            if ( AIDA::IHistogram1D* residx_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
              {
                residx_histo->fill(_waferResidX[iDetector]);
              }
            else {
              streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualXLocalname << endl;
              streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
              _histogramSwitch = false;
            }
          }

          if ( _histogramSwitch ) {
            tempHistoName = _residualYLocalname + "_d" + to_string( sensorID );
            if ( AIDA::IHistogram1D* residy_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
              residy_histo->fill(_waferResidY[iDetector]);
            else {
              streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualYLocalname << endl;
              streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
              _histogramSwitch = false;
            }
          }
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
          if ( _histogramSwitch ) {
            tempHistoName = _residualZLocalname + "_d" + to_string( sensorID );
            if ( AIDA::IHistogram1D* residz_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
              residz_histo->fill(_waferResidZ[iDetector]);
            else {
              streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualZLocalname << endl;
              streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
              _histogramSwitch = false;
            }
          }

#endif

        } // end loop over all detector planes

#endif

      } // end if apply track cuts

      // clean up
      delete [] _zPosHere;
      delete [] _yPosHere;
      delete [] _xPosHere;

    } // end loop over all track candidates

  } // end if only one track or no single track event

  streamlog_out ( MILLEMESSAGE ) << "Finished fitting tracks in event " << _iEvt << endl;

#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)

  string tempHistoName;

  if ( _histogramSwitch ) {
    {
      stringstream ss;
      ss << _numberTracksLocalname << endl;
    }
    if ( AIDA::IHistogram1D* number_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[_numberTracksLocalname]) )
      number_histo->fill(_nGoodTracks);
    else {
      streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _numberTracksLocalname << endl;
      streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
      _histogramSwitch = false;
    }
  }

#endif

  // count events
  ++_iEvt;
  if ( isFirstEvent() ) _isFirstEvent = false;

}

void EUTelMille::end() {

  delete [] _telescopeResolY;
  delete [] _telescopeResolX;
  delete [] _telescopeResolZ;
  delete [] _yFitPos;
  delete [] _xFitPos;
  delete [] _waferResidY;
  delete [] _waferResidX;
  delete [] _waferResidZ;
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
  if(_alignMode == 3)
    {
      delete []  hitsarray;
    }
#endif

  // close the output file
  delete _mille;

  // if write the pede steering file
  if (_generatePedeSteerfile) {

    streamlog_out ( MESSAGE4 ) << endl << "Generating the steering file for the pede program..." << endl;

    string tempHistoName;
    double *meanX = new double[_nPlanes];
    double *meanY = new double[_nPlanes];
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
    double *meanZ = new double[_nPlanes];
#endif
    // loop over all detector planes
    for( int iDetector = 0; iDetector < _nPlanes; iDetector++ ) {

      int sensorID = _orderedSensorID.at( iDetector );

      if ( _histogramSwitch ) {
        tempHistoName =  _residualXLocalname + "_d" + to_string( sensorID );
        if ( AIDA::IHistogram1D* residx_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
          meanX[iDetector] = residx_histo->mean();
        else {
          streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualXLocalname << endl;
          streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
          _histogramSwitch = false;
        }
      }

      if ( _histogramSwitch ) {
        tempHistoName =  _residualYLocalname + "_d" + to_string( sensorID );
        if ( AIDA::IHistogram1D* residy_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
          meanY[iDetector] = residy_histo->mean();
        else {
          streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualYLocalname << endl;
          streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
          _histogramSwitch = false;
        }
      }
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
      if ( _histogramSwitch ) {
        tempHistoName =  _residualZLocalname + "_d" + to_string( sensorID );
        if ( AIDA::IHistogram1D* residz_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
          meanZ[iDetector] = residz_histo->mean();
        else {
          streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualZLocalname << endl;
          streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
          _histogramSwitch = false;
        }
      }
#endif
    } // end loop over all detector planes

    ofstream steerFile;
    steerFile.open(_pedeSteerfileName.c_str());

    if (steerFile.is_open()) {

      // find first and last excluded plane
      int firstnotexcl = _nPlanes;
      int lastnotexcl = 0;

      // loop over all planes
      for (int help = 0; help < _nPlanes; help++) {

        int excluded = 0;

        // loop over all excluded planes
        for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
          if (help == _excludePlanes[helphelp]) {
            excluded = 1;
          }
        } // end loop over all excluded planes

        if (excluded == 0 && firstnotexcl > help) {
          firstnotexcl = help;
        }

        if (excluded == 0 && lastnotexcl < help) {
          lastnotexcl = help;
        }
      } // end loop over all planes

      // calculate average
      double averageX = (meanX[firstnotexcl] + meanX[lastnotexcl]) / 2;
      double averageY = (meanY[firstnotexcl] + meanY[lastnotexcl]) / 2;
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
      double averageZ = (meanZ[firstnotexcl] + meanZ[lastnotexcl]) / 2;
#endif
      steerFile << "Cfiles" << endl;
      steerFile << _binaryFilename << endl;
      steerFile << endl;

      steerFile << "Parameter" << endl;

      int counter = 0;

      // loop over all planes
      for (int help = 0; help < _nPlanes; help++) {

        int excluded = 0; // flag for excluded planes

        // loop over all excluded planes
        for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {

          if (help == _excludePlanes[helphelp]) {
            excluded = 1;
          }

        } // end loop over all excluded planes

        // if plane not excluded
        if (excluded == 0) {
          
          bool fixed = false;
          for(size_t i = 0;i< _FixedPlanes.size(); i++)
            {
              if(_FixedPlanes[i] == help)
                fixed = true;
            }
          
          // if fixed planes
          // if (help == firstnotexcl || help == lastnotexcl) {
          if( fixed || (_FixedPlanes.size() == 0 && (help == firstnotexcl || help == lastnotexcl) ) )
            {
              if (_alignMode == 1) {
                steerFile << (counter * 3 + 1) << " 0.0 -1.0" << endl;
                steerFile << (counter * 3 + 2) << " 0.0 -1.0" << endl;
                steerFile << (counter * 3 + 3) << " 0.0 -1.0" << endl;
              } else if (_alignMode == 2) {
                steerFile << (counter * 2 + 1) << " 0.0 -1.0" << endl;
                steerFile << (counter * 2 + 2) << " 0.0 -1.0" << endl;
              } else if (_alignMode == 3) {
                steerFile << (counter * 6 + 1) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 2) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 3) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 4) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 5) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 6) << " 0.0 -1.0" << endl;
              }
              
            } else {
            
            if (_alignMode == 1) {

              if (_usePedeUserStartValues == 0) {
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
                steerFile << (counter * 3 + 1) << " " << (averageX - meanX[help]) << " 0.0" << endl;
                steerFile << (counter * 3 + 2) << " " << (averageY - meanY[help]) << " 0.0" << endl;
                steerFile << (counter * 3 + 3) << " " << (averageZ - meanZ[help]) << " 0.0" << endl;
#endif
              } else {
                steerFile << (counter * 3 + 1) << " " << _pedeUserStartValuesX[help] << " 0.0" << endl;
                steerFile << (counter * 3 + 2) << " " << _pedeUserStartValuesY[help] << " 0.0" << endl;
                steerFile << (counter * 3 + 3) << " " << _pedeUserStartValuesGamma[help] << " 0.0" << endl;
              }

            } else if (_alignMode == 2) {

              if (_usePedeUserStartValues == 0) {
                steerFile << (counter * 2 + 1) << " " << (averageX - meanX[help]) << " 0.0" << endl;
                steerFile << (counter * 2 + 2) << " " << (averageY - meanY[help]) << " 0.0" << endl;
              } else {
                steerFile << (counter * 2 + 1) << " " << _pedeUserStartValuesX[help] << " 0.0" << endl;
                steerFile << (counter * 2 + 2) << " " << _pedeUserStartValuesY[help] << " 0.0" << endl;
              }

            } else if (_alignMode == 3) {
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
              if (_usePedeUserStartValues == 0)
                {
                  if(_FixParameter[help] & (1 << 0))
                    steerFile << (counter * 6 + 1) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 1) << " " << (averageX - meanX[help]) << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 1))
                    steerFile << (counter * 6 + 2) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 2) << " " << (averageY - meanY[help]) << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 2))
                    steerFile << (counter * 6 + 3) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 3) << " " << (averageZ - meanZ[help]) << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 3))
                    steerFile << (counter * 6 + 4) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 4) << " 0.0 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 4))
                    steerFile << (counter * 6 + 5) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 5) << " 0.0 0.0" << endl;

                  if(_FixParameter[help] & (1 << 5))
                    steerFile << (counter * 6 + 6) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 6) << " 0.0 0.0" << endl;
                }
              else
                {
                  if(_FixParameter[help] & (1 << 0))
                    steerFile << (counter * 6 + 1) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 1) << " " << _pedeUserStartValuesX[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 1))
                    steerFile << (counter * 6 + 2) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 2) << " " << _pedeUserStartValuesY[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 2))
                    steerFile << (counter * 6 + 3) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 3) << " " << _pedeUserStartValuesZ[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 3))
                    steerFile << (counter * 6 + 4) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 4) << " " << _pedeUserStartValuesAlpha[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 4))
                    steerFile << (counter * 6 + 5) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 5) << " " << _pedeUserStartValuesBeta[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 5))
                    steerFile << (counter * 6 + 6) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 6) << " " << _pedeUserStartValuesGamma[help] << " 0.0" << endl;
                }
#endif
            }
          }

          counter++;

        } // end if plane not excluded

      } // end loop over all planes

      steerFile << endl;
      steerFile << "! chiscut 5.0 2.5" << endl;
      steerFile << "! outlierdownweighting 4" << endl;
      steerFile << endl;
      steerFile << "method inversion 10 0.001" << endl;
      steerFile << endl;
      steerFile << "histprint" << endl;
      steerFile << endl;
      steerFile << "end" << endl;

      steerFile.close();

      streamlog_out ( MESSAGE2 ) << "File " << _pedeSteerfileName << " written." << endl;

    } else {

      streamlog_out ( ERROR2 ) << "Could not open steering file." << endl;

    }
    //cleaning up
    delete [] meanX;
    delete [] meanY;
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
    delete [] meanZ;
#endif
  } // end if write the pede steering file

  streamlog_out ( MESSAGE2 ) << endl;
  streamlog_out ( MESSAGE2 ) << "Number of data points used: " << _nMilleDataPoints << endl;
  streamlog_out ( MESSAGE2 ) << "Number of tracks used: " << _nMilleTracks << endl;

  // if running pede using the generated steering file
  if (_runPede == 1) {

    // check if steering file exists
    if (_generatePedeSteerfile == 1) {

      std::string command = "pede " + _pedeSteerfileName;

      // before starting pede, let's check if it is in the path
      bool isPedeInPath = true;

      // create a new process
      redi::ipstream which("which pede");

      // wait for the process to finish
      which.close();

      // get the status
      // if it 255 then the program wasn't found in the path
      isPedeInPath = !( which.rdbuf()->status() == 255 );

      if ( !isPedeInPath ) {
        streamlog_out( ERROR ) << "Pede cannot be executed because not found in the path" << endl;
      } else {

        streamlog_out ( MESSAGE2 ) << endl;
        streamlog_out ( MESSAGE2 ) << "Starting pede..." << endl;

        redi::ipstream pede( command.c_str() );
        string output;
        while ( getline( pede, output ) ) {
          streamlog_out( MESSAGE2 ) << output << endl;
        }

        // wait for the pede execution to finish
        pede.close();

        // check the exit value of pede
        if ( pede.rdbuf()->status() == 0 ) {
          streamlog_out ( MESSAGE2 ) << "Pede successfully finished" << endl;
        }

        // reading back the millepede.res file and getting the
        // results.
        string millepedeResFileName = "millepede.res";

        streamlog_out ( MESSAGE2 ) << "Reading back the " << millepedeResFileName << endl
                                   << "Saving the alignment constant into " << _alignmentConstantLCIOFile << endl;

        // open the millepede ASCII output file
        ifstream millepede( millepedeResFileName.c_str() );

        // reopen the LCIO file this time in append mode
        LCWriter * lcWriter = LCFactory::getInstance()->createLCWriter();

        try {
          lcWriter->open( _alignmentConstantLCIOFile, LCIO::WRITE_NEW );
        } catch ( IOException& e ) {
          streamlog_out ( ERROR4 ) << e.what() << endl
                                   << "Sorry for quitting. " << endl;
          exit(-1);
        }

        // write an almost empty run header
        LCRunHeaderImpl * lcHeader  = new LCRunHeaderImpl;
        lcHeader->setRunNumber( 0 );


        lcWriter->writeRunHeader(lcHeader);

        delete lcHeader;

        LCEventImpl * event = new LCEventImpl;
        event->setRunNumber( 0 );
        event->setEventNumber( 0 );

        LCTime * now = new LCTime;
        event->setTimeStamp( now->timeStamp() );
        delete now;

        LCCollectionVec * constantsCollection = new LCCollectionVec( LCIO::LCGENERICOBJECT );


        if ( millepede.bad() ) {
          streamlog_out ( ERROR4 ) << "Error opening the " << millepedeResFileName << endl
                                   << "The alignment slcio file cannot be saved" << endl;
        } else {
          vector<double > tokens;
          stringstream tokenizer;
          string line;
          double buffer;

          // get the first line and throw it away since it is a
          // comment!
          getline( millepede, line );

          int counter = 0;

          while ( ! millepede.eof() ) {

            EUTelAlignmentConstant * constant = new EUTelAlignmentConstant;

            bool goodLine = true;
            unsigned int numpars;
            if(_alignMode != 3)
              numpars = 3;
            else
              numpars = 6;
            for ( unsigned int iParam = 0 ; iParam < numpars ; ++iParam ) {
              getline( millepede, line );

              if ( line.empty() ) {
                goodLine = false;
              }

              tokens.clear();
              tokenizer.clear();
              tokenizer.str( line );

              while ( tokenizer >> buffer ) {
                tokens.push_back( buffer ) ;
              }

              if ( ( tokens.size() == 3 ) || ( tokens.size() == 6 ) || (tokens.size() == 5) ) {
                goodLine = true;
              } else goodLine = false;

              bool isFixed = ( tokens.size() == 3 );
              if ( isFixed ) {
                streamlog_out ( DEBUG0 ) << "Parameter " << tokens[0] << " is at " << ( tokens[1] / 1000 )
                                         << " (fixed)"  << endl;
              } else {
                streamlog_out ( DEBUG0 ) << "Parameter " << tokens[0] << " is at " << (tokens[1] / 1000 )
                                         << " +/- " << ( tokens[4] / 1000 )  << endl;
              }
              if(_alignMode != 3)
                {
                  if ( iParam == 0 ) {
                    constant->setXOffset( tokens[1] / 1000 );
                    if ( ! isFixed ) constant->setXOffsetError( tokens[4] / 1000 ) ;
                  }
                  if ( iParam == 1 ) {
                    constant->setYOffset( tokens[1] / 1000 ) ;
                    if ( ! isFixed ) constant->setYOffsetError( tokens[4] / 1000 ) ;
                  }
                  if ( iParam == 2 ) {
                    constant->setGamma( tokens[1]  ) ;
                    if ( ! isFixed ) constant->setGammaError( tokens[4] ) ;
                  }
                }
              else
                {
                  if ( iParam == 0 ) {
                    constant->setXOffset( -1.0*tokens[1] / 1000 );
                    if ( ! isFixed ) constant->setXOffsetError( tokens[4] / 1000 ) ;                    
                  }
                  if ( iParam == 1 ) {
                    constant->setYOffset(  -1.0*tokens[1] / 1000 ) ;
                    if ( ! isFixed ) constant->setYOffsetError( tokens[4] / 1000 ) ;
                  }
                  if ( iParam == 2 ) {
                    constant->setZOffset(  -1.0*tokens[1] / 1000 ) ;
                    if ( ! isFixed ) constant->setZOffsetError( tokens[4] / 1000 ) ;
                  }
                  if ( iParam == 3 ) {
                    constant->setAlpha( tokens[1]  ) ;
                    if ( ! isFixed ) constant->setAlphaError( tokens[4] ) ;
                  } 
                  if ( iParam == 4 ) {
                    constant->setBeta( tokens[1]  ) ;
                    if ( ! isFixed ) constant->setBetaError( tokens[4] ) ;
                  } 
                  if ( iParam == 5 ) {
                    constant->setGamma( tokens[1]  ) ;
                    if ( ! isFixed ) constant->setGammaError( tokens[4] ) ;
                  } 

                }
              
            }


            // right place to add the constant to the collection
            if ( goodLine ) {
              constant->setSensorID( _orderedSensorID_wo_excluded.at( counter ) );
              ++ counter;
              constantsCollection->push_back( constant );
              streamlog_out ( MESSAGE0 ) << (*constant) << endl;
            }
            else delete constant;
          }

        }
        event->addCollection( constantsCollection, _alignmentConstantCollectionName );
        lcWriter->writeEvent( event );
        delete event;

        lcWriter->close();

        millepede.close();

      }
    } else {

      streamlog_out ( ERROR2 ) << "Unable to run pede. No steering file has been generated." << endl;

    }


  } // end if running pede using the generated steering file

  streamlog_out ( MESSAGE2 ) << endl;
  streamlog_out ( MESSAGE2 ) << "Successfully finished" << endl;
}

void EUTelMille::bookHistos() {


#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)

  try {
    streamlog_out ( MESSAGE2 ) << "Booking histograms..." << endl;

    const int    tracksNBin = 20  ;
    const double tracksMin  = -0.5;
    const double tracksMax  = 19.5;
    const int    Chi2NBin = 100 ;
    const double Chi2Min  =   0.;
    const double Chi2Max  = 100.;
    const int    NBin =  30000;
    const double Min  = -15000.;
    const double Max  =  15000.;

    AIDA::IHistogram1D * numberTracksLocal =
      AIDAProcessor::histogramFactory(this)->createHistogram1D(_numberTracksLocalname,tracksNBin,tracksMin,tracksMax);
    if ( numberTracksLocal ) {
      numberTracksLocal->setTitle("Number of tracks after #chi^{2} cut");
      _aidaHistoMap.insert( make_pair( _numberTracksLocalname, numberTracksLocal ) );
    } else {
      streamlog_out ( ERROR2 ) << "Problem booking the " << (_numberTracksLocalname) << endl;
      streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
      _histogramSwitch = false;
    }

    AIDA::IHistogram1D * chi2XLocal =
      AIDAProcessor::histogramFactory(this)->createHistogram1D(_chi2XLocalname,Chi2NBin,Chi2Min,Chi2Max);
    if ( chi2XLocal ) {
      chi2XLocal->setTitle("Chi2 X");
      _aidaHistoMap.insert( make_pair( _chi2XLocalname, chi2XLocal ) );
    } else {
      streamlog_out ( ERROR2 ) << "Problem booking the " << (_chi2XLocalname) << endl;
      streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
      _histogramSwitch = false;
    }

    AIDA::IHistogram1D * chi2YLocal =
      AIDAProcessor::histogramFactory(this)->createHistogram1D(_chi2YLocalname,Chi2NBin,Chi2Min,Chi2Max);
    if ( chi2YLocal ) {
      chi2YLocal->setTitle("Chi2 Y");
      _aidaHistoMap.insert( make_pair( _chi2YLocalname, chi2YLocal ) );
    } else {
      streamlog_out ( ERROR2 ) << "Problem booking the " << (_chi2YLocalname) << endl;
      streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
      _histogramSwitch = false;
    }

    string tempHistoName;
    string histoTitleXResid;
    string histoTitleYResid;
    string histoTitleZResid;

    for( int iDetector = 0; iDetector < _nPlanes; iDetector++ ){

      // this is the sensorID corresponding to this plane
      int sensorID = _orderedSensorID.at( iDetector );

      tempHistoName     =  _residualXLocalname + "_d" + to_string( sensorID );
      histoTitleXResid  =  "XResidual_d" + to_string( sensorID ) ;

      AIDA::IHistogram1D *  tempXHisto =
        AIDAProcessor::histogramFactory(this)->createHistogram1D(tempHistoName,NBin, Min,Max);
      if ( tempXHisto ) {
        tempXHisto->setTitle(histoTitleXResid);
        _aidaHistoMap.insert( make_pair( tempHistoName, tempXHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }

      tempHistoName     =  _residualYLocalname + "_d" + to_string( sensorID );
      histoTitleYResid  =  "YResidual_d" + to_string( sensorID ) ;

      AIDA::IHistogram1D *  tempYHisto =
        AIDAProcessor::histogramFactory(this)->createHistogram1D(tempHistoName,NBin, Min,Max);
      if ( tempYHisto ) {
        tempYHisto->setTitle(histoTitleYResid);
        _aidaHistoMap.insert( make_pair( tempHistoName, tempYHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }

#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
      tempHistoName     =  _residualZLocalname + "_d" + to_string( sensorID );
      histoTitleZResid  =  "ZResidual_d" + to_string( sensorID ) ;

      AIDA::IHistogram1D *  tempZHisto =
        AIDAProcessor::histogramFactory(this)->createHistogram1D(tempHistoName,NBin, Min,Max);
      if ( tempZHisto ) {
        tempZHisto->setTitle(histoTitleZResid);
        _aidaHistoMap.insert( make_pair( tempHistoName, tempZHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }
 
#endif
    }

  } catch (lcio::Exception& e ) {


#ifdef EUTEL_INTERACTIVE
    streamlog_out ( ERROR2 ) << "No AIDAProcessor initialized. Type q to exit or c to continue without histogramming" << endl;
    string answer;
    while ( true ) {
      streamlog_out ( ERROR2 ) << "[q]/[c]" << endl;
      cin >> answer;
      transform( answer.begin(), answer.end(), answer.begin(), ::tolower );
      if ( answer == "q" ) {
        exit(-1);
      } else if ( answer == "c" )
        _histogramSwitch = false;
      break;
    }
#else
    streamlog_out( WARNING2 ) << "No AIDAProcessor initialized. Continue without histogramming" << endl;

#endif


  }
#endif

}

#endif // USE_GEAR

