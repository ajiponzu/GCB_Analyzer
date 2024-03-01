# -*- coding: utf-8 -*-
#
# GCB_parser.py -- GCB LED pattern parser.
#
# Mon Oct  9 02:45:17 2023 JST v1.0
# Tue Oct 10 22:10:01 2023 JST v1.1  validateCLID()
# Tue Oct 31 01:46:45 2023 JST v1.20  Support for CL-Beacon analysis.
# Sun Nov  5 00:57:01 2023 JST v1.30  analyzeCL(): Algorithmic CLI analysis; exposure time must be specified in parseGCB().
# Fri Dec  1 00:13:51 2023 JST v1.40  preprocess() and exposure duration estimation , are implemented.

import sys
import os
import json
import re
import math
ver = 'v1.40'
myname = os.path.basename(sys.argv[0])

# ------------------------------------------------------------------------
# Utilities
# ------------------------------------------------------------------------


def Error(*msg):
    print('{:s} ERROR: {:s}'.format(myname, ' '.join(msg)), file=sys.stderr)
    sys.exit(1)


def PrStderr(*msg):
    print(' '.join(msg), file=sys.stderr)


# ------------------------------------------------------------------------
#  Load dictionary files.
# ------------------------------------------------------------------------

DICT_list = {
    "CL-Beacon" : "dict/dict_CL_Beacon_B1000.json",
    "CM-Beacon" : "dict/dict_CM_Beacon_B1000.json"
}

DB = {}

for beacon,dict_file in DICT_list.items():
    DB[beacon] = {}
    try:
        with open(dict_file) as jf:
            DB[beacon]['dict'] = json.load(jf)
    except:
        Error(f"{beacon} dictionay not found ==> '{file}'")


# ---------------------------------------------------------------------------------
# parser/beacon_device_definition.json の IDを Simulatorに合わせたIDに変換するテーブル
# ---------------------------------------------------------------------------------
#
ID2BID = {}
ID2BID['CM-Beacon'] = {
    'ID1': 'B8',     'ID2': 'B9',     'ID3': 'nB9',    'ID4': 'nB8',
    'ID5': 'B09',    'ID6': 'B19',    'ID7': 'B29',    'ID8': 'B39',  'ID9': 'B49',    'ID10': 'B59',    'ID11': 'B69',    'ID12': 'B79',
    'ID13': 'nB79',   'ID14': 'nB69',   'ID15': 'nB59',   'ID16': 'nB49', 'ID17': 'nB39',   'ID18': 'nB29',   'ID19': 'nB19',   'ID20': 'nB09',
    'ID21': 'B08',    'ID22': 'B18',    'ID23': 'B28',    'ID24': 'B38',  'ID25': 'B48',    'ID26': 'B58',    'ID27': 'B68',    'ID28': 'B78',
    'ID29': 'nB78',   'ID30': 'nB68',   'ID31': 'nB58',   'ID32': 'nB48', 'ID33': 'nB38',   'ID34': 'nB28',   'ID35': 'nB18',   'ID36': 'nB08',
    'ID37': 'B07',    'ID38': 'B17',    'ID39': 'B27',    'ID40': 'B37',  'ID41': 'B47',    'ID42': 'B57',    'ID43': 'B67',    'ID44': 'B7',
    'ID45': 'nB7',    'ID46': 'nB67',   'ID47': 'nB57',   'ID48': 'nB47', 'ID49': 'nB37',   'ID50': 'nB27',   'ID51': 'nB17',   'ID52': 'nB07',
    'ID53': 'B06',    'ID54': 'B16',    'ID55': 'B26',    'ID56': 'B36',  'ID57': 'B46',    'ID58': 'B56',    'ID59': 'B6',     'ID60': 'B76',
    'ID61': 'nB76',   'ID62': 'nB6',    'ID63': 'nB56',   'ID64': 'nB46', 'ID65': 'nB36',   'ID66': 'nB26',   'ID67': 'nB16',   'ID68': 'nB06',
    'ID69': 'B05',    'ID70': 'B15',    'ID71': 'B25',    'ID72': 'B35',  'ID73': 'B45',    'ID74': 'B5',     'ID75': 'B65',    'ID76': 'B75',
    'ID77': 'nB75',   'ID78': 'nB65',   'ID79': 'nB5',    'ID80': 'nB45', 'ID81': 'nB35',   'ID82': 'nB25',   'ID83': 'nB15',   'ID84': 'nB05',
    'ID85': 'B04',    'ID86': 'B14',    'ID87': 'B24',    'ID88': 'B34',  'ID89': 'B4',     'ID90': 'B54',    'ID91': 'B64',    'ID92': 'B74',
    'ID93': 'nB74',   'ID94': 'nB64',   'ID95': 'nB54',   'ID96': 'nB4',  'ID97': 'nB34',   'ID98': 'nB24',   'ID99': 'nB14',   'ID100': 'nB04',
    'ID101': 'B03',    'ID102': 'B13',    'ID103': 'B23',    'ID104': 'B3',   'ID105': 'B43',    'ID106': 'B53',    'ID107': 'B63',    'ID108': 'B73',
    'ID109': 'nB73',   'ID110': 'nB63',   'ID111': 'nB53',   'ID112': 'nB43', 'ID113': 'nB3',    'ID114': 'nB23',   'ID115': 'nB13',   'ID116': 'nB03',
    'ID117': 'B02',    'ID118': 'B12',    'ID119': 'B2',     'ID120': 'B32',  'ID121': 'B42',    'ID122': 'B52',    'ID123': 'B62',    'ID124': 'B72',
    'ID125': 'nB72',   'ID126': 'nB62',   'ID127': 'nB52',   'ID128': 'nB42', 'ID129': 'nB32',   'ID130': 'nB2',    'ID131': 'nB12',   'ID132': 'nB02',
    'ID133': 'B0',     'ID134': 'B1',
    'ID135': 'PPS',    'ID136': 'nPPS',
    'ID137': 'nB1',    'ID138': 'nB0'
}
ID2BID['CL-Beacon'] = {
    'ID1': 'nB9',    'ID2': 'nB8',    'ID3': 'nB7',  'ID4': 'nB6',  'ID5': 'nB5',
    'ID6': 'nB4',    'ID7': 'nB3',    'ID8': 'nB2',  'ID9': 'nB1', 'ID10': 'nB0',
    'ID11': 'B9',     'ID12': 'B8',    'ID13': 'B7',  'ID14': 'B6',  'ID15': 'B5',
    'ID16': 'B4',     'ID17': 'B3',    'ID18': 'B2',  'ID19': 'B1',  'ID20': 'B0',
    'ID21': 'nPPS',    'ID22': 'PPS'
}

