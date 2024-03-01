# -*- coding: utf-8 -*-
#
# builder.py -- Resource builder script
#
# Tue Nov 28 00:42:15 2023 JST
import cv2
import json
import os
from datetime import datetime,timedelta
import subprocess

#------------------------------------------------------------------------
#  Extract exif dictionary from video file
#------------------------------------------------------------------------
#
#  Exiftool command must be installed.
#  cf. https://exiftool.org/
#
# This script is developed using exiftool 12.70 on MacOS 14.1.1.
#

def getMetadata(file_path):
    try:
        # Get metadata in JSON format using exiftool
        result = subprocess.run(['exiftool', '-json', file_path], capture_output=True, text=True, check=True)

        metadata = json.loads(result.stdout)

        return metadata[0]  # MOV files usually contain only one piece of metadata.

    except subprocess.CalledProcessError as e:
        print(f"Error: {e}")
        return None


#------------------------------------------------------------------------
# mediaEvent extractor
#------------------------------------------------------------------------
#
#
#
def extractMediaEvent(inputVideo,fps,startT,mediaEvent,outputDir,mediaID):
    # 入力動画ファイルを開く
    inputVideoCapture = cv2.VideoCapture(inputVideo)

    # 入力動画の読み込みに失敗した場合は False を返す
    if not inputVideoCapture.isOpened():
        print(f"Error: Failed to open input video '{inputVideo}'.")
        return False

    # 動画のプロパティを取得
    #fps = inputVideoCapture.get(cv2.CAP_PROP_FPS)
    vWid = int(inputVideoCapture.get(cv2.CAP_PROP_FRAME_WIDTH))
    vHigh = int(inputVideoCapture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    total_frames = int(inputVideoCapture.get(cv2.CAP_PROP_FRAME_COUNT))    
    print(f'fps={fps}  total_frames={total_frames}')

    currFrame = 0
    for event in mediaEvent:
        section = mediaEvent[event]['section']
        target = mediaEvent[event]['target']
        
        fromSec = section[0]; dSec = section[1]
        eventName = f'{mediaID}_{event}'

        
        eventT = startT + timedelta(seconds=fromSec)
        
        _BaseName = '{:s}/{:s}_{:s}'.format(outputDir,eventName,eventT.strftime("%y%m%d_%H%M%S"))
        mediaFile = f'{_BaseName}.mp4'
        metaFile = f'{_BaseName}.json'
        frameFile = f'{_BaseName}.jpg'
        startFrame = int(fromSec * fps)
        endFrame = int((fromSec + dSec) * fps)
        
        print(f'mediaFile={mediaFile}. fromSec={fromSec}, dSec={dSec}')

        # VideoWriter オブジェクトを作成
        fmt = cv2.VideoWriter_fourcc(*'mp4v')
        videoWriter = cv2.VideoWriter(mediaFile, fmt, int(fps), (vWid, vHigh))
        
        mediaMeta = {}
        mediaMeta['mediaFile'] = mediaFile
        mediaMeta['inputFrom'] = inputVideo
        mediaMeta['fps'] = fps
        mediaMeta['eventName'] = eventName
        mediaMeta['mediaEvent'] = mediaEvent[event]
        mediaMeta['startT'] = eventT.strftime('%Y/%m/%d %H:%M:%S %z')
        mediaMeta['frameOfs'] = []

        topFrame = True
        while currFrame < endFrame:
            ret, frame = inputVideoCapture.read()
            if not ret:
                break

            currFrame = int(inputVideoCapture.get(cv2.CAP_PROP_POS_FRAMES))

            if currFrame < startFrame:
                continue

            #--------------------------------
            # Draw recognition frames
            #--------------------------------
            for devName,val in mediaMeta['mediaEvent']['target'].items():
                x = val['rect_x'] ; y = val['rect_y']
                w = val['rect_width'] ; h = val['rect_height']
                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 0, 255), thickness=2)

            videoWriter.write(frame)  # Copy frame to media file.
            
            if topFrame:
                cv2.imwrite(frameFile,frame)    # Save 1st frame to JPG file.
                topFrame = False

            ofsTime = (currFrame - startFrame) / fps
            mediaMeta['frameOfs'].append(int(ofsTime * 100000)/100000) # Frame offset (sec)

            if currFrame >= endFrame - 1:
                break
        
        videoWriter.release()  # Close ouput media.

        with open(metaFile,'w') as mf:
            json.dump(mediaMeta,mf,indent=4)  # Save media meta data.
        
    inputVideoCapture.release()
    
    return True

#------------------------------------------------------------------------
# Video File extractor
#------------------------------------------------------------------------
#
# Specify an input video file and a section in the video file with procdef
# and output a video file (mp4) with the section cut out and metadata (json)
# necessary for analysis.
# The "section in the video file" is called mediaEvent.
# The mediaEvent is specified as an offset (in seconds) from the beginning
# of the input file and the length of the section (in seconds).

def extractVideoFile(procdef,inputPath=False):
    inputVideo = procdef['inputFrom']
    if inputPath:
        inputVideo = inputPath + '/' + inputVideo
        
    outputDir  = procdef['outputDir']
    mediaID = procdef['mediaID']
    
    meta = getMetadata(inputVideo)
    if not meta: return False
    #print(json.dumps(meta,indent=4))

    inputFile = os.path.basename(meta['SourceFile'])
    fps = meta['VideoFrameRate']
    try:
        startTime = meta['CreationDate']        # For iPhone movie file.
    except KeyError:
        startTime = meta['CreateDate']

    try:
        startT = datetime.strptime(startTime, '%Y:%m:%d %H:%M:%S%z')    # Timezone 
    except ValueError:
        startT = datetime.strptime(startTime, '%Y:%m:%d %H:%M:%S')      # No timezone
        
    duration = meta['Duration']
    if duration[-1] == 's':
        dT = float(duration[:-1])
        dsec = int(dT); dmili = int((dT - dsec) * 1000)
        totalT = timedelta(seconds=dsec,milliseconds=dmili)
    else:
        totalT = datetime.strptime(duration,'%H:%M:%S')-datetime(1900,1,1,0,0,0)
    
    print(f'mediaID="{mediaID}", startTime={startT} , totalTime={totalT}({totalT.seconds}sec), fps={fps} , outputDir={outputDir}')
    os.makedirs(outputDir,exist_ok=True)
    
    return extractMediaEvent(inputVideo,fps,startT,procdef['mediaEvent'],outputDir,mediaID)

#========================================================================
# Sample main
#========================================================================

if __name__ == "__main__":
    import argparse,sys,glob

    parser = argparse.ArgumentParser(usage="usage: python resource_builder.py [-h] JSONfiles")
    parser.add_argument('-i','--inputPath',help="Specify common inputFrom path.")
    parser.add_argument('JSON',nargs='+',default=['*'])
    args = parser.parse_args()

    flist = []
    for f in args.JSON:
        flist.extend(glob.glob(f))

    for file in flist:
        with open(file) as f:
            procdef = json.load(f)

            if type(procdef) is list:
                for i,pdef in enumerate(procdef):
                    print(f'{file}[{i}] = ',json.dumps(pdef,indent=4))
                    extractVideoFile(pdef,inputPath=args.inputPath)
            else:
                print(f'{file} = ',json.dumps(procdef,indent=4))
                extractVideoFile(procdef,inputPath=args.inputPath)
                
                    
