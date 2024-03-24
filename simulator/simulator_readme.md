# simulator_readme.md
GCB_simulator.pynb is a notebook file created in JupyterLab (v3.3.2).

The following functions are implemented and will work if executed in order from the top of the file.

## Simulator of signal waveforms
* pulseFunc()

Define a signal to drive a light sources.

* showSignalStack()
* showMatrixSignalStack()
* showBeaconS_SignalStack()
* showBeaconNS_SignalStack()

Graph the signal waveforms 

Save the results in 'figs/signals/*.html'.

## Exposure simulators 
 * buildExposureFigure()
 * showExposureSimulation()
 
Graphs the exposure simulation and saves the result in 'figs/exposure/*.html'.

## Beacon simulators 
 * L_BeaconSimulator()
 * CL_BeaconSimulator()
 * M_BeaconSimulator()
 * CM_BeaconSimulator()
 
Graph exposure simulations by Beacon type and save the results in 'figs/simulate/*.html'.

## Time detection accuracy analysis
* analyzeBeacon()

Performs exposure simulation of OCB with the configuration specified in the parameters and writes the results to 'data/RESULT_*.json'.

By default, the analysis results of OCBs with four different encoding schemes (N=1000,1024, Binary code/Gray code) are written in one file.

The configuration of OCBs to be simulated is described in Beacon analyzers rev1 and Beacon analyzers rev2 Cells, so please execute the necessary one.

If expRange = quickExpRange, the simulation interval is sped up.
If expRange = defaultExpRange, one simulation may take several minutes to several tens of minutes.

## Graph the results of the accuracy analysis 
* drawAccGraphPair()

Call drawAccGraphPair() with a RESULT file to graph the results of a positive analysis.

* drawTimeAccuracyRatioGraph()

Graphs the accuracy ratio of two OCB configurations.

* drawDupGraphs()

Graphs the intervals where the same lighting pattern appears.

Both save the result to 'figs/accuracy/*.html'.

## Generate a dictionary file 
* buildDictionary()

Creates a dictionary of LED lighting patterns to be referenced by the image processing system.
The result is saved in 'dict/dict_*.json'.