#
#
BID2ID = {}
for beacon in DB:
    DB[beacon]['id2bid'] = ID2BID[beacon]   # ID -> BID dictionary.
    DB[beacon]['bid2id'] = {}               # BID -> ID dictionary.
    for id, bid in ID2BID[beacon].items():
        DB[beacon]['bid2id'][bid] = id

# ------------------------------------------------------------------------
# CM_Beacon IDString definition by BID
# ------------------------------------------------------------------------
#
CMindex = ['PPS']
CLindex = ['PPS']
for col in range(9, -1, -1):
    CLindex.append('B{}'.format(col))
    for row in range(col, -1, -1):
        if col == row:
            id = 'B{}'.format(col)
        else:
            id = 'B{}{}'.format(row, col)
        # print(id,end=' ')
        CMindex.append(id)
    # print()
DB['CL-Beacon']['index'] = CLindex
DB['CM-Beacon']['index'] = CMindex
    

#------------------------------------------------------------------------
# BeaconID converter
#------------------------------------------------------------------------
#
# Convert LED IDs ('IDxx') written in GCB analyzer's recognition result
# into BIDs ('Bxx' ,'nBxx') which GCB Parser can recognize.

def convertID2BID(beaconType,idResult):
    try:
        GCBindex = DB[beaconType]['index']
        bid2id   = DB[beaconType]['bid2id']
    except KeyError:
        return False

    bidResult = {}
    for idx in GCBindex:
        try:
            id0 = bid2id[idx]       # 'IDxx' -> 'Byy'
            bidResult[idx]  = idResult[id0]
        except KeyError:
            pass

        try:
            idx1 = 'n' + idx        # Make Complemental LED ID. 'nIDxx'
            id1 = bid2id[idx1]      # 'nIDxx' -> 'nByy'
            bidResult[idx1] = idResult[id1]
        except KeyError:
            pass

    return bidResult

