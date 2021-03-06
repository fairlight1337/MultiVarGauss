#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>
#include <fstream>

#include <mvg/KMeans.h>
#include <mvg/MixedGaussians.hpp>


bool fileExists(std::string strFilepath) {
  std::ifstream ifFile(strFilepath, std::ios::in);
  
  return ifFile.good();
}


std::string makeOutputFilename(std::string strFileIn) {
  size_t szSlash = strFileIn.find_last_of("/");
  if(szSlash == std::string::npos) {
    szSlash = -1;
  }
  
  size_t szDot = strFileIn.find_first_of(".", szSlash);
  if(szDot == std::string::npos) {
    szDot = strFileIn.length();
  }
  
  szSlash++;
  
  return strFileIn.substr(szSlash, szDot - szSlash) + ".out" + strFileIn.substr(szDot);
}


mvg::Dataset::Ptr loadCSV(std::string strFilepath, std::vector<unsigned int> vecUsedIndices = {}) {
  mvg::Dataset::Ptr dsData = nullptr;
  
  std::ifstream ifFile(strFilepath, std::ios::in);
  
  if(ifFile.good()) {
    dsData = mvg::Dataset::create();
    
    std::string strLine;
    std::getline(ifFile, strLine); // Header
    while(std::getline(ifFile, strLine)) {
      std::vector<std::string> vecTokens;
      
      char* cToken = std::strtok((char*)strLine.c_str(), ",");
      while(cToken != nullptr) {
	vecTokens.push_back(cToken);
	cToken = std::strtok(NULL, ",");
      }
      
      std::vector<double> vecData;
      for(unsigned int unI = 0; unI < vecTokens.size(); ++unI) {
	if(vecUsedIndices.size() == 0 || std::find(vecUsedIndices.begin(), vecUsedIndices.end(), unI) != vecUsedIndices.end()) {
	  std::string strToken = vecTokens[unI];
	  
	  try {
	    vecData.push_back(std::stod(strToken));
	  } catch(std::exception& seException) {
	    vecData.push_back(0.0);
	  }
	}
      }
      
      if(vecData.size() > 0) {
	Eigen::VectorXf vxdData(vecData.size());
	for(unsigned int unI = 0; unI < vecData.size(); ++unI) {
	  vxdData[unI] = vecData[unI];
	}
	
	dsData->add(vxdData);
      }
    }
  }
  
  return dsData;
}


int main(int argc, char** argv) {
  int nReturnvalue = EXIT_FAILURE;
  
  if(argc > 1) {
    std::string strFileIn = argv[1];
    std::string strFileOut;
    
    if(argc > 2) {
      strFileOut = argv[2];
    } else {
      strFileOut = makeOutputFilename(strFileIn);
    }
    
    if(fileExists(strFileIn)) {
      std::cout << "Cluster Analysis: '" << strFileIn << "' --> '" << strFileOut << "'" << std::endl;
      
      mvg::KMeans kmMeans;
      mvg::Dataset::Ptr dsData = loadCSV(strFileIn, {0, 1, 3});
      std::cout << "Dataset: " << dsData->count() << " samples with " << dsData->dimension() << " dimension" << (dsData->dimension() == 1 ? "" : "s") << std::endl;
      
      if(dsData) {
	kmMeans.setSource(dsData);
	std::cout << "Calculating KMeans clusters .. " << std::flush;
	
	if(kmMeans.calculate(1, 5)) {
	  std::cout << "done" << std::endl;
	  
	  std::vector<mvg::Dataset::Ptr> vecClusters = kmMeans.clusters();
	  std::cout << "Optimal cluster count: " << vecClusters.size() << std::endl;
	  
	  unsigned int unSumSamplesUsed = 0;
	  for(unsigned int unI = 0; unI < vecClusters.size(); ++unI) {
	    std::cout << " * Cluster #" << unI << ": " << vecClusters[unI]->count() << " sample" << (vecClusters[unI]->count() == 1 ? "" : "s") << std::endl;
	    unSumSamplesUsed += vecClusters[unI]->count();
	  }
	  
	  unsigned int unRemovedOutliers = dsData->count() - unSumSamplesUsed;
	  if(unRemovedOutliers > 0) {
	    std::cout << "Removed " << unRemovedOutliers << " outlier" << (unRemovedOutliers == 1 ? "" : "s") << std::endl;
	  }
	  
	  mvg::MixedGaussians<double> mgGaussians;
	  
	  for(mvg::Dataset::Ptr dsCluster : vecClusters) {
	    mvg::MultiVarGauss<double>::Ptr mvgGaussian = mvg::MultiVarGauss<double>::create();
	    mvgGaussian->setDataset(dsCluster);
	    
	    mgGaussians.addGaussian(mvgGaussian, 1.0);
	    //break;
	  }
	  
	  mvg::MultiVarGauss<double>::Rect rctBB = mgGaussians.boundingBox();
	  mvg::MultiVarGauss<double>::DensityFunction fncDensity = mgGaussians.densityFunction();
	  
	  std::cout << "Clusters bounding box: [" << rctBB.vecMin[0] << ", " << rctBB.vecMin[1] << "] --> [" << rctBB.vecMax[0] << ", " << rctBB.vecMax[1] << "]" << std::endl;
	  
	  // Two dimensional case
	  float fStepSizeX = 0.01;
	  float fStepSizeY = 0.01;
	  
	  std::cout << "Writing CSV file (step size = [" << fStepSizeX << ", " << fStepSizeY << "]) .. " << std::flush;
	  
	  std::ofstream ofFile(strFileOut, std::ios::out);
	  
	  for(float fX = rctBB.vecMin[0]; fX < rctBB.vecMax[0]; fX += fStepSizeX) {
	    for(float fY = rctBB.vecMin[1]; fY < rctBB.vecMax[1]; fY += fStepSizeY) {
	      float fValue = fncDensity({fX, fY, -100});
	      
	      ofFile << fX << ", " << fY << ", " << fValue << std::endl;
	    }
	  }
	  
	  ofFile.close();
	  
	  std::cout << "done" << std::endl;
	  
	  nReturnvalue = EXIT_SUCCESS;
	} else {
	  std::cout << "failed" << std::endl;
	}
      }
    } else {
      std::cerr << "Error: File not found ('" << strFileIn << "')" << std::endl;
    }
  } else {
    std::cerr << "Usage: " << argv[0] << " <data.csv> [<output.csv>]" << std::endl;
  }
  
  return nReturnvalue;
}
