#! /usr/local/bin/python3
# -*- coding: utf-8 -*-
#
# client.py -- GCB Analyzer Client application (samle)
#
# Sat Nov 18 23:05:37 2023 JST
# Tue Jan 16 22:36:13 2024 JST>
#
import sys,os,json
import cv2
from datetime import datetime,timedelta

import gcb_client   # GCB image analzyer client
import gcb_parser   # GCB time stamp parser

#------------------------------------------------------------------------
#   Determin exposure time(center) and exposure duration accracy.
#------------------------------------------------------------------------
#
#input:
#    approxT := Appoximate exposure time estimated by frame offset from videoStartTime.
#    lastF := lastFrame info.
#           lastF['expT'] -- Exposure time of last frame.
#           lastF['sect'] -- Section info of last frame.
#    sect := Section info of current frame
#       sect[0]  := Exposure start time offset(ms) by GCB parser.
#       sect[1]  := Exposure duration(ms) by GCB parser.
#       sect[3]  := Estimated exposure time accuracy(ms)
#
#Output:
#   expT , dT
#         expT := Central estimated time of frame exposure time(datetime object)
#         dT     := Estimated exposure time accuracy. => [expT-dT/2, expT+dT/2]
#
def determinExposureTime(approxT,lastF,sect):
    lastT = lastF['expT']

    expTms = sect[0] + sect[1]/2
    expT = lastT.replace(microsecond=int(expTms*1000))
    if (expT - lastT).total_seconds() < 0:
        expT += timedelta(seconds=1.0)

    dT = (approxT - expT).total_seconds()
    #print(f'lastT:{lastT} , approxT:{approxT} ,dT:{dT} , expT:{expT} ,  expTms:{round(expTms,2)} , sect({sect[0]},{sect[1]},{sect[2]})')
    expT += timedelta(seconds=int(dT))
    
    acc = sect[2]/1000
    return expT, acc