#------------------------------------------------------------------------
#  Format converter for GCB Analyzer image recognition results
#------------------------------------------------------------------------

def convertAnalyzerResult(analyzerResult):
    frame_num = analyzerResult['frame_num']
    result = {}
    result['frame'] = [ False ] * frame_num
    
    for frameID,frameDic in analyzerResult.items():
        if frameID == 'frame_num':
            continue
        frameNum = int(frameID[5:])

        fdic = {}
        for dev,data in frameDic.items():
            if dev == 'device_keys': continue

            beaconType = data['device_name']
            fdic[dev] = {}
            fdic[dev]['beaconType'] = beaconType
            fdic[dev]['beacon']     = convertID2BID(beaconType,data['beacon'])
            fdic[dev]['position']   = data['position']

        result['frame'][frameNum] = fdic

    return result

#------------------------------------------------------------------------
#  Luminance Stat Aggregator : pass1,2
#------------------------------------------------------------------------
#
#
def aggregateLuminanceStat(parserInput,lumRange = 32):
    #
    #  Pass 1 :: Histogram of LED luminance recognition results aggregated for all LEDs.
    #
    STAT = {}
    for frame in parserInput['frame']:
        for instance,beacon in frame.items():
            if not beacon['beacon']:
                continue # Image recognition was failed. ignore.
            beaconType = beacon['beaconType']
            try:
                STAT[instance]
            except:
                STAT[instance] = {}
                for id in DB[beaconType]['index']:
                    for ID in [id, 'n'+id]:
                        STAT[instance][ID] = {}
                        STAT[instance][ID]['tile'] = {}
                        STAT[instance][ID]['hist'] = [0] * lumRange  
                    STAT[instance]['all'] = {}
                    STAT[instance]['all']['tile'] = {}
                    STAT[instance]['all']['hist'] = [0] * lumRange

            for id in DB[beaconType]['index']:
                for ID in [id , 'n'+id]:
                    try:
                        val = beacon['beacon'][ID]
                        STAT[instance][ID]['hist'][val] += 1    # Count frequency per luminance for individual LED.
                        if id != 'PPS':
                            STAT[instance]['all']['hist'][val] += 1 # Count it for all LED's
                    except KeyError:
                        #print(f'{ID}',end=' ')
                        pass
    #
    # Pass 2:: Statistics are calculated from luminance histograms for each LED.
    #
    for instance in STAT:
        for ID,S in STAT[instance].items():

            hist = S['hist']  # Luminance histgram array.
            ttl = sum(hist)
            if ttl:
                sumH=0
                tile0 = -1
                tile90 = -1
                tile99 = -1
                for idx,val in enumerate(hist):
                    if tile0 < 0 and val:
                        tile0 = idx
                    sumH += val
                    if tile90 < 0 and sumH/ttl > 0.9:
                        tile90 = idx  # Set 90% tile.
                    if tile99 < 0 and sumH/ttl > 0.99:
                        tile99 = idx # Set 99% tile

                th = int((tile90 - tile0) * 0.5) + tile0
                S['tile'] = { 'ttl':ttl, 'th': th ,'tile0':tile0 , 'tile90':tile90 , 'tile99': tile99 }
            else:
                S['tile'] = { 'ttl':0 }
    return STAT

#
# Pass 3:: Determines which LEDs are always on based on all LED statistics
#

