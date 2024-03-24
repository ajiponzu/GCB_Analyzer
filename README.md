# GCB_Demonstration

## Summary

- This repository includes emulator of "GNSS-Clock-Beacon (GCB)".

- About the detail of system configuration and methodology, you could refer to the journal paper; **Design method for optical clock beacons that can accurately detect exposure time from captured images**.

## Usage

### 1. Please clone this repogitory.
```
$ git clone https://github.com/ajiponzu/GCB_Demonstration
```

### 2. You can execute three programs using below commands.

#### In "./analyzer" directory, you can launch the docker container of movie or image analyzation server.

```
$ cd analyzer
$ docker image build -t ubuntu:22.04 .
$ docker run -d --name gcb_analyzer -p 8080:8080 -it ubuntu:22.04
```

> This server in the port of 8080 analyzes a movie and obtains a JSON described the pattern of GCB lighting.

> ***[notice]** The Drogon frame work (https://github.com/drogonframework/drogon) is used in the element of server.*

#### In "./client" directory, you can launch the client side of GCB_Analyzer.
The following command executes the analysis in a batch.

```
$ cd client
$ make
```

>. /client/input/* contains three sets of sample files of analysis resources.

>The json files are the definition file of the analysis resource.
When client.py is executed with this definition file as an argument, it analyzes GCB images in mp4 files and detects the exposure time of each frame. The results are stored in . /client/result.

> This program sends a selected movie to the server in "./analyzer" and calculates the exposure time of every frame in a movie accurately.

> ./client/result/*.json are the results of the analysis.

> ./client/result/*.mp4 are movies of the analysis results.

#### In "./simulator" directory,
>The simulator is python code written in Jupyter Lab Notebook format.

>See simulator/readme_simulator.md for details.