#========================================================================
#  GCB video processor
#========================================================================
# Input:
#    meta := Input resource dictionary.
#       meta['mediaFile'] -- Input video file path
#       meta['eventName'] -- Representative name for this resource
#       meta['startT']   -- Approximinate recording start time of vide file.
#       meta['frameOfs'] -- Approximate offset time of each frame. (array)
#       meta['mediaEvent']['target'] -- Definition of GCB instances to be recognized.
#       meta['expDuration'] -- Exposure duration of each frame.(optional)
#         If omitted, videoProcessor estimates the exposure time from the GCB blinking pattern.
#
#       outputDir  := Directory path to store the results
#
# Output:
#       Function stores the results in the outputDir directory.
#             'outputDir/{sesourceID}.json' -- A timestamp for each frame of the analysis results.
#             'outputDir/{sesourceID}.mp4' -- A video file with a timestamp of the analysis results.
#       videoProcessor() itsself returns nothing.
#
def videoProcessor(meta, outputDir):

    #--------------------------------
    # Input resouce definitions
    #--------------------------------

    input_video_path = meta['mediaFile']   # Input video file
    resourceID = meta['eventName']
    try:
        expDuration = meta['expDuration']  # Exposure duration of each frame(sec)
    except KeyError:
        expDuration = 0                    # Exposure duration is not specified.
                                           # It will estimeted by  gcb_parser.preprocess()
    metaFps = meta['fps']                  # Video frame rate by EXIF.

    print(f'InputVideo={input_video_path}')
    print(f'resourceID={resourceID}')

    try:
        videoStartT = datetime.strptime(meta['startT'],'%Y/%m/%d %H:%M:%S %z')
    except ValueError:
        videoStartT = datetime.strptime(meta['startT'],'%Y/%m/%d %H:%M:%S ')
    # At this point, videoStartT is an approximation of the recording start time based on EXIF.
    # videoStartT will be updated to a more accurate value when the frame capture time is
    # accurately determined by GCB image recognition.
    videoStartT_Updated = False

    frameOfs    = meta['frameOfs']
    # frameOfs is an array of approximate offset time (in seconds) from videoStartT for each frame.

    #--------------------------------
    # Output file definitions
    #--------------------------------
    output_video_path   = f"{outputDir}/{resourceID}.mp4"
    output_json_path    = f"{outputDir}/{resourceID}.json"

    #-------------------------------------------------
    # Extract beacon information for image recognizer(GCB analyzer)
    #-------------------------------------------------
    target = meta['mediaEvent']['target']
    requestDic = { "device_key": [] }
    for key in target:
        requestDic['device_key'].append(key) # Beacon instance ID
        requestDic[key] = target[key]        # Beacon type and coordinates for the instance.
    request_json = json.dumps(requestDic);

    #--------------------------------  
    # Temporaly file definitions.
    #--------------------------------
    tmp_video_path = f"./temp/tmp_{resourceID}.mp4"   # Work file for output video.
    tmp_json_path  = f"./temp/tmp_{resourceID}.json"  # Recognision result from GDB analyzer server for debug.
    
    print(f'output_video_path = {output_video_path}') 
    print(f'output_json_path = {output_json_path}')
    print(f'tmp_video_path = {tmp_video_path}')
    print(f'tmp_json_path = {tmp_json_path}')

    #------------------------------------
    # Invoke request to GCB Analyzer API
    #------------------------------------

    print(f'\nAnalyzing video file => {input_video_path} .....')
    api_access_id, analyzerResult = gcb_client.request_analyze_video(input_video_path, request_json)
    with open(tmp_json_path, 'w') as fp:
        json.dump(analyzerResult,fp,indent=4)   # Save analyzer result.

    #--------------------------------------------------------------------
    # Pre-processing of GCB analyzer image recognition results for GCB parser.
    #--------------------------------------------------------------------
    #  The analyzerResult returned by gcb_analyzer() contains the recognition result of
    #  the beacon image.
    #  The following function converts analyzerResult into a format suitable for
    #  gcb_parser to estimate the frame shooting time.
    #
    #  At the same time, it also provides a function to analyze the image recognition
    #  results over multiple frames to estimate the exposure duration.
    #
    # If the exposure duration(expDuration) is 0, the estimated value is calculated
    # in gcb_parser.preprocess(). metaFps is the video frame rate, which is referenced
    # as an upper limit when estimating expDuration.
    #
    # If expDuration is not explicitly specified in a higher-level function, it is 0.
    # If the exposure duration is known accurately in advance, it can be specified
    # in meta['expDuration'].
    #
    debug = True
    parserInput = gcb_parser.preprocess(analyzerResult,expDuration,metaFps,debug)
    expDuration = parserInput['dTexp']
    print(f'---- Exposure duration is {int(expDuration * 10000)/10}ms and  frame rate is {metaFps}')

    with open('temp/parserInput.json','w') as fp:
        json.dump(parserInput,fp,indent=4)

    #--------------------------------
    # Create result video
    #--------------------------------
    print(f'\nCreating result video ....')
    video_binary = gcb_client.request_visualize_analyzation_result(api_access_id,os.path.basename(input_video_path))

    if video_binary is None:
        print("error: request visualize_result")
        quit()

    with open(tmp_video_path, 'wb') as fp:
        fp.write(video_binary)
    visualization_video = cv2.VideoCapture(tmp_video_path)

    v_wid = int(visualization_video.get(cv2.CAP_PROP_FRAME_WIDTH))
    v_high = int(visualization_video.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = visualization_video.get(cv2.CAP_PROP_FPS)
    fmt = cv2.VideoWriter_fourcc('m', 'p', '4', 'v')
    video_writer = cv2.VideoWriter(output_video_path, fmt, fps, (v_wid, v_high))
    if not video_writer.isOpened():
        print(f"Can't open {output_video_path}!")
        quit()

    #------------------------------------------------------------------------
    #  Process for  all frames.
    #------------------------------------------------------------------------
    parserResult = []
    STAT = parserInput['STAT']
    lastFRAME = {}
    for frameNum,frameDic in enumerate(parserInput['frame']):
        #----------------------------------------------------------------
        # Read temprary video frame
        #
        ret, frame_img = visualization_video.read()
        if ret is not True:
            #print("frame is empty", file=sys.stderr)
            break
        cv2.rectangle(frame_img, [0, 0, 1500, 50], [255, 255, 255], -1)
        #----------------------------------------------------------------
        frameResult = {}
        frameResult['Frame'] = frameNum
        frameResult['GCB'] = []

        device_count = 0
        for instanceID,instanceJSON in frameDic.items():
            beaconType = instanceJSON["beaconType"]

            try:
                lastF = lastFRAME[instanceID]
            except KeyError:
                lastFRAME[instanceID] = {}
                lastF = lastFRAME[instanceID]
                lastF['expT']     = videoStartT + timedelta(seconds=frameOfs[frameNum])
                lastF['frameOfs'] = frameOfs[frameNum]

            if beaconType == 'M-Beacon':       # M-Beacon is not supported.
                continue

            bidResult = instanceJSON['beacon']      # <== Recognized LED lighting pattern.
            parseResult = gcb_parser.parseGCB(beaconType, bidResult, expDuration,STAT[instanceID]) # Parse it!

            result = {}
            result['type'] = beaconType
            result['devId'] = instanceID
            result['result'] = parseResult

            if parseResult:  # Parse success!
                #
                # Determin frame exposure time  and accracy.
                #
                sect = parseResult['time'][1] # Detected section: 0:Offset Time(ms) 1:width(ms) 2:accracry(ms)
                frOfs = frameOfs[frameNum]
                approxT = videoStartT + timedelta(seconds=frOfs)
                expT , acc = determinExposureTime(approxT,lastF,sect)

                lastF['expT'] = expT
                lastF['sect'] = sect
                lastF['frameOfs'] = frOfs
                
                #--------------------------------------------------------------------------------
                #
                expT_text = expT.strftime('%Y-%m-%dT%H:%M%:%S.{:05d}').format(int((expT.timestamp() % 1) * 100000))
                display_text = "{} acc={:.2f}".format(expT_text, acc*1000)

                print('{:3d} {:s} {:s}'.format(frameNum,f'{beaconType} {display_text}',parseResult['clid']))

                result['expT'] = expT_text
                result['dT'] = acc
                result['expDur'] = expDuration

                result['clid'] = parseResult['clid']
                try:
                    result['cmid'] = parseResult['cmid']
                except KeyError:
                    pass

            else:
                print(f"Frame#{frameNum} {beaconType} fail!!")
                display_text = ""
                result['expT'] = False
                result['dT'] = False

            result['beacon'] = bidResult
                
            frameResult['GCB'].append(result)

            #
            #  Write parsed timestamp text on output video frame.
            #
            if display_text != "":
                cv2.putText(frame_img,
                            text=display_text,
                            org=(10 + device_count * 750, 40),
                            fontFace=cv2.FONT_HERSHEY_SIMPLEX,
                            fontScale=1.0,
                            color=(0, 0, 0),
                            thickness=2,
                            lineType=cv2.LINE_4)

            device_count += 1

        print()
        parserResult.append(frameResult)    # Save frame result
        video_writer.write(frame_img)       # Write time-stamped video frame.

    #--------------------------------
    # Output parser results.
    #--------------------------------
    #
    with open(output_json_path,'w') as js:
        json.dump(parserResult,js,indent=4)     # Parser result by JSON
    #
    videoSize = os.path.getsize(output_video_path)
    jsonSize = os.path.getsize(output_json_path)
    print(f"Wrote to {output_video_path}({videoSize} bytes) , {output_json_path}({jsonSize} bytes)")
    #--------------------------------


if __name__ == "__main__":
    import argparse,sys,glob
    resultDir = './result'

    myname = os.path.basename(sys.argv[0])

    parser = argparse.ArgumentParser(usage=f"usage: python {myname} [-h] JSONfiles")
    parser.add_argument('JSON',nargs='+',default=['*'])
    args = parser.parse_args()

    flist = []
    for f in args.JSON:
        flist.extend(glob.glob(f))


    for file in flist:
        try:
            with open(file) as f:
                meta = json.load(f)
                #print(f'{file} = ',json.dumps(meta,indent=4))
        except:
            print(f'{file} open error.  ignored ..')
            continue

        #try:
        #    expDuration = meta['expDuration']
        #except KeyError:
        #    expDuration = 1 / meta['fps'] * 0.5  # Estimated exposure time (tentative)
        #    meta['expDuration'] = expDuration
            
        videoProcessor(meta,resultDir)

    print(".....done..",datetime.now(),'\n\n')