def aggregateLuminanceStat_pass3(STAT,expDuration, verbose = False):

    try:
        excBit = int(math.log10(expDuration*1000)/math.log10(2)) + 2
        if excBit < 0:
            excBit = 0
        excID = "0123456789"[excBit:]
    except ValueError as e:
        excID = "789"
    print(f'expDur={expDuration*1000}  excID={excID}')
    
    for instance in STAT:
        if verbose: print(f'----<{instance}>------')
            
        tileA = STAT[instance]['all']['tile']
        tileAmin = tileA['tile0']
        tleAth   = tileA['th']
        tileAmax = tileA['tile99']
        dTileA = tileAmax - tileAmin
        
        for ID,S in STAT[instance].items():
            if ID == 'all':
                continue
            if verbose: print(f'{ID} := ',end='')

            tile = S['tile']
            if not tile or tile['ttl'] == 0:
                continue

            #------------------------------------------------
            # Determination logic of always-on LEDs
            #
            #  LEDs containing excID should not be always on.
            if not (ID[-1] in excID or ID[-2] in excID):
                tileMin = tile['tile0'];
                tileMax = tile['tile99']
                dTile = tileMax - tileMin

                if tileMin > tileAmin + dTileA/4 or dTile < dTileA * 0.6: # LED always-on?
                    #tile['tile0'] = tileAmin  # Replace 0%-tile to All-LED-0%-tile.
                    tile['th'] = tileMin      # Set threshold to  0%tile value.
                    tile['on'] = True

            #--------------------------------------

            if not verbose:
                continue

            print(f'=======<{instance}({ID})>=============================================')
            print(f'\t{S["tile"]}\n')

            ttl = tile['ttl']
            sumH = 0
            for idx,val in enumerate(S['hist']):
                if idx == tile['tile0']:
                    print('\t---<0%>-----')
                if idx == tile['th']:
                    print('\t---<THRESHOLD>-----')

                sumH += val
                v = int(val / ttl * 60)
                h = int(sumH / ttl * 60)
                print('\t{:02d} [{:3d}/{:3d}] :'.format(idx,val,sumH),end='')
                for i in range(0,h+1):
                    if i == h:
                        print('+',end='')
                    elif i == v:
                        print('*',end='')
                    else:
                        print(' ',end='')
                print()

                if idx == tile['tile90']:
                    print('\t---<90%>-----')
                if idx == tile['tile99']:
                    print('\t---<99%>-----')

    return STAT


#------------------------------------------------------------------------
# Estimate exposure duration
#------------------------------------------------------------------------
#
# Function to take B0-B9,PPS and nB0-nB9,nPPS from Beacon recognition results,
# analyze the blinking state of Complemental LED Pair, and estimate exposure duraion.
#
# Function retuns estimated exposure duration(sec).
# Function returns 0 , if estimation failed.
#
def estimateExposureDuration(parserInput, verbose = False):
    BID = [ 'B0' , 'B1' , 'B2' , 'B3' , 'B4' , 'B5' , 'B6', 'B7' ,'B8','B9','PPS' ]

    stat = {}
    for frame in parserInput['frame']:
        for instance,beacon in frame.items():
            if not beacon['beacon']:
                continue # Image recognition was failed. ignore.
            beaconType = beacon['beaconType']
            try:
                stat[beaconType]
            except:
                stat[beaconType] = {}
                for id in BID:
                    stat[beaconType][id] = [ [0] * 32 , [0] * 32 , [0] * 32]

            for id in BID:
                val0 = beacon['beacon'][id]
                val1 = beacon['beacon']['n'+id]
                stat[beaconType][id][0][val0] += 1          # Count positive logic LED
                stat[beaconType][id][1][val1] += 1          # Count negative logic LED
                stat[beaconType][id][2][abs(val0-val1)] += 1 # Count difference.

    EED = {} # Initialize EED(Estimated Exposure Duration) dictionary.
    
    for beaconType in stat:
        if verbose: print(f'----<{beaconType}>------')
        EED[beaconType] = []
            
        for p,id in enumerate(BID):
            if verbose: print(f'{id} := ',end='')
            stat0 = stat[beaconType][id][0]
            stat1 = stat[beaconType][id][1]
            ttl0 = 0
            ttl1 = 0
            for val in range(0,32):
                if verbose: print('{:2d}:{:-2d}'.format(stat0[val],stat1[val]),end=' ')
                ttl0 += stat0[val]
                ttl1 += stat1[val]
            if verbose: print('  => {:02d}:{:-02d}'.format(ttl0,ttl1))

            
            stat3 = stat[beaconType][id][2]
            sum=0
            tile90 = 0
            tile99 = 0
            for val in range(0,32):
                n = stat3[val]
                sum += n
                if tile90 == 0 and sum/ttl0 > 0.9:
                    tile90 = val  # Set 90% tile.
                if tile99 == 0 and sum/ttl0 > 0.99:
                    tile99 = val # Set 99% tile
                

                if verbose:
                    graph = [''] * 101
                    r = int((n / ttl0) * 100) + 1
                    for i in range(0,101):
                        if i < r: graph[i] = '*'
                        else:     graph[i] = ' '

                        if i == int((sum / ttl0) * 100):
                            graph[i] = 'X'
                    print('\t{:2d}[{:2d}/{:3d},{:4.1f}%] '.format(val, n,sum,(sum/ttl0) * 100),''.join(graph))
            if verbose: print()

            if tile99 >= 16:
                # Calculate degrees up to 90th percentile threshold✕ 0.6
                sum = 0
                for val in range(0,int(tile90* 0.6)): sum += stat3[val]
                ratio = sum/ttl0  # This value is considered to be the ratio that both LED pairs were lit.
                expDur = (2 ** p) * ratio # Estimate ExposureDuration from this ratio.(msec)
                expDur = int((expDur * 100))/100000  # Convert to secondConverted to seconds with a resolution of 10 microseconds
            else:
                ratio = expDur = 0

            if verbose: print(f'90%tile={tile90} , 99%tile={tile99} , ratio={int(ratio * 1000)/10}% , expDur={expDur * 1000}ms')

            EED[beaconType].append(expDur)
    #
    #-------------------------------------------------------------------------------
    # Select the most reasonable value from the exposure duration estimated from each of
    # the LED (B0-B9) blinking patterns of each beacon.
    #
    # 1, Select the beacon with the fastest blinking LED that has been successfully
    #    estimated.
    # 2、Select 2 LEDs from the selected beacon that successfully estimated the exposure
    #    time from among the LEDs that blink fastest.
    # 3、The average value of the two selected estimations is used as the estimated
    #    exposure duration.
    #------------------------------------------------------------------------------

    minBit = 999; minExpDur = 999
    for beacon,ary in EED.items():

        for b in range(0,8):    # Search from B0(1ms) to B7(256ms)
            if ary[b] == 0:
                continue        # Estimation failed. ignore.
            if ary[b+1] != 0:
                dt  = (ary[b] + ary[b+1])/2 # Calculate the average with the next value
                if b < minBit:
                    minExpDur = dt
                elif b == minBit and minExpDur > dt:
                    minExpDur = dt
                break

    if minExpDur == 999: minExpDur = 0           # Estimation failed!!
        
    if verbose:
        print(json.dumps(EED,indent=4))
        print('estimated exposure duration is {:.2f}ms'.format(minExpDur * 1000))

    return  minExpDur

#========================================================================
def _scaleVal(val,tile):
    try:
        if tile['on']:
            return 1.0  # This LED is always ON
    except KeyError:
        pass

    #
    #  Scaling 0%tile as 0, th = 0.5 , 90%tile = 1.0
    #
    tlMin = tile['tile0']
    tlTH = tile['th']
    tlMax = tile['tile99']

    if val < tlTH:
        V = (val - tlMin)/(tlTH - tlMin) * 0.5
    elif val == tlTH:
        V = 0.5
    else:
        V = (val - tlTH)/(tlMax - tlTH) * 0.5

    if   V < 0:   V = 0.0
    elif V > 1.0: V = 1.0
    return V

#------------------------------------------------------------------------
# Build CLID/CMID
#------------------------------------------------------------------------
def _convertIDS(beaconType,led):
    IDS = ''
    for bid in DB[beaconType]['index']:
        try:
            val0 = led[bid]
            val1 = led['n'+bid]
            if val0:
                if val1:
                    ch = 'X'     # ON-ON
                else:
                    ch = '1'     # ON-OFF
            else:
                if val1:
                    ch = '0'     # OFF-ON
                else:
                    ch =  '-'    # OFF-OFF
        except KeyError:
            ch = '?'             # Unkown
        IDS = IDS + ch
    return IDS


def buildIDS(beaconType,result,insStat):
    led = {}
    for bid,val in result.items():
        tile = insStat[bid]['tile']
        th = tile['th']         # Get threshold value from statistic data.
        if val > th:
            led[bid] = True   # LED is ON
        else:
            led[bid] = False  # LED is OFF

    clid = _convertIDS('CL-Beacon',led)
    if beaconType == 'CM-Beacon':
        cmid = _convertIDS('CM-Beacon',led)
        return clid,cmid
    else:
        return clid

#------------------------------------------------
#　　Calculation of pseudo-distance between patterns
#-------------------------------------------------
#
dM = 4   # Matched
d1 = 2   # Diff 1
dR = -4  # Reverse match
dN = 0   # Unknown

# '-':off-off, '0':off-on, '1':on-off , 'X':on-on.  , '<': LED1 is brighter, '<':LED0 is brighter.
pdisTbl = {
    '-': { '-':dM , '0':d1 , '1':d1 , 'X':dR , '?':dN},
    '0': { '-':d1 , '0':dM , '1':dR , 'X':d1 , '?':dN},
    '1': { '-':d1 , '0':dR , '1':dM , 'X':d1 , '?':dN},
    'X': { '-':dR , '0':d1 , '1':d1 , 'X':dM , '?':dN},
    '?': { '-':dN , '0':dN , '1':dN , 'X':dN , '?':dN},
}

def getPatDist(pat0,pat1):
    if len(pat0) != len(pat1):
        return False
    pd = 0
    for i in range(0,len(pat0)):
        pd += pdisTbl[pat0[i]][pat1[i]]
        
    return round(pd/(len(pat0) * dM),3) # Normalize to 1.0

#--------------------------------------------------------
# Find the closest dictionary entry to dTexp
#--------------------------------------------------------
#
def searchExposureDuration(dTexp,Tdict):
    lastT = False
    for t in Tdict:
        T = float(t)
        if T > dTexp:
            if dTexp - lastT > T - dTexp:
                return T
            else:
                return lastT
        lastT = T
    return False
                
#------------------------------------------------------------------------
#  parseExposureTime :: Exposure time parser.
#------------------------------------------------------------------------
#
# Input
#   pat0 := Search pattern (LED lighting pattern string)
#   DICT := DB[beacon]['dict']
#   Trange[] = Specify a range of exposure times to search  in ms.
#              default 0ms <-> 1000ms
#
# Output
#   False <= Serch failed
#   [ratio , section[]]
#           ratio    := Pattern match ratio.
#                       1.0 indicates an exact match.  only 1 section will return.
#                       Partial match if less than 1.0
#           section[] := Detected section candidates.
#             sectiton[i] = [expT,duration,accuracy] 
#               expT     := Detected exposure start time(ms).
#               duration := Detected exposure duration(ms).
#               accuracy := Estimated time accuraty(ms)
#
def parseExposureTime(pat0,dTexp,DICT,Trange=[0, 1000]):
    dTexp *= 1000.
    dT = searchExposureDuration(dTexp,DICT['dTexp'])
    if not dT:
        return False
    #print(f'Selected ExposureDuration={dT}')

    Edict = DICT['dTexp'][str(dT)] # Get dictionary for the exposure duration.

    R = {}
    for t,ent in Edict.items():
        T = float(t)
        if T < Trange[0]:
            continue
        if T > Trange[1]:
            break
        
        pat1 = DICT['pat'][ent[0]]
        ratio = getPatDist(pat0,pat1)
        try:
            R[ratio].append(ent)
        except KeyError:
            R[ratio] = [ent]
            
    for ratio in sorted(R,reverse=True):
        #print(f'ratio={ratio} , R[ratio] = {R[ratio]}')
        return [ratio,R[ratio]]   # Returns retio of 1st entry and section candidates.
    
    
# --------------------------------------------------------
# CL-Beacon ID analyzer
# --------------------------------------------------------

def parseCL_Analytically(CL_pat, expTime):

    # print('analyzeCL("{}",{:.2f})'.format(CL_pat,expTime*1000))
    lastIdc = CL_pat[0]  # PPS LED

    pw = 0.512   # Start from 512ms pulse width.
    t0 = 0       # Origin time of current signal.
    for bid in range(1, 11):
        idc = CL_pat[bid]
        if idc == '0':         # LED is OFF
            fromT = t0
            toT = t0 + pw - expTime
        elif idc == '1':       # LED is ON
            fromT = t0 + pw
            toT = t0 + 2 * pw - expTime
            t0 += pw
        else:
            if lastIdc == 'X':   # Negative edge.
                fromT = t0 + 2 * pw - expTime
                toT = t0 + 2 * pw
            else:                # Positive edge
                fromT = t0 + pw - expTime
                toT = t0 + pw
            break
        lastIdc = idc
        pw /= 2

    #if fromT > 1.0:
    #    print(f'CL_pat={CL_pat}, expTime={expTime}  fromT={fromT}  toT={toT}')
    dur = (toT - fromT) * 1000
    fromT = (fromT % 1) * 1000
    return [round(fromT,2), round(dur,2) , round(dur,2)]


# ========================================================================
# Pre-processor for GCB Parser  
# ========================================================================
#
# Pre-processing of GCB analyzer image recognition results for time identification with GCB parser
#
# Input
#       analyzerResult := Image recognition result by gcb_analyzer.
#       expDuration    := Exposure duration for each vide frame.(sec)
#                         It will estimated by estimateExposureDuration() , it expDuration == 0.
#       exifFps  := Vide0 frame rate that specified exif metadata.
#                   This value is used as upper limit of exposure duration estimation.
#
def preprocess(analyzerResult,expDuration,exifFps,debug=False):
    parserInput = convertAnalyzerResult(analyzerResult)   # Convert ID to BID and re-format dictioonary.
    STAT = aggregateLuminanceStat(parserInput)            # Aggrigate luminance statistics. (pass 1, pass 2)
    
    # Estimate exposure duration if its not specified.
    if expDuration == 0:
        expDuration = estimateExposureDuration(parserInput,verbose = False)

        if expDuration > 1/exifFps or expDuration < 0.0003:
            expDuration = 1 / exifFps

        expDuration = int(expDuration * 100000)/100000 # Round to 10us accracy.

    parserInput['STAT'] = aggregateLuminanceStat_pass3(STAT,expDuration,verbose=False)  # Aggrigate luminance statistics.
    parserInput['dTexp'] = expDuration
    return parserInput

# ========================================================================
# Generic entry for time recognition engine
# ========================================================================
# Input
#   beaconType := 'CL-Beacon' or 'CM-Beacon'
#   bidResult  := LED lighting pattern DB detected by analyzePicture()
#   dTexp     := Exposure time.  default = False: Auto ditect (not recommended).
# Output
#     parseResult['clid'] := CL-Beacon ID string. 
#     parseResult['cmid'] := CM-Beacon ID string. (only if beaconType == 'CM-Beacon')
#     parseResult['time'] := pattern match ratio and detected section candidates.
#           [ratio,section[]]
#                section[][0]  = Detected exposure start time (0..1.0)
#                section[][1]  = Detected exposure duration.
#                section[][2]  = Detected exposure time accuracy.
#
def parseGCB(beaconType, bidResult, dTexp, insStat):
    
    parseResult = {}
    parseResult['dTexp'] = dTexp

    R = False
    if beaconType == 'CL-Beacon':
        clid = buildIDS('CL-Beacon',bidResult,insStat)
        if not clid:
            return False
        parseResult['clid'] = clid
        parseResult['time'] = parseResult['time_acl'] = parseCL_Analytically(clid, dTexp)
        #print(f'\t\tACL={parseResult["time"]}')
        R = parseResult['time_cl'] = parseExposureTime(clid, dTexp,DB['CL-Beacon']['dict'])
        #print(f'\t\tclR={R}')
        #return parseResult
    elif beaconType == 'CM-Beacon':
        clid, cmid = buildIDS('CM-Beacon',bidResult,insStat)
        parseResult['clid'] = clid
        parseResult['cmid'] = cmid
        #print(f'\tCM-Beacon dTexp={round(dTexp* 1000,2)} {clid} {cmid}')

        if clid:
            R = parseResult['time_cl'] = parseExposureTime(clid, dTexp,DB['CL-Beacon']['dict'])
            #print(f'\t\tclR={R}')
        else:
            parseResult['time_cl'] = False
        if cmid:
            R = parseResult['time_cm'] = parseExposureTime(cmid, dTexp ,DB['CM-Beacon']['dict'])
            #print(f'\t\tcmR={R}')
        else:
            parseResult['time_cm'] = False
    else:
        return False
            
    if not R:
        return False

    parseResult['time'] = R[1][0]  # select 1st candidate section.
    return parseResult
    


